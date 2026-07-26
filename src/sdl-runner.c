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

// I should probably add some inline functions like this in libadt..
static int append(vec_t *vec, void *data)
{
	vec_t attempt = libadt_vector_append(*vec, data);
	if (libadt_vector_identity(attempt, *vec))
		return -1;
	*vec = attempt;
	return 0;
}

SDL_Window *window;
SDL_Renderer *renderer;

int error_opcode = -1;
int event_fd = -1;
int gamesh_event_new_listener_event = -1;

vec_t keyboard_event_listeners = { .size = sizeof(int) };
vec_t mouse_event_listeners = { .size = sizeof(int) };
vec_t gamepad_event_listeners = { .size = sizeof(int) };
vec_t quit_event_listeners = { .size = sizeof(int) };

typedef struct {
	vec_t buffers;
	int active_buffer;
	SDL_Texture *texture;
} surface_t;

vec_t render_surfaces = { .size = sizeof(vec_t) };

SDL_AppResult init_render_surfaces(void)
{
	render_surfaces = libadt_vector_init(sizeof(vec_t), cli_count());
	if (!render_surfaces.capacity) {
		SDL_Log("Couldn't initialize surfaces vector");
		return SDL_APP_FAILURE;
	}

	for (int i = 0; i < cli_count(); i++) {
		vec_t client_surfaces = { .size = sizeof(surface_t) };
		if (append(&render_surfaces, &client_surfaces) < 0) {
			// This should never happen, but e.
			SDL_Log("Appending client surfaces failed");
			return SDL_APP_FAILURE;
		}
	}

	return SDL_APP_CONTINUE;
}

static vec_t *get_client_surfaces(int fd)
{
	if (render_surfaces.length <= fd - CLI_BEGIN)
		return NULL;
	return index(render_surfaces, fd - CLI_BEGIN);
}

static surface_t *get_surface(vec_t *surfaces, int surface_id)
{
	if (surfaces->length <= surface_id)
		return NULL;
	return index(*surfaces, surface_id);
}

static gamesh_shared_buffer_t *get_buffer(surface_t surface, int i)
{
	bool error = i < 0
		|| surface.buffers.length <= i;

	if (error)
		return NULL;

	return *(gamesh_shared_buffer_t**)index(surface.buffers, i);
}

#define MESSAGE_TYPES(OPERATION) \
	OPERATION(gamesh_event_listen_op) \
	OPERATION(gamesh_sdl_event_quit) \
	OPERATION(gamesh_sdl_event_keyboard) \
	OPERATION(gamesh_sdl_event_mouse) \
	OPERATION(gamesh_sdl_event_gamepad) \
	OPERATION(gamesh_sdl_render_surface) \
	OPERATION(gamesh_sdl_render_surface_buffer_add) \
	OPERATION(gamesh_sdl_render_surface_buffer_swap)

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

	SDL_AppResult init_render_surfaces_result = init_render_surfaces();
	if (init_render_surfaces_result != SDL_APP_CONTINUE)
		return init_render_surfaces_result;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
		SDL_Log("Couldn't initialize video/joystick: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!(window = SDL_CreateWindow("some-string", DEFAULT_WIDTH, DEFAULT_HEIGHT, 0))) {
		SDL_Log("Couldn't create window: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!(renderer = SDL_CreateRenderer(window, NULL))) {
		SDL_Log("Couldn't create renderer: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

vec_t *vec_for_opcode(int opcode)
{
	return opcode == gamesh_sdl_event_keyboard ? &keyboard_event_listeners
		: opcode == gamesh_sdl_event_mouse ? &mouse_event_listeners
		: opcode == gamesh_sdl_event_gamepad ? &gamepad_event_listeners
		: opcode == gamesh_sdl_event_quit ? &quit_event_listeners
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

static int handle_new_surface(int fd)
{
	surface_t surface = {
		.buffers = {
			.size = sizeof(gamesh_shared_buffer_t*),
		},
		.active_buffer = 0,
		.texture = NULL,
	};

	vec_t *surfaces = get_client_surfaces(fd);
	if (!surfaces)
		// should never happen, so if it does something's seriously wrong
		return -1;

	if (append(surfaces, &surface) < 0) {
		return -1;
	}
	return surfaces->length - 1;
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

static int handle_new_surface_buffer(int fd, gamesh_recv_buffer_t recv_buffer)
{
	const bool error = recv_buffer.surface_id < 0
		|| recv_buffer.buffer == NULL;

	if (error)
		return -1;

	vec_t *surfaces = get_client_surfaces(fd);
	if (!surfaces)
		return -1;

	surface_t *surface = get_surface(surfaces, recv_buffer.surface_id);
	if (!surface)
		return -1;

	if (append(&surface->buffers, &recv_buffer.buffer) < 0)
		return -1;

	if (surface->texture == NULL) {
		SDL_Surface *buffer = gamesh_get_shared_buffer_surface(recv_buffer.buffer);
		surface->texture = SDL_CreateTexture(
			renderer,
			buffer->format,
			SDL_TEXTUREACCESS_STREAMING,
			buffer->w,
			buffer->h
		);
	}

	return surface->buffers.length - 1;
}

int handle_swap_surface(int fd, int *ids, int size)
{
	if (size < sizeof(int[2]))
		return -1;

	int surface_id = ids[0];
	int buffer_id = ids[1];

	vec_t *surfaces = get_client_surfaces(fd);
	if (!surfaces)
		return -1;

	surface_t *surface = get_surface(surfaces, surface_id);
	if (!surface)
		return -1;

	gamesh_shared_buffer_t *buffer = get_buffer(*surface, buffer_id);
	if (!buffer)
		return -1;

	surface->active_buffer = buffer_id;
	return 0;
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
		int result = handle_new_surface(fd);
		if (-1 < result) {
			writeop(fd, gamesh_sdl_render_surface, &result, sizeof(result));
			return;
		}
	} else if (opcode == gamesh_sdl_render_surface_buffer_add) {
		gamesh_recv_buffer_t recv_buffer = gamesh_recv_shared_buffer(
			opcode,
			buffer,
			size,
			header
		);
		int result = handle_new_surface_buffer(fd, recv_buffer);
		if (-1 < result) {
			writeop(
				fd,
				gamesh_sdl_render_surface_buffer_add,
				&result,
				sizeof(result)
			);
			return;
		}
	} else if (opcode == gamesh_sdl_render_surface_buffer_swap) {
		int result = handle_swap_surface(fd, buffer, size);
		if (-1 < result) {
			writeop(fd, gamesh_sdl_render_surface_buffer_swap, NULL, 0);
			return;
		}
	}

	// error case
	writeop(fd, error_opcode, NULL, 0);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
	SDL_RenderClear(renderer);

	SDL_AppResult result = SDL_APP_CONTINUE;

	struct pollfd last = { 0 };
	do {
		last = pollop(handle_requests, &result, 0);
	} while (last.revents && !(last.revents & POLLIN) && keep_running);

	for (int i = CLI_BEGIN; i < cli_end(); i++) {
		vec_t *client_surfaces = get_client_surfaces(i);

		for (int j = 0; j < client_surfaces->length; j++) {
			surface_t *surface = get_surface(client_surfaces, j);
			if (surface->texture) {
				gamesh_shared_buffer_t *buffer = get_buffer(*surface, surface->active_buffer);
				if (!buffer)
					continue;

				// probably some logic for "skip if buffer swapped"
				if (update_texture(gamesh_get_shared_buffer_surface(buffer), surface->texture) < 0) {
					SDL_Log("Couldn't update texture: %s", SDL_GetError());
					continue;
				}

				SDL_RenderTexture(renderer, surface->texture, NULL, NULL);
			}
		}
	}
	SDL_RenderPresent(renderer);

	return result;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}
