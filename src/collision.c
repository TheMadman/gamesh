#include <srvsh.h>
#include <SDL3/SDL.h>
#include <libadt.h>

#include <stdlib.h>
#include <stdio.h>

#define pexit(...) \
	fprintf(stderr, "%s:%d ", __FILE__, __LINE__), \
	fprintf(stderr, __VA_ARGS__), \
	fprintf(stderr, "\n"), \
	exit(EXIT_FAILURE)

typedef struct libadt_vector vec_t;

typedef struct {
	int class;
	SDL_FRect location;
	struct {
		float x;
		float y;
	} velocity;
	struct {
		float x;
		float y;
	} acceleration;

	// physics stuff
	struct {
		enum {
			TYPE_STATIC,
			TYPE_STATIC_PHYSICAL,
			TYPE_PHYSICAL,
		} type;
		float mass;
		float conservation;
	};
} collision_t;

vec_t positions = {.size = sizeof(collision_t)};

#define MESSAGE_TYPES(OPERATION) \
	OPERATION(gamesh_collision_surface) \
	OPERATION(gamesh_collision_surface_position) \
	OPERATION(gamesh_collision_surface_free)

#define INIT_GLOBAL(MESSAGE) int MESSAGE = -1;

MESSAGE_TYPES(INIT_GLOBAL)

static void handle_response(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	int *client_fd = context;
	sendmsgop(
		*client_fd,
		opcode,
		data,
		size,
		header.msg_control,
		header.msg_controllen
	);

	close_cmsg_fds(header);
}

static void handle_request(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	if (!is_cli(fd))
		return;

	sendmsgop(
		SRV_FILENO,
		opcode,
		data,
		size,
		header.msg_control,
		header.msg_controllen
	);

	pollopsrv(handle_response, &fd, -1);

	close_cmsg_fds(header);
}

typedef struct {
	int *dest;
	const char *name;
} message_t;

#define CREATE_STRUCT(MESSAGE) { .dest = &MESSAGE, .name = #MESSAGE },

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		pexit("open_opcode_db");

	static const message_t messages[] = {
		MESSAGE_TYPES(CREATE_STRUCT)
		{ 0 },
	};

	for (const message_t *message = messages; message->dest; message++) {
		*message->dest = get_opcode(db, message->name);
		if (*message->dest < 0)
			pexit("load_opcode(%s)", message->name);
	}

	close_opcode_db(db);

	int ncli = cli_count();
	do {
		struct pollfd result = pollop(
			handle_request,
			NULL,
			-1
		);
		if (is_cli(result.fd) && result.revents & POLLHUP)
			ncli--;
	} while (0 < ncli);
}
