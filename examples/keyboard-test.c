#include <gamesh.h>
#include <srvsh.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <stdbool.h>

int event_fd = -1;
int gamesh_sdl_event_keyboard = -1;
int gamesh_sdl_event_quit = -1;
bool keep_running = true;

#define PERROR_EXIT(...) fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"), exit(EXIT_FAILURE)

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
	else if (opcode == gamesh_sdl_event_keyboard)
		puts("Listener received gamesh_sdl_event_keyboard");
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

	event_fd = gamesh_event_fd();
	if (event_fd < 0)
		PERROR_EXIT("failed getting event_fd");

	if (gamesh_event_listen(gamesh_sdl_event_keyboard) < 0)
		PERROR_EXIT("failed setting event listener");
	if (gamesh_event_listen(gamesh_sdl_event_quit) < 0)
		PERROR_EXIT("failed setting event listener for quit event");

	struct pollfd poll_event = { .fd = event_fd };
	while (keep_running)
		pollopfd(poll_event, process_event, NULL, -1);
}
