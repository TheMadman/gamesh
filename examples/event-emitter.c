#include <gamesh.h>
#include <srvsh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int event_fd = -1;
int example_opcode = -1;
int gamesh_event_new_listener_event = -1;
int listener_fd = -1;

#define PERROR_EXIT(...) fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"), exit(EXIT_FAILURE)

int get_fd(struct msghdr header)
{
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&header);
	while (
		cmsg
		&& cmsg->cmsg_level != SOL_SOCKET
		&& cmsg->cmsg_type != SCM_RIGHTS
	) {
		cmsg = CMSG_NXTHDR(&header, cmsg);
	}
	if (!cmsg)
		return -1;

	int result = -1;
	memcpy(&result, CMSG_DATA(cmsg), sizeof(int));
	return result;
}

void process_event(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	if (opcode == gamesh_event_new_listener_event) {
		listener_fd = get_fd(header);
	}
}

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		PERROR_EXIT("failed opening opcode_db");

	example_opcode = get_opcode(db, "example_opcode");
	if (example_opcode < 0)
		PERROR_EXIT("failed to load example_opcode");

	gamesh_event_new_listener_event = get_opcode(db, "gamesh_event_new_listener_event");
	if (gamesh_event_new_listener_event < 0)
		PERROR_EXIT("failed to load gamesh_event_new_listener_event");

	event_fd = gamesh_event_fd();
	if (event_fd < 0)
		PERROR_EXIT("failed getting event_fd");

	if (gamesh_event_emit(example_opcode) < 0)
		PERROR_EXIT("failed setting event listener");

	struct pollfd poll_event = { .fd = event_fd };
	struct pollfd result = pollopfd(poll_event, process_event, NULL, -1);

	if (result.revents & POLLHUP)
		PERROR_EXIT("pollhup");

	if (writeop(listener_fd, example_opcode, NULL, 0) < 0)
		PERROR_EXIT("error writing");

	puts("Emitter exiting");
}
