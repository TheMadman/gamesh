#include "gamesh/gamesh.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <srvsh.h>

int
	gamesh_event_fd_request = -1,
	gamesh_event_fd_response = -1;

// if/when the list above gets too long I'll use
// X Macros for the first time
#define OPCODE(NAME) { #NAME, &NAME }

static int init_opcodes(void)
{
	struct opcode {
		const char *message;
		int *destination;
	} opcodes[] = {
		OPCODE(gamesh_event_fd_request),
		OPCODE(gamesh_event_fd_response),
		{ 0 },
	};

	opcode_db *db = NULL;

	struct opcode *current;
	for (current = opcodes; current->message; current++) {
		if (*current->destination == -1) {
			if (!db)
				db = open_opcode_db();

			*current->destination = get_opcode(db, current->message);
			if (*current->destination == -1)
				return -1;
		}
	}

	close_opcode_db(db);
	return 0;
}

static void await_response(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	int *response_fd = context;
	if (opcode != gamesh_event_fd_response)
		return;

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&header);

	if (!cmsg)
		return;

	if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
		return;

	memcpy(response_fd, CMSG_DATA(cmsg), sizeof(*response_fd));
}

int gamesh_event_fd(void)
{
	if (init_opcodes() == -1)
		return -1;

	ssize_t written = writesrv(gamesh_event_fd_request, NULL, 0);
	if (written < 0)
		return -1;

	int response_fd = -1;

	struct pollfd result = pollopsrv(await_response, &response_fd, -1);

	return response_fd;
}

void gamesh_event_fd_close(int fd)
{
	close(fd);
}
