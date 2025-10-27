#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <srvsh.h>

#include <stdio.h>

#include "gamesh/game-runner.h"
#include "gamesh/gamesh.h"

#define DEFAULT_HEIGHT 720
#define DEFAULT_WIDTH 1280

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

int
	gamesh_input_keyboard,
	gamesh_input_mouse,
	gamesh_input_gamepad,
	gamesh_sprite,
	gamesh_collision,
	gamesh_exit;

void handle_client_messages(
	int fd,
	int opcode,
	void *buf,
	int buflen,
	struct msghdr header,
	void *context
)
{
	SDL_AppResult *result = context;
}

ssize_t write_input(int fd, SDL_Event *event)
{
	switch (event->type) {
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP: {
			struct gamesh_keyboard shevent = {
				.timestamp = event->key.timestamp,
				.code = event->key.scancode,
				.key = event->key.key,
				.pressed = event->type == SDL_EVENT_KEY_DOWN,
			};
			return writeop(
				fd,
				gamesh_input_keyboard,
				&shevent,
				sizeof(shevent)
			);
		}
		case SDL_EVENT_QUIT:
			return writeop(
				fd,
				gamesh_exit,
				NULL,
				0
			);
		default:
			return -1;
	}
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	opcode_db *db = open_opcode_db();
	if (!db) {
		fprintf(stderr, "Failed to open opcodedb\n");
		return SDL_APP_FAILURE;
	}

	static const struct message {
		const char *name;
		int *code;
	} messages[] = {
		{"gamesh_input_keyboard", &gamesh_input_keyboard},
		{"gamesh_input_mouse", &gamesh_input_mouse},
		{"gamesh_input_gamepad", &gamesh_input_gamepad},
		{"gamesh_sprite", &gamesh_sprite},
		{"gamesh_collision", &gamesh_collision},
		{"gamesh_exit", &gamesh_exit},
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

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
		SDL_Log("Couldn't initialize video/joystick: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	if (
		!SDL_CreateWindowAndRenderer(
			"some-string",
			DEFAULT_WIDTH,
			DEFAULT_HEIGHT,
			0,
			&window,
			&renderer
		)
	) {
		SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
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
	for (int i = CLI_BEGIN; i < cli_end(); i++) {
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
}
