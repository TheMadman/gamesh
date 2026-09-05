#include "gamesh/gamesh.h"
#include <srvsh.h>
#include <libadt.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct libadt_vector vec_t;

typedef struct client_s {
	vec_t surfaces;
	vec_t buffers;
} client_t;

vec_t pollfds = {.size = sizeof(struct pollfd)};
int count = 0;

int mouse_fd = -1;

#define MESSAGE_TYPES(OPERATION) \
	OPERATION(gamesh_sdl_event_mouse) \
	OPERATION(gamesh_sdl_render_surface) \
	OPERATION(gamesh_sdl_buffer_add) \
	OPERATION(gamesh_sdl_buffer_set) \
	OPERATION(gamesh_sdl_render_surface_free) \
	OPERATION(gamesh_sdl_buffer_free)

#define DECLARE_GLOBAL(MESSAGE_TYPE) int MESSAGE_TYPE = -1;

MESSAGE_TYPES(DECLARE_GLOBAL)

// _really_ should add this to libadt somewhere
int append(vec_t *vec, void *item)
{
	vec_t attempt = libadt_vector_append(*vec, item);
	if (libadt_vector_identity(*vec, attempt))
		return -1;
	*vec = attempt;
	return 0;
}

void handle_client_request(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	if (opcode == gamesh_sdl_render_surface) {}
	else if (opcode == gamesh_sdl_buffer_add) {}
}

void handle_polls(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	if (is_cli(fd)) {
		handle_client_request(fd, opcode, buffer, size, header, context);
	}
}

typedef struct {
	int *destination;
	const char *name;
} message_t;

#define CREATE_STRUCT(MESSAGE) { &MESSAGE, #MESSAGE },

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		exit(EXIT_FAILURE);

	message_t messages[] = {
		MESSAGE_TYPES(CREATE_STRUCT)
		{ 0 },
	};

	for (message_t *message = messages; message->destination; message++) {
		*message->destination = get_opcode(db, message->name);
		if (*message->destination < 0)
			exit(EXIT_FAILURE);
	}

	close_opcode_db(db);

	for (int i = CLI_BEGIN, end = cli_end(); i < end; i++) {
		struct pollfd cli_pollfd = {.fd = i};
		if (append(&pollfds, &cli_pollfd) < 0)
			perror("append"), exit(EXIT_FAILURE);
	}
	count = cli_count();

	mouse_fd = gamesh_event_listen(gamesh_sdl_event_mouse);
	if (mouse_fd < 0)
		exit(EXIT_FAILURE);

	struct pollfd mouse_pollfd = {.fd = mouse_fd};

	static const int block_forever = -1;

	do {
		struct pollfd result = pollopfds(
			pollfds.buffer,
			pollfds.length,
			handle_polls,
			NULL,
			block_forever
		);
		if (result.revents & POLLHUP)
			--count;
	} while (-1 < count);
}
