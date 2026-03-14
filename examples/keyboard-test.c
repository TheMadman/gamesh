#include <gamesh.h>

#include <srvsh.h>
#include <SDL3/SDL_events.h>

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define perror_exit(msg) perror(msg), exit(1)

int
	gamesh_input_keyboard,
	gamesh_exit;

bool app_continue = true;

static void receive_event(
	int fd,
	int opcode,
	void *buf,
	int size,
	struct msghdr header,
	void *context
)
{
	memcpy(context, buf, sizeof(SDL_Event));
}

static SDL_Event wait_event(int event_fd)
{
	SDL_Event event = { 0 };
	struct pollfd event_pollfd = {.fd = event_fd};
	struct pollfd last = pollopfd(
		event_pollfd,
		receive_event,
		&event,
		-1
	);
	if (!(last.revents & POLLIN))
		exit(0);
	if (event.type == SDL_EVENT_QUIT)
		app_continue = false;

	return event;
}

int main()
{
	int event_fd = gamesh_event_fd();
	if (event_fd < 0)
		perror_exit("event_fd");

	while (app_continue) {
		SDL_Event event = wait_event(event_fd);
		printf(
			"%d",
			event.type
		);
		if (
			event.type == SDL_EVENT_KEY_DOWN
			|| event.type == SDL_EVENT_KEY_UP
		)
			printf("	%ld	%d	%d	%d\n",
			event.key.timestamp,
			event.key.scancode,
			event.key.key,
			event.type == SDL_EVENT_KEY_DOWN
		);
		else
			printf("\n");
	}
}
