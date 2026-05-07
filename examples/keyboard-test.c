#include <gamesh.h>
#include <srvsh.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

int event_fd = -1;
int gamesh_sdl_keyboard_event = -1;

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
	if (opcode == gamesh_sdl_keyboard_event)
		puts("Listener received gamesh_sdl_keyboard_event");
}

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		PERROR_EXIT("failed opening opcode_db");

	gamesh_sdl_keyboard_event = get_opcode(db, "gamesh_sdl_keyboard_event");
	if (gamesh_sdl_keyboard_event < 0)
		PERROR_EXIT("failed to load gamesh_sdl_keyboard_event");

	event_fd = gamesh_event_fd();
	if (event_fd < 0)
		PERROR_EXIT("failed getting event_fd");

	if (gamesh_event_listen(gamesh_sdl_keyboard_event) < 0)
		PERROR_EXIT("failed setting event listener");

	struct pollfd poll_event = { .fd = event_fd };
	pollopfd(poll_event, process_event, NULL, -1);
}
