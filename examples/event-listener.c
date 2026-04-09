#include <gamesh.h>
#include <stdio.h>
#include <stdlib.h>

int event_fd = -1;

#define PERROR_EXIT(...) fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"), exit(EXIT_FAILURE)

int main()
{
	event_fd = gamesh_event_fd();
	if (event_fd < 0)
		PERROR_EXIT("failed getting event_fd");
}
