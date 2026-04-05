#include <srvsh.h>
#include <stdio.h>

#include "gamesh/fd-manager.h"

#define PERROR_EXIT(FORMAT, ...) fprintf(stderr, FORMAT "\n" __VA_OPT__(,) __VA_ARGS__)

#define MESSAGE_LIST(OPERATION) \
	OPERATION(gamesh_event_fd_request) \
	OPERATION(gamesh_event_fd_response) \
	OPERATION(gamesh_event_listen_request) \
	OPERATION(gamesh_event_listen_response) \
	OPERATION(gamesh_event_emit_request) \
	OPERATION(gamesh_event_emit_response) \
	OPERATION(gamesh_event_new_listener_event)

#define DECLARE_INT(MESSAGE) int MESSAGE = -1;
MESSAGE_LIST(DECLARE_INT)

typedef struct {
	const char *name;
	int *opcode;
} message_t;

#define CREATE_MESSAGE_STRUCT(MESSAGE) { #MESSAGE, &MESSAGE },

static const message_t messages[] = {
	MESSAGE_LIST(CREATE_MESSAGE_STRUCT)
	{ 0 },
};

int fd_buffer[1024] = { 0 };

typedef struct {
	int client_fd;
	fd_manager_t fd_manager;
} client_fd_manager_t;

typedef struct {
	int opcode;
	fd_manager_t fd_manager;
} opcode_fd_manager_t;

void handle_client_messages(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	switch (opcode) {
	}
}

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		PERROR_EXIT("open_opcode_db failed");

	for (const message_t *current = messages; current->opcode; current++) {
		*current->opcode = get_opcode(db, current->name);
		if (*current->opcode < 0)
			PERROR_EXIT("get_opcode failed for %s", current->name);
	}

	close_opcode_db(db);

	fd_manager_init_buffer(fd_buffer, sizeof(fd_buffer) / sizeof(fd_buffer[0]));

	int number_clients = cli_count();

	while (number_clients > 0) {
		struct pollfd last = pollop(
			handle_client_messages,
			NULL,
			// &result,
			0
		);
		if (last.revents && !(last.revents & POLLIN))
			number_clients--;
	}
}
