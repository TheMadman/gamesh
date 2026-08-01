#include <gamesh.h>
#include <srvsh.h>
#include <sys/socket.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ERROR_EXIT(...) fprintf(stderr, "%s:%d ", __FILE__, __LINE__), fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"), exit(EXIT_FAILURE)

int main()
{
	int tick_fd = gamesh_get_tick_fd();
	if (tick_fd < 0)
		ERROR_EXIT("gamesh_get_tick_fd()");

	uint64_t current = gamesh_get_tick(tick_fd);
	if (current == (uint64_t)-1)
		ERROR_EXIT("gamesh_get_tick()");

	while (current != (uint64_t)-1) {
		uint64_t after = gamesh_get_tick(tick_fd);
		printf("ticks/s: %7ld\r", 1000 / MAX(1, after - current));
		current = after;
	}
}
