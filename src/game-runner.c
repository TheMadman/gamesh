#include <srvsh.h>
#include <stdio.h>
#include <stdlib.h>
#include <libadt.h>
#include <unistd.h>

#include "gamesh/fd-manager.h"

#define ARRAY_SIZE libadt_util_arrlength
#define ARRAY_END libadt_util_arrend

#define PERROR_EXIT(...) fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"), exit(EXIT_FAILURE)

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

typedef struct libadt_vector vec_t;

#define CREATE_MESSAGE_STRUCT(MESSAGE) { #MESSAGE, &MESSAGE },

static const message_t messages[] = {
	MESSAGE_LIST(CREATE_MESSAGE_STRUCT)
};

int fd_buffer[1024] = { 0 };

typedef struct {
	int id;
	fd_manager_t fd_manager;
} entry_t;

typedef struct {
	int id;
	int event_socket;
} event_socket_entry_t;

// not sure how I feel about setting .size directly but w/e
vec_t client_event_fds = { .size = sizeof(event_socket_entry_t) };
vec_t opcode_listener_fds = { .size = sizeof(entry_t) };
vec_t opcode_emitter_fds = { .size = sizeof(entry_t) };

static void *vec_find(
	vec_t vector,
	void *element,
	int (*comp)(void *, void*)
)
{
	for (unsigned i = 0; i < vector.length; i++) {
		void *c = libadt_vector_index(vector, i);
		if (!comp(c, element))
			return c;
	}
	return NULL;
}

static int comp_ints(void *a, void *b)
{
	return *(int*)a - *(int*)b;
}

static int comp_entries(void *a, void *b)
{
	return comp_ints(
		&((entry_t*)a)->id,
		&((entry_t*)b)->id
	);
}

static int comp_event_socket_entries(void *a, void *b)
{
	return comp_ints(
		&((event_socket_entry_t*)a)->id,
		&((event_socket_entry_t*)b)->id
	);
}

typedef struct {
	vec_t *vec;
	int id;
} find_entry_t;

static entry_t *find_entry(vec_t vector, int id)
{
	entry_t id_wrapper = { .id = id };
	return vec_find(vector, &id_wrapper, comp_entries);
}

static int find_event_socket(vec_t vector, int id)
{
	event_socket_entry_t wrapper = { .id = id };
	event_socket_entry_t *found = vec_find(vector, &wrapper, comp_event_socket_entries);
	if (!found)
		return -1;
	return found->event_socket;
}

static ssize_t send_fd(int receiver, int opcode, int data)
{
	union {
		struct cmsghdr header;
		char buf[CMSG_SPACE(sizeof(data))];
	} cmsg = {
		.header = {
			.cmsg_level = SOL_SOCKET,
			.cmsg_type = SCM_RIGHTS,
			.cmsg_len = CMSG_LEN(sizeof(data)),
		},
	};

	memcpy(CMSG_DATA(&cmsg.header), &data, sizeof(data));

	return sendmsgop(
		receiver,
		opcode,
		NULL,
		0,
		&cmsg.header,
		sizeof(cmsg)
	);
}

static bool send_event_fd_response(int fd)
{
	int eventfds[2] = { 0 };
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, eventfds) < 0)
		goto write_failure_response;

	int entry = find_event_socket(client_event_fds, fd);

	if (-1 < entry)
		goto close_sockets;

	event_socket_entry_t new_entry = {
		.id = fd,
		.event_socket = eventfds[0],
	};
	vec_t attempt = libadt_vector_append(client_event_fds, &new_entry);

	if (libadt_vector_identity(attempt, client_event_fds))
		goto close_sockets;

	client_event_fds = attempt;

	if (send_fd(fd, gamesh_event_fd_response, eventfds[1]) < 0)
		goto close_sockets;

	close(eventfds[1]);
	return true;

close_sockets:
	close(eventfds[0]);
	close(eventfds[1]);
write_failure_response:
	writeop(fd, gamesh_event_fd_response, NULL, 0);
	return false;
}

typedef entry_t *entry_op(entry_t *, void *);

static entry_t *then(entry_t *entry, entry_op *op, void *context)
{
	if (!entry)
		return entry;
	return op(entry, context);
}

static entry_t *or(entry_t *entry, entry_op *op, void *context)
{
	if (entry)
		return entry;
	return op(entry, context);
}

static entry_t *find_entry_wrapper(entry_t *entry, void *context)
{
	find_entry_t *find = context;
	return find_entry(*find->vec, find->id);
}

static entry_t *insert_other(entry_t *_, void *context)
{
	(void)_;
	find_entry_t *find = context;
	entry_t new_entry = {
		.id = find->id,
		.fd_manager = fd_manager_manage(fd_buffer, ARRAY_SIZE(fd_buffer)),
	};
	vec_t attempt = libadt_vector_append(*find->vec, &new_entry);
	if (libadt_vector_identity(*find->vec, attempt))
		return NULL;
	*find->vec = attempt;
	return libadt_vector_index(*find->vec, find->vec->length - 1);
}

static entry_t *send_event_fds_to(entry_t *listeners, void *context)
{
	int *emitter_fd = context;
	fd_manager_t *const manager = &listeners->fd_manager;
	for (
		int listener_fd = fd_manager_first(manager);
		-1 < listener_fd;
		listener_fd = fd_manager_next(manager, listener_fd)
	) {
		int listener_event_fd = find_event_socket(
			client_event_fds,
			listener_fd
		);
		if (listener_event_fd < 0)
			continue;

		send_fd(
			*emitter_fd,
			gamesh_event_new_listener_event,
			listener_event_fd
		);
	}
	return listeners;
}

static entry_t *send_event_fds_to_each(entry_t *emitters, void *context)
{
	int *listener_event_fd = context;
	fd_manager_t *const manager = &emitters->fd_manager;
	for (
		int emitter_fd = fd_manager_first(manager);
		-1 < emitter_fd;
		emitter_fd = fd_manager_next(manager, emitter_fd)
	) {
		int emitter_event_fd = find_event_socket(
			client_event_fds,
			emitter_fd
		);
		if (emitter_event_fd < 0)
			continue;

		send_fd(
			emitter_event_fd,
			gamesh_event_new_listener_event,
			*listener_event_fd
		);
	}
	return emitters;
}

static entry_t *add_client_fd(entry_t *entry, void *context)
{
	int *client_fd = context;
	fd_manager_t *const fd_manager = &entry->fd_manager;
	for (
		int c = fd_manager_first(fd_manager);
		-1 < c;
		c = fd_manager_next(fd_manager, c)
	) {
		if (c == *client_fd)
			return entry;
	}
	if (fd_manager_add(&entry->fd_manager, *client_fd) < 0)
		return NULL;
	return entry;
}

static bool add_client_listener_for_opcode(int client_fd, int opcode)
{
	int event_fd = find_event_socket(client_event_fds, client_fd);
	if (event_fd < 0)
		return false;

	find_entry_t other = {
		.vec = &opcode_listener_fds,
		.id = opcode,
	};
	entry_t *entry = find_entry(opcode_listener_fds, opcode);
	entry = or(entry, insert_other, &other);
	entry = then(entry, add_client_fd, &client_fd);
	find_entry_t find_emitters = {
		.vec = &opcode_emitter_fds,
		.id = opcode,
	};
	entry = then(entry, find_entry_wrapper, &find_emitters);

	int opcode_to_send = entry ? gamesh_event_listen_response : -1;
	writeop(client_fd, opcode_to_send, NULL, 0);

	entry = then(entry, send_event_fds_to_each, &event_fd);

	return !!entry;
}

static bool add_client_emitter_for_opcode(int client_fd, int opcode)
{
	int event_fd = find_event_socket(client_event_fds, client_fd);
	if (event_fd < 0)
		return false;

	find_entry_t other = {
		.vec = &opcode_emitter_fds,
		.id = opcode,
	};
	entry_t *entry = find_entry(opcode_emitter_fds, opcode);
	entry = or(entry, insert_other, &other);
	entry = then(entry, add_client_fd, &client_fd);
	find_entry_t find_listeners = {
		.vec = &opcode_listener_fds,
		.id = opcode,
	};
	entry = then(entry, find_entry_wrapper, &find_listeners);

	int opcode_to_send = entry? gamesh_event_emit_response : -1;
	writeop(client_fd, opcode_to_send, NULL, 0);

	entry = then(entry, send_event_fds_to, &event_fd);

	return !!entry;
}

static void handle_client_messages(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	if (opcode == gamesh_event_fd_request) {
		send_event_fd_response(fd);
		return;
	} else if (opcode == gamesh_event_listen_request) {
		if (size != sizeof(int)) {
			writeop(fd, -1, NULL, 0);
			return;
		}

		add_client_listener_for_opcode(fd, *(int*)data);
		return;
	} else if (opcode == gamesh_event_emit_request) {
		if (size != sizeof(int)) {
			writeop(fd, -1, NULL, 0);
			return;
		}

		add_client_emitter_for_opcode(fd, *(int*)data);
		return;
	}
}

int main()
{
	opcode_db *db = open_opcode_db();
	if (!db)
		PERROR_EXIT("open_opcode_db failed");

	for (const message_t *current = messages; current < ARRAY_END(messages); current++) {
		*current->opcode = get_opcode(db, current->name);
		if (*current->opcode < 0)
			PERROR_EXIT("get_opcode failed for %s", current->name);
	}

	close_opcode_db(db);

	fd_manager_init_buffer(fd_buffer, ARRAY_SIZE(fd_buffer));

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
