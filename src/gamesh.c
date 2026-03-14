#include "gamesh/gamesh.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <srvsh.h>

#define MESSAGE_LIST(OPERATION) \
	OPERATION(gamesh_event_fd_request) \
	OPERATION(gamesh_event_fd_response) \
	OPERATION(gamesh_collision_fd_request) \
	OPERATION(gamesh_collision_fd_response) \
	OPERATION(gamesh_sprite_request) \
	OPERATION(gamesh_sprite_response)

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
	struct opcode {
		const char *message;
		int *destination;
	} opcodes[] = {
		MESSAGE_LIST(OPCODE)
		{ 0 },
	};

#undef OPCODE

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

static void event_response(
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

	struct pollfd result = pollopsrv(event_response, &response_fd, -1);

	return response_fd;
}

void gamesh_event_fd_close(int fd)
{
	close(fd);
}

static void sprite_id_response(
	int fd,
	int opcode,
	void *data,
	int length,
	struct msghdr header,
	void *context
)
{
	gamesh_sprite_id_t *sprite_id = context;

	if (length != sizeof(gamesh_sprite_id_t))
		return;

	memcpy(sprite_id, data, sizeof(sprite_id));
}

gamesh_sprite_id_t gamesh_texture(
	int fd,
	size_t height,
	size_t width,
	unsigned bit_depth
)
{
	if (init_opcodes() == -1)
		return -1;

	struct gamesh_sprite texture = {
		.height = height,
		.width = width,
		.bit_depth = bit_depth,
	};

	ssize_t request_result = send_fd(
		SRV_FILENO,
		gamesh_sprite_request,
		&texture,
		sizeof(texture),
		fd
	);

	if (request_result < 0)
		return -1;

	gamesh_sprite_id_t result = -1;

	struct pollfd response_result = pollopsrv(
		sprite_id_response,
		&result,
		-1
	);

	return result;
}

