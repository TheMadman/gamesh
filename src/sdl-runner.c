#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <srvsh.h>
#include <libadt.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>

#include "gamesh/gamesh.h"

typedef struct libadt_vector vec_t;

#define DEFAULT_HEIGHT 720
#define DEFAULT_WIDTH 1280

#define ARRLENGTH(arr) (sizeof(arr) / sizeof(arr[0]))

#define index libadt_vector_index

#define MAX libadt_util_max

typedef struct buffer_s buffer_t;

typedef struct buffer_s {
	int allocated;
	union {
		gamesh_shared_buffer_t *buffer;
		buffer_t *next_free;
	};
} buffer_t;

static SDL_Surface *get_buffer_surface(buffer_t *shared_buffer)
{
	return gamesh_get_shared_buffer_surface(shared_buffer->buffer);
}

static buffer_t *next_free_buffer(buffer_t *buffer)
{
	if (!buffer)
		return NULL;
	if (buffer->allocated)
		return NULL;
	if (!buffer->next_free)
		return NULL;
	if (buffer->next_free->allocated)
		return NULL;
	return buffer->next_free;
}

// I should probably add some inline functions like this in libadt..
static int append(vec_t *vec, void *data)
{
	vec_t attempt = libadt_vector_append(*vec, data);
	if (libadt_vector_identity(attempt, *vec))
		return -1;
	*vec = attempt;
	return 0;
}

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
const char *window_title = NULL;

int error_opcode = -1;
int event_fd = -1;
int gamesh_event_new_listener_event = -1;

vec_t keyboard_event_listeners = { .size = sizeof(int) };
vec_t mouse_event_listeners = { .size = sizeof(int) };
vec_t gamepad_event_listeners = { .size = sizeof(int) };
vec_t quit_event_listeners = { .size = sizeof(int) };
vec_t tick_event_listeners = { .size = sizeof(int) };
vec_t resize_event_listeners = { .size = sizeof(int) };

typedef struct surface_s surface_t;
typedef struct surface_s {
	int allocated;
	union {
		struct {
			SDL_Texture *texture;
			int active_buffer;
			int changed;
			int x;
			int y;
		};
		surface_t *next_free;
	};
} surface_t;

static surface_t *next_free_surface(surface_t *surface)
{
	if (!surface)
		return NULL;
	if (surface->allocated)
		return NULL;
	if (!surface->next_free)
		return NULL;
	if (surface->next_free->allocated)
		return NULL;
	return surface->next_free;
}

typedef struct {
	vec_t surfaces;
	vec_t buffers;
	void *surfaces_free;
	void *buffers_free;
} client_t;

vec_t clients = { .size = sizeof(client_t) };

vec_t render_surfaces = { .size = sizeof(vec_t) };

SDL_AppResult init_clients(void)
{
	int clis = cli_count();
	if (clis < 0)
		return SDL_APP_FAILURE;

	render_surfaces = libadt_vector_init(sizeof(client_t), clis);
	if (!render_surfaces.capacity) {
		SDL_Log("Couldn't initialize surfaces vector");
		return SDL_APP_FAILURE;
	}

	for (int i = 0; i < clis; i++) {
		client_t client = {
			.surfaces = { .size = sizeof(surface_t) },
			.buffers = { .size = sizeof(buffer_t) },
		};
		if (append(&clients, &client) < 0) {
			// This should never happen, but e.
			SDL_Log("Appending client buffer/surface data failed");
			return SDL_APP_FAILURE;
		}
	}

	return SDL_APP_CONTINUE;
}

static client_t *get_client(int fd)
{
	const int i = fd - CLI_BEGIN;
	const bool error = i < 0
		|| clients.length <= i;

	if (error)
		return NULL;

	return index(clients, i);
}

static surface_t *get_surface(int fd, int surface_id)
{
	client_t *client = get_client(fd);
	if (!client)
		return NULL;

	const bool error = surface_id < 0
		|| client->surfaces.length <= surface_id;

	if (error)
		return NULL;

	surface_t *result = index(client->surfaces, surface_id);
	if (!result->allocated)
		return NULL;
	return result;
}

static buffer_t *get_buffer(int fd, int buffer_id)
{
	client_t *client = get_client(fd);
	if (!client)
		return NULL;

	const bool error = buffer_id < 0
		|| client->buffers.length <= buffer_id;

	if (error)
		return NULL;

	buffer_t *result = index(client->buffers, buffer_id);

	if (!result->allocated)
		return NULL;

	return result;
}

#define MESSAGE_TYPES(OPERATION) \
	OPERATION(gamesh_event_listen_op) \
	OPERATION(gamesh_sdl_event_quit) \
	OPERATION(gamesh_sdl_event_keyboard) \
	OPERATION(gamesh_sdl_event_mouse) \
	OPERATION(gamesh_sdl_event_gamepad) \
	OPERATION(gamesh_sdl_event_tick) \
	OPERATION(gamesh_sdl_event_resize) \
	OPERATION(gamesh_sdl_render_surface) \
	OPERATION(gamesh_sdl_buffer_add) \
	OPERATION(gamesh_sdl_buffer_set) \
	OPERATION(gamesh_sdl_render_surface_free) \
	OPERATION(gamesh_sdl_buffer_free)

#define CREATE_GLOBAL(MESSAGE_TYPE) int MESSAGE_TYPE = -1;
MESSAGE_TYPES(CREATE_GLOBAL)

#define PRINT_MESSAGE_TYPE(MESSAGE_TYPE) SDL_Log("%s %d\n", #MESSAGE_TYPE, MESSAGE_TYPE);

static inline void print_messages(void)
{
	MESSAGE_TYPES(PRINT_MESSAGE_TYPE)
}

typedef struct {
	int *destination;
	const char *name;
} message_opcode;

bool keep_running = true;

#define CREATE_STRUCT(MESSAGE_TYPE) { &MESSAGE_TYPE, #MESSAGE_TYPE },

// expects one cmsg header with file descriptors and nothing else
static int get_fd(struct msghdr header)
{
	struct cmsghdr *chdr = CMSG_FIRSTHDR(&header);
	if (!chdr)
		return -1;

	if (chdr->cmsg_level != SOL_SOCKET || chdr->cmsg_type != SCM_RIGHTS)
		return -1;

	int result = -1;
	memcpy(&result, CMSG_DATA(chdr), sizeof(result));
	return result;
}


vec_t *vec_for_opcode(int opcode)
{
	return opcode == gamesh_sdl_event_keyboard ? &keyboard_event_listeners
		: opcode == gamesh_sdl_event_mouse ? &mouse_event_listeners
		: opcode == gamesh_sdl_event_gamepad ? &gamepad_event_listeners
		: opcode == gamesh_sdl_event_quit ? &quit_event_listeners
		: opcode == gamesh_sdl_event_tick ? &tick_event_listeners
		: opcode == gamesh_sdl_event_resize ? &resize_event_listeners
		: NULL;
}

static int send_event(int opcode, SDL_Event *event)
{
	vec_t *listeners = vec_for_opcode(opcode);
	if (!listeners)
		return -1;

	for (size_t i = 0; i < listeners->length; i++) {
		int *event_fd = index(*listeners, (int)i);
		writeop(*event_fd, opcode, event, sizeof(*event));
	}
	return 0;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	int opcode = -1;
	switch (event->type) {
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_TEXT_EDITING:
		case SDL_EVENT_TEXT_INPUT:
		case SDL_EVENT_KEYMAP_CHANGED:
		case SDL_EVENT_KEYBOARD_ADDED:
		case SDL_EVENT_KEYBOARD_REMOVED:
		case SDL_EVENT_TEXT_EDITING_CANDIDATES:
		case SDL_EVENT_SCREEN_KEYBOARD_SHOWN:
		case SDL_EVENT_SCREEN_KEYBOARD_HIDDEN:
			opcode = gamesh_sdl_event_keyboard;
			break;

		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_WHEEL:
		case SDL_EVENT_MOUSE_ADDED:
		case SDL_EVENT_MOUSE_REMOVED:
			opcode = gamesh_sdl_event_mouse;
			break;

		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		case SDL_EVENT_GAMEPAD_ADDED:
		case SDL_EVENT_GAMEPAD_REMOVED:
		case SDL_EVENT_GAMEPAD_REMAPPED:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
		case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
		case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
		case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
			opcode = gamesh_sdl_event_gamepad;
			break;

		case SDL_EVENT_QUIT:
			opcode = gamesh_sdl_event_quit;
			break;
	}

	if (-1 < opcode)
		send_event(opcode, event);

	if (event->type == SDL_EVENT_QUIT) {
		keep_running = false;
		return SDL_APP_SUCCESS;
	}
	return SDL_APP_CONTINUE;
}

static int handle_new_listener(int fd, int *opcodes, int count)
{
	// only return an error if no opcodes were valid/appended
	int result = -1;
	for (; 0 < count; count--, opcodes++) {
		vec_t *vec = vec_for_opcode(*opcodes);
		if (!vec)
			continue;

		result &= append(vec, &fd);
	}

	return result;
}

static int handle_new_surface(int fd, int *buffer, int size)
{
	if (size < sizeof(int[2]))
		return -1;

	surface_t surface = {
		.allocated = true,
		.texture = NULL,
		.active_buffer = 0,
		.x = buffer[0],
		.y = buffer[1],
	};

	client_t *client = get_client(fd);
	if (!client)
		return -1;

	surface_t *free = client->surfaces_free;
	if (free) {
		ssize_t index = free - (surface_t*)client->surfaces.buffer;
		if (index < 0 || INT_MAX < index)
			return -1;

		client->surfaces_free = next_free_surface(client->surfaces_free);
		*free = surface;
		return (int)index;
	}

	if (append(&client->surfaces, &surface) < 0)
		return -1;

	return client->surfaces.length - 1;
}

static int update_texture(SDL_Surface *surface, SDL_Texture *texture)
{
	SDL_Surface *texture_surface;
	if (!SDL_LockTextureToSurface(texture, NULL, &texture_surface))
		return -1;

	int result = SDL_BlitSurface(surface, NULL, texture_surface, NULL);
	SDL_UnlockTexture(texture);
	return result ? 0 : -1;
}

static int handle_new_buffer(int fd, gamesh_shared_buffer_t *recv_buffer)
{
	const bool error = recv_buffer == NULL;

	if (error)
		return -1;

	client_t *client = get_client(fd);
	if (!client)
		return -1;

	buffer_t result = {
		.allocated = true,
		.buffer = recv_buffer,
	};

	buffer_t *free = client->buffers_free;
	if (free) {
		ssize_t index = free - (buffer_t*)client->buffers.buffer;
		if (index < 0 || INT_MAX < index)
			return -1;

		client->buffers_free = next_free_buffer(client->buffers_free);
		*free = result;
		return (int)index;
	}

	if (append(&client->buffers, &result) < 0)
		return -1;

	return client->buffers.length - 1;
}

static void emit_resize(int w, int h)
{
	int dimensions[] = { w, h };

	for (int i = 0; i < resize_event_listeners.length; i++) {
		int *fd = index(resize_event_listeners, i);
		writeop(*fd, gamesh_sdl_event_resize, dimensions, sizeof(dimensions));
	}
}

static int handle_set_surface_buffer(int fd, int *ids, int size)
{
	if (size < sizeof(int[2]))
		return -1;

	int surface_id = ids[0];
	int buffer_id = ids[1];

	surface_t *surface = get_surface(fd, surface_id);
	if (!surface)
		return -1;

	buffer_t *shared_buffer = get_buffer(fd, buffer_id);
	if (!shared_buffer)
		return -1;

	SDL_Surface *buffer = get_buffer_surface(shared_buffer);
	if (!buffer)
		return -1;

	surface->active_buffer = buffer_id;
	surface->changed = 1;
	int
		window_w = buffer->w + surface->x,
		window_h = buffer->h + surface->y;

	if (!renderer) {
		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
			SDL_Log("Couldn't initialize video/joystick: %s", SDL_GetError());
			return SDL_APP_FAILURE;
		}

		if (!(window = SDL_CreateWindow(window_title, window_w, window_h, 0))) {
			SDL_Log("Couldn't create window: %s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
		emit_resize(buffer->w, buffer->h);

		if (!(renderer = SDL_CreateRenderer(window, NULL))) {
			SDL_Log("Couldn't create renderer: %s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
	}

	if (!surface->texture) {
		surface->texture = SDL_CreateTexture(
			renderer,
			buffer->format,
			SDL_TEXTUREACCESS_STREAMING,
			buffer->w,
			buffer->h
		);
	}

	int current_window_w = 0, current_window_h = 0;
	if (!SDL_GetWindowSize(window, &current_window_w, &current_window_h)) {
		SDL_Log("Couldn't get window size: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (current_window_w < window_w || current_window_h < window_h) {
		int new_w = MAX(current_window_w, window_w);
		int new_h = MAX(current_window_h, window_h);
		if (!SDL_SetWindowSize(window, new_w, new_h)) {
			SDL_Log("Couldn't set new window size: %s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
		emit_resize(new_w, new_h);
	}

	return 0;
}

static void handle_free_surface(int fd, int *surface_id, int size)
{
	if (size < sizeof(int))
		return;

	client_t *client = get_client(fd);
	if (!client)
		return;

	surface_t *surface = get_surface(fd, *surface_id);
	if (!surface)
		return;

	if (surface->texture)
		SDL_DestroyTexture(surface->texture);

	surface->allocated = false;
	surface->next_free = client->surfaces_free;
	client->surfaces_free = surface;
}

static void handle_free_buffer(int fd, int *buffer_id, int size)
{
	if (size < sizeof(int))
		return;

	client_t *client = get_client(fd);
	if (!client)
		return;

	buffer_t *buffer = get_buffer(fd, *buffer_id);
	if (!buffer)
		return;

	gamesh_destroy_shared_buffer(buffer->buffer);
	buffer->allocated = false;
	buffer->next_free = client->buffers_free;
	client->buffers_free = buffer;
}

static void handle_requests(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	SDL_AppResult *result = context;
	if (fd == SRV_FILENO)
		return;

	if (opcode == gamesh_event_listen_op) {
		if (-1 < handle_new_listener(get_fd(header), buffer, size / sizeof(int))) {
			writeop(fd, gamesh_event_listen_op, NULL, 0);
			return;
		}
	} else if (opcode == gamesh_sdl_render_surface) {
		int result = handle_new_surface(fd, buffer, size);
		if (-1 < result) {
			writeop(fd, gamesh_sdl_render_surface, &result, sizeof(result));
			return;
		}
	} else if (opcode == gamesh_sdl_buffer_add) {
		gamesh_shared_buffer_t *recv_buffer = gamesh_recv_shared_buffer(
			opcode,
			buffer,
			size,
			header
		);
		int result = handle_new_buffer(fd, recv_buffer);
		if (-1 < result) {
			writeop(
				fd,
				gamesh_sdl_buffer_add,
				&result,
				sizeof(result)
			);
			return;
		}
	} else if (opcode == gamesh_sdl_buffer_set) {
		int result = handle_set_surface_buffer(fd, buffer, size);
		if (-1 < result) {
			writeop(fd, gamesh_sdl_buffer_set, NULL, 0);
			return;
		}
	} else if (opcode == gamesh_sdl_render_surface_free) {
		handle_free_surface(fd, buffer, size);
		return;
	} else if (opcode == gamesh_sdl_buffer_free) {
		handle_free_buffer(fd, buffer, size);
		return;
	}

	// error case
	writeop(fd, error_opcode, NULL, 0);
}

static SDL_FRect get_dest_rect(int x, int y, SDL_Surface *buffer)
{
	static const SDL_FRect error = { 0 };
	int w = buffer->w, h = buffer->h;
	int window_w, window_h;
	if (!SDL_GetWindowSize(window, &window_w, &window_h))
		return error;

	if (x < 0)
		x = x + 1 + window_w - w;
	if (y < 0)
		y = y + 1 + window_h - h;
	return (SDL_FRect) {
		.x = (float)x,
		.y = (float)y,
		.h = (float)h,
		.w = (float)w,
	};
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	SDL_AppResult result = SDL_APP_CONTINUE;

	struct pollfd last = { 0 };
	do {
		last = pollop(handle_requests, &result, 0);
	} while (last.revents && !(last.revents & POLLIN) && keep_running);

	for (int i = CLI_BEGIN; i < cli_end(); i++) {
		client_t *client = get_client(i);
		for (int j = 0; j < client->surfaces.length; j++) {
			surface_t *surface = index(client->surfaces, j);

			if (surface->texture) {
				buffer_t *shared_buffer
					= get_buffer(i, surface->active_buffer);
				SDL_Surface *buffer = get_buffer_surface(shared_buffer);

				if (surface->changed) {
					if (update_texture(buffer, surface->texture) < 0)
						continue;
					surface->changed = 0;
				}

				SDL_FRect dest = get_dest_rect(surface->x, surface->y, buffer);
				SDL_RenderTexture(renderer, surface->texture, NULL, &dest);

				surface->changed = 0;
			}
		}
	}
	SDL_RenderPresent(renderer);

	for (int i = 0; i < tick_event_listeners.length; i++) {
		int *event_fd = index(tick_event_listeners, i);
		Uint64 current_tick = SDL_GetTicks();
		writeop(
			*event_fd,
			gamesh_sdl_event_tick,
			&current_tick,
			sizeof(current_tick)
		);
	}

	return result;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	opcode_db *db = open_opcode_db();
	if (!db) {
		SDL_Log("Failed to open opcodedb");
		return SDL_APP_FAILURE;
	}
	static const message_opcode emit_opcodes[] = {
		MESSAGE_TYPES(CREATE_STRUCT)
		{ 0 },
	};

	for (const message_opcode *current = emit_opcodes; current->destination; current++) {
		*current->destination = get_opcode(db, current->name);
		if (*current->destination < 0) {
			SDL_Log("Failed to load opcode %s", current->name);
			return SDL_APP_FAILURE;
		}
	}

	close_opcode_db(db);

	SDL_AppResult init_clients_result = init_clients();
	if (init_clients_result != SDL_APP_CONTINUE)
		return init_clients_result;

	if (argc < 2)
		window_title = "SDL Runner";
	else
		window_title = argv[1];

	SDL_AppResult result = SDL_APP_CONTINUE;
	struct pollfd last = { 0 };
	do {
		last = pollop(handle_requests, &result, 0);
	} while (
		last.revents
		&& !(last.revents & POLLIN)
		&& keep_running
		&& !window
	);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}
