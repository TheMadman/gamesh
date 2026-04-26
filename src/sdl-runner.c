#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <srvsh.h>
#include <libadt.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "gamesh/gamesh.h"

typedef struct libadt_vector vec_t;

#define DEFAULT_HEIGHT 720
#define DEFAULT_WIDTH 1280

#define ARRLENGTH(arr) (sizeof(arr) / sizeof(arr[0]))

SDL_Window *window;
SDL_Renderer *renderer;

int event_fd = -1;
int gamesh_new_event_listener_event = -1;

vec_t keyboard_event_listeners = { .size = sizeof(int) };
vec_t mouse_event_listeners = { .size = sizeof(int) };
vec_t gamepad_event_listeners = { .size = sizeof(int) };

#define LISTEN_EVENTS(OPERATION) \
	OPERATION(gamesh_sdl_texture_event) \
	OPERATION(gamesh_sdl_texture_buffer_add_event) \
	OPERATION(gamesh_sdl_texture_buffer_swap_event)

#define EMIT_EVENTS(OPERATION) \
	OPERATION(gamesh_sdl_quit_event) \
	OPERATION(gamesh_sdl_keyboard_event) \
	OPERATION(gamesh_sdl_mouse_event) \
	OPERATION(gamesh_sdl_gamepad_event)

#define CREATE_GLOBAL(MESSAGE_TYPE) int MESSAGE_TYPE = -1;
LISTEN_EVENTS(CREATE_GLOBAL)
EMIT_EVENTS(CREATE_GLOBAL)

typedef struct {
	int *destination;
	const char *name;
} message_opcode;

#define CREATE_STRUCT(MESSAGE_TYPE) { &MESSAGE_TYPE, #MESSAGE_TYPE },

// I should probably add some inline functions like this in libadt..
static int append(vec_t *vec, void *data)
{
	vec_t attempt = libadt_vector_append(*vec, data);
	if (libadt_vector_identity(attempt, *vec))
		return -1;
	*vec = attempt;
	return 0;
}

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
	event_fd = gamesh_event_fd();
	if (event_fd < 0) {
		fprintf(stderr, "Failed to open event_fd\n");
		return SDL_APP_FAILURE;
	}

	opcode_db *db = open_opcode_db();
	if (!db) {
		fprintf(stderr, "Failed to open opcodedb\n");
		return SDL_APP_FAILURE;
	}

	static const message_opcode emit_opcodes[] = {
		EMIT_EVENTS(CREATE_STRUCT)
		{ 0 },
	};

	for (const message_opcode *c = emit_opcodes; c->destination; c++) {
		*c->destination = get_opcode(db, c->name);
		if (*c->destination < 0) {
			fprintf(stderr, "Failed to load opcode %s\n", c->name);
			return SDL_APP_FAILURE;
		}

		if (gamesh_event_emit(*c->destination) < 0) {
			fprintf(stderr, "Failed to register emit event %s\n", c->name);
			return SDL_APP_FAILURE;
		}
	}

	static const message_opcode listen_opcodes[] = {
		LISTEN_EVENTS(CREATE_STRUCT)
		{ 0 },
	};

	for (const message_opcode *c = listen_opcodes; c->destination; c++) {
		*c->destination = get_opcode(db, c->name);
		if (*c->destination < 0) {
			fprintf(stderr, "Failed to load opcode %s\n", c->name);
			return SDL_APP_FAILURE;
		}

		if (gamesh_event_listen(*c->destination) < 0) {
			fprintf(stderr, "Failed to register listen event %s\n", c->name);
			return SDL_APP_FAILURE;
		}
	}

	gamesh_new_event_listener_event = get_opcode(db, "gamesh_new_event_listener_event");
	if (gamesh_new_event_listener_event < 0) {
		fprintf(stderr, "Failed to load opcode gamesh_new_event_listener_event\n");
		return SDL_APP_FAILURE;
	}

	close_opcode_db(db);

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
	return opcode == gamesh_sdl_keyboard_event ? &keyboard_event_listeners
		: opcode == gamesh_sdl_mouse_event ? &mouse_event_listeners
		: opcode == gamesh_sdl_gamepad_event ? &gamepad_event_listeners
		: NULL;
}

static int send_event(int opcode, SDL_Event *event)
{
	vec_t *listeners = vec_for_opcode(opcode);
	assert(listeners);
	if (!listeners)
		return -1;

	for (int i = 0; i < listeners->length; i++) {
		int *event_fd = libadt_vector_index(*listeners, i);
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
			opcode = gamesh_sdl_keyboard_event;
			break;

		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_WHEEL:
		case SDL_EVENT_MOUSE_ADDED:
		case SDL_EVENT_MOUSE_REMOVED:
			opcode = gamesh_sdl_mouse_event;
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
			opcode = gamesh_sdl_gamepad_event;
			break;
	}

	if (-1 < opcode)
		send_event(opcode, event);

	return SDL_APP_CONTINUE;
}

void handle_new_event_listener(int event_opcode, struct msghdr header)
{
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&header);
	for (; cmsg; cmsg = CMSG_NXTHDR(&header, cmsg)) {
		if (!(
			cmsg->cmsg_type == SOL_SOCKET
			&& cmsg->cmsg_level == SCM_RIGHTS
		))
			continue;

		vec_t *event_listeners = vec_for_opcode(event_opcode);
		assert(event_listeners);
		if (!event_listeners) {
			fprintf(stderr, "Received listener for invalid opcode");
			goto close_fds;
		}

		if (append(event_listeners, CMSG_DATA(cmsg)) < 0) {
			fprintf(stderr, "Failed to add event_opcode listener\n");
			goto close_fds;
		}
	}

close_fds:
	close_cmsg_fds(header);
}

void handle_events(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	if (opcode == gamesh_new_event_listener_event) {
		if (size != sizeof(int))
			return;

		int event_opcode = *(int*)buffer;
		handle_new_event_listener(event_opcode, header);
		return;
	}
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	SDL_AppResult result = { 0 };

	struct pollfd last = { 0 };
	do {
		struct pollfd event_pollfd = {
			.fd = event_fd,
		};
		last = pollopfd(
			event_pollfd,
			handle_events,
			&result,
			0
		);
	} while (last.revents && !(last.revents & POLLIN));

	return result;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}
