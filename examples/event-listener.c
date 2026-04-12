#include <gamesh.h>
#include <srvsh.h>
#include <stdio.h>
#include <stdlib.h>

int event_fd = -1;
int example_opcode = -1;

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
	if (opcode == example_opcode)
		puts("Listener received example_opcode");
}

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		PERROR_EXIT("failed opening opcode_db");

	example_opcode = get_opcode(db, "example_opcode");
	if (example_opcode < 0)
		PERROR_EXIT("failed to load example_opcode");

	event_fd = gamesh_event_fd();
	if (event_fd < 0)
		PERROR_EXIT("failed getting event_fd");

	if (gamesh_event_listen(example_opcode) < 0)
		PERROR_EXIT("failed setting event listener");

	struct pollfd poll_event = { .fd = event_fd };
	pollopfd(poll_event, process_event, NULL, -1);
}
