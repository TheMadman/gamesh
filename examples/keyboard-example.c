#include <gamesh.h>
#include <srvsh.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <stdbool.h>

int gamesh_sdl_event_keyboard = -1;
int gamesh_sdl_event_quit = -1;
bool keep_running = true;

#define ARRLEN(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

#define PERROR_EXIT(MESSAGE) fprintf(stderr, "%s:%d ", __FILE__, __LINE__), perror(MESSAGE), exit(EXIT_FAILURE)

#define EVENT_TYPES(OPERATION) \
	OPERATION(SDL_EVENT_KEY_DOWN) \
	OPERATION(SDL_EVENT_KEY_UP) \
	OPERATION(SDL_EVENT_TEXT_EDITING) \
	OPERATION(SDL_EVENT_TEXT_INPUT) \
	OPERATION(SDL_EVENT_KEYMAP_CHANGED) \
	OPERATION(SDL_EVENT_KEYBOARD_ADDED) \
	OPERATION(SDL_EVENT_KEYBOARD_REMOVED) \
	OPERATION(SDL_EVENT_TEXT_EDITING_CANDIDATES) \
	OPERATION(SDL_EVENT_SCREEN_KEYBOARD_SHOWN) \
	OPERATION(SDL_EVENT_SCREEN_KEYBOARD_HIDDEN)

#define CREATE_STRUCT(EVENT) { EVENT, #EVENT },

typedef struct {
	SDL_EventType type;
	const char *name;
} event_name;

event_name events[] = {
	EVENT_TYPES(CREATE_STRUCT)
	{ 0 },
};

void print_event(SDL_Event *event)
{
	for (event_name *current = events; current->name; current++)
		if (event->type == current->type) {
			printf("%s", current->name);
			if (event->type == SDL_EVENT_KEY_DOWN)
				printf(" '%c'", ((SDL_KeyboardEvent *)event)->key);
			printf("\n");
			return;
		}
}

void process_event(
	int fd,
	int opcode,
	void *data,
	int length,
	struct msghdr header,
	void *context
)
{
	if (opcode == gamesh_sdl_event_quit)
		keep_running = false;
	else if (opcode == gamesh_sdl_event_keyboard) {
		if (length < sizeof(SDL_Event)) {
			fprintf(stderr, "Received event of wrong size\n");
			return;
		}
		print_event(data);
	}
}

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		PERROR_EXIT("failed opening opcode_db");

	gamesh_sdl_event_quit = get_opcode(db, "gamesh_sdl_event_quit");
	if (gamesh_sdl_event_quit < 0)
		PERROR_EXIT("failed to load gamesh_sdl_event_quit");

	gamesh_sdl_event_keyboard = get_opcode(db, "gamesh_sdl_event_keyboard");
	if (gamesh_sdl_event_keyboard < 0)
		PERROR_EXIT("failed to load gamesh_sdl_event_keyboard");

	int opcodes[] = {
		gamesh_sdl_event_quit,
		gamesh_sdl_event_keyboard,
	};

	int event_fd = gamesh_events_listen(opcodes, ARRLEN(opcodes));
	if (event_fd < 0)
		PERROR_EXIT("failed to get event_fd");

	struct pollfd poll_event = { .fd = event_fd };
	while (keep_running)
		pollopfd(poll_event, process_event, NULL, -1);
}
