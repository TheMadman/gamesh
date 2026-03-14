#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <srvsh.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "gamesh/game-runner.h"
#include "gamesh/gamesh.h"
#include "gamesh/fd-manager.h"

#define DEFAULT_HEIGHT 720
#define DEFAULT_WIDTH 1280

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

#define MESSAGE_LIST(PROCESSOR) \
	PROCESSOR(gamesh_exit) \
	PROCESSOR(gamesh_input_keyboard) \
	PROCESSOR(gamesh_input_mouse) \
	PROCESSOR(gamesh_input_gamepad) \
	PROCESSOR(gamesh_sprite) \
	PROCESSOR(gamesh_collision) \
	PROCESSOR(gamesh_event_fd_request) \
	PROCESSOR(gamesh_event_fd_response)

#define GLOBAL(MESSAGE) int MESSAGE;
MESSAGE_LIST(GLOBAL)
#undef GLOBAL

fd_manager_t *client_event_fds = NULL;

static ssize_t send_fd(int receiver, int opcode, int data)
{
	union {
		struct cmsghdr header;
		char buf[CMSG_SPACE(sizeof(data))];
	} cmsg = {
		.header = {
			.cmsg_level = SOL_SOCKET,
			.cmsg_type = SCM_RIGHTS,
			.cmsg_len = CMSG_LEN(sizeof(data)),
		},
	};

	memcpy(CMSG_DATA(&cmsg.header), &data, sizeof(data));

	return sendmsgop(
		receiver,
		opcode,
		NULL,
		0,
		&cmsg.header,
		sizeof(cmsg)
	);
}

ssize_t write_input(int fd, SDL_Event *event)
{
	return writeop(fd, (int)event->type, event, sizeof(*event));
}

static void send_event_fd_response(int fd)
{
	int eventfds[2] = { 0 };
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, eventfds) < 0) {
		// sending a response with no file descriptors to indicate
		// an error
		writeop(fd, gamesh_event_fd_response, NULL, 0);
		return;
	}

	if (fd_manager_add(client_event_fds, eventfds[0]) < 0) {
		writeop(fd, gamesh_event_fd_response, NULL, 0);
		return;
	}

	if (send_fd(fd, gamesh_event_fd_response, eventfds[1]) < 0) {
		writeop(fd, gamesh_event_fd_response, NULL, 0);
		return;
	}

	close(eventfds[1]);
}

static void handle_client_messages(
	int fd,
	int opcode,
	void *buf,
	int buflen,
	struct msghdr header,
	void *context
)
{
	SDL_AppResult *result = context;

	if (opcode == gamesh_event_fd_request)
		send_event_fd_response(fd);

	*result = SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	opcode_db *db = open_opcode_db();
	if (!db) {
		fprintf(stderr, "Failed to open opcodedb\n");
		return SDL_APP_FAILURE;
	}

#define STRUCT(MESSAGE) { #MESSAGE, &MESSAGE },

	static const struct message {
		const char *name;
		int *code;
	} messages[] = {
		MESSAGE_LIST(STRUCT)
		{ 0 },
	};

	for (const struct message *message = messages; message->name; message++) {
		*message->code = get_opcode(db, message->name);
		if (*message->code < 0) {
			fprintf(stderr, "Failed to get opcode for message %s\n", message->name);
			return SDL_APP_FAILURE;
		}
	}

	close_opcode_db(db);

	client_event_fds = fd_manager(1024);
	if (!client_event_fds)
		return SDL_APP_FAILURE;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
		SDL_Log("Couldn't initialize video/joystick: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!(window = SDL_CreateWindow("some-string", DEFAULT_WIDTH, DEFAULT_HEIGHT, 0))) {
		SDL_Log("Couldn't create window: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (!SDL_CreateRenderer(window, NULL)) {
		SDL_Log("Couldn't create renderer: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB24,
		SDL_TEXTUREACCESS_TARGET,
		DEFAULT_WIDTH,
		DEFAULT_HEIGHT
	);

	if (!texture) {
		SDL_Log("Couldn't create texture: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	for (
		int i = fd_manager_first(client_event_fds);
		-1 < i;
		i = fd_manager_next(client_event_fds, i)
	) {
		write_input(i, event);
	}

	if (event->type == SDL_EVENT_QUIT)
		return SDL_APP_SUCCESS;

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	SDL_AppResult result = { 0 };

	struct pollfd last = { 0 };
	do {
		last = pollop(
			handle_client_messages,
			&result,
			0
		);
	} while (last.revents && !(last.revents & POLLIN));

	SDL_SetRenderTarget(renderer, texture);
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	return result;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	fd_manager_free(client_event_fds);
}
