#include "gamesh/gamesh.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <srvsh.h>

#define MESSAGE_LIST(OPERATION) \
	OPERATION(gamesh_event_listen_op)

#define DECLARE_INT(MESSAGE) int MESSAGE = -1;
MESSAGE_LIST(DECLARE_INT)
#undef DECLARE_INT

/*
 * candidate for moving somewhere global/reusable?
 */
static ssize_t send_fd(int receiver, int opcode, void *data, int length, int fd)
{
	union {
		struct cmsghdr header;
		char buf[CMSG_SPACE(sizeof(fd))];
	} cmsg = {
		.header = {
			.cmsg_level = SOL_SOCKET,
			.cmsg_type = SCM_RIGHTS,
			.cmsg_len = CMSG_LEN(sizeof(fd)),
		},
	};

	memcpy(CMSG_DATA(&cmsg.header), &fd, sizeof(fd));

	return sendmsgop(
		receiver,
		opcode,
		data,
		length,
		&cmsg.header,
		sizeof(cmsg)
	);
}

#define OPCODE(MESSAGE) { #MESSAGE, &MESSAGE },

static int init_opcodes(void)
{
	static const struct opcode {
		const char *message;
		int *destination;
	} opcodes[] = {
		MESSAGE_LIST(OPCODE)
		{ 0 },
	};

	opcode_db *db = NULL;

	const struct opcode *current;
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

void gamesh_event_fd_close(int fd)
{
	close(fd);
}

static void event_listen_response(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	*(int*)context = opcode == gamesh_event_listen_op ? 0 : -1;
}

int gamesh_events_listen(int opcodec, int *opcodev)
{
	// I don't like doing this in every single function but
	// I can't figure out something better yet
	if (init_opcodes() == -1)
		return -1;

	int sockets[2] = { 0 };
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
		return -1;

	ssize_t written = send_fd(
		SRV_FILENO,
		gamesh_event_listen_op,
		opcodev,
		sizeof(*opcodev) * opcodec,
		sockets[1]
	);
	close(sockets[1]);
	if (written < 0)
		goto close_socket;

	int result = -1;

	pollopsrv(event_listen_response, &result, -1);

	if (result < 0)
		goto close_socket;

	return sockets[0];

close_socket:
	close(sockets[0]);
	return -1;
}

int gamesh_event_listen(int opcode)
{
	return gamesh_events_listen(1, &opcode);
}

