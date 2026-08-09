#define _GNU_SOURCE // memfd_create

#include "gamesh/gamesh.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <srvsh.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MESSAGE_LIST(OPERATION) \
	OPERATION(gamesh_event_listen_op) \
	OPERATION(gamesh_sdl_render_surface) \
	OPERATION(gamesh_sdl_buffer_add) \
	OPERATION(gamesh_sdl_buffer_set) \
	OPERATION(gamesh_sdl_render_surface_position) \
	OPERATION(gamesh_sdl_render_surface_free) \
	OPERATION(gamesh_sdl_buffer_free) \
	OPERATION(gamesh_sdl_event_tick)


#define DECLARE_INT(MESSAGE) int MESSAGE = -1;
MESSAGE_LIST(DECLARE_INT)
#undef DECLARE_INT

struct gamesh_shared_buffer_s {
	int shmem_fd;
	SDL_Surface *surface;
};

typedef struct gamesh_surface_add_s {
	SDL_Surface sdl_surface;
} gamesh_surface_add_t;

/*
 * candidate for moving somewhere global/reusable?
 */
static int get_fd(struct msghdr header)
{
	struct cmsghdr *chdr = CMSG_FIRSTHDR(&header);
	if (!chdr)
		return -1;

	if (chdr->cmsg_level != SOL_SOCKET || chdr->cmsg_type != SCM_RIGHTS)
		return -1;

	int result = -1;
	memcpy(&result, CMSG_DATA(chdr), sizeof(result));
	return result;
}

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
			if (!db)
				return -1;

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
	//close_cmsg_fds(header);
}

int gamesh_events_listen(int *opcodes, int length)
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
		opcodes,
		sizeof(*opcodes) * length,
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
	return gamesh_events_listen(&opcode, 1);
}

static void handle_render_surface_response(
	int fd,
	int opcode,
	void *data,
	int size,
	struct msghdr header,
	void *context
)
{
	if (opcode != gamesh_sdl_render_surface)
		return;

	if (size < sizeof(int))
		return;

	*(int*)context = *(int*)data;
}

int gamesh_create_render_surface(int x, int y)
{
	if (init_opcodes() < 0)
		return -1;

	int coords[2] = { x, y };

	if (writesrv(gamesh_sdl_render_surface, coords, sizeof(coords)) < 0)
		return -1;

	int result = -1;
	pollopsrv(handle_render_surface_response, &result, -1);

	return result;
}

static ssize_t get_size(SDL_Surface *surface)
{
	const bool valid = surface->w
		&& surface->h
		&& surface->pitch
		&& surface->pixels;
	if (!valid)
		return -1;
	return surface->h * surface->pitch;
}

static gamesh_shared_buffer_t *create_shared_buffer_from(
	int shmem_fd,
	SDL_Surface *surface
)
{
	ssize_t size = get_size(surface);
	if (size < 0)
		return NULL;

	void *pixels = mmap(
		NULL,
		size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		shmem_fd,
		0
	);
	if (pixels == MAP_FAILED)
		return NULL;

	SDL_Surface *internal_surface = SDL_CreateSurfaceFrom(
		surface->w,
		surface->h,
		surface->format,
		pixels,
		surface->pitch
	);
	if (!internal_surface)
		goto free_pixels;

	gamesh_shared_buffer_t *result = malloc(sizeof(*result));
	if (!result)
		goto free_surface;

	result->shmem_fd = shmem_fd;
	result->surface = internal_surface;
	return result;

free_surface:
	SDL_DestroySurface(internal_surface);
free_pixels:
	munmap(pixels, size);
	return NULL;
}

gamesh_shared_buffer_t *gamesh_create_shared_buffer(SDL_Surface *surface)
{
	ssize_t size = get_size(surface);
	if (size < 0)
		return NULL;

	int shmem_fd = memfd_create("gamesh-shared-buffer", 0);
	if (shmem_fd < 0)
		return NULL;

	if (write(shmem_fd, surface->pixels, size) < 0)
		goto close_fd;

	return create_shared_buffer_from(shmem_fd, surface);

close_fd:
	close(shmem_fd);
	return NULL;
}

void gamesh_destroy_shared_buffer(gamesh_shared_buffer_t *buffer)
{
	if (!buffer)
		return;

	close(buffer->shmem_fd);
	munmap(buffer->surface->pixels, get_size(buffer->surface));
	SDL_DestroySurface(buffer->surface);
	free(buffer);
}

static void add_buffer_response(
	int fd,
	int opcode,
	void *data,
	int length,
	struct msghdr header,
	void *context
)
{
	int *result = context;
	const bool error = opcode != gamesh_sdl_buffer_add
		|| length != sizeof(int);

	if (error) {
		*result = -1;
		return;
	}

	*result = *(int*)data;
}

int gamesh_add_buffer(gamesh_shared_buffer_t *buffer)
{
	if (init_opcodes() < 0)
		return -1;

	if (
		send_fd(
			SRV_FILENO,
			gamesh_sdl_buffer_add,
			buffer->surface,
			sizeof(*buffer->surface),
			buffer->shmem_fd
		) < 0
	) {
		return -1;
	}

	int result = -1;

	pollopsrv(add_buffer_response, &result, -1);

	return result;
}

gamesh_shared_buffer_t *gamesh_recv_shared_buffer(
	int opcode,
	void *buffer,
	int length,
	struct msghdr header
)
{
	int shmem_fd = get_fd(header);

	const bool error = opcode != gamesh_sdl_buffer_add
		|| length < sizeof(SDL_Surface);

	if (error)
		return NULL;

	SDL_Surface *network = buffer;

	return create_shared_buffer_from(
		shmem_fd,
		network
	);
}

SDL_Surface *gamesh_get_shared_buffer_surface(gamesh_shared_buffer_t *buffer)
{
	return buffer->surface;
}

void set_surface_buffer_response(
	int fd,
	int opcode,
	void *data,
	int length,
	struct msghdr header,
	void *context
)
{
	int *result = context;
	if (opcode == gamesh_sdl_buffer_set)
		*result = 0;
}

int gamesh_set_surface_buffer(int surface_id, int buffer_id)
{
	int ids[] = { surface_id, buffer_id };

	if (writesrv(gamesh_sdl_buffer_set, ids, sizeof(ids)) < 0)
		return -1;

	int result = -1;
	pollopsrv(set_surface_buffer_response, &result, -1);
	return result;
}

int gamesh_get_tick_fd(void)
{
	if (init_opcodes() < 0)
		return -1;
	return gamesh_event_listen(gamesh_sdl_event_tick);
}

static void get_tick_nonblocking(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	uint64_t *result = context;
	const bool error = opcode != gamesh_sdl_event_tick
		|| size != sizeof(uint64_t);

	if (error) {
		*result = (uint64_t)-1;
		return;
	}

	*result = *(uint64_t*)buffer;
}

static void get_tick_blocking(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	struct pollfd pollfd = { .fd = fd };
	uint64_t *result = context;
	const bool error = opcode != gamesh_sdl_event_tick
		|| size != sizeof(uint64_t);

	if (error) {
		*result = (uint64_t)-1;
		return;
	}

	*result = *(uint64_t*)buffer;

	do {
		pollfd = pollopfd(pollfd, get_tick_nonblocking, context, 0);
	} while (pollfd.revents & POLLIN);
}

uint64_t gamesh_get_tick(int tick_fd)
{
	uint64_t result = -1;
	struct pollfd pollfd = { .fd = tick_fd };
	pollopfd(pollfd, get_tick_blocking, &result, -1);
	return result;
}

void set_surface_position_response(
	int fd,
	int opcode,
	void *buffer,
	int size,
	struct msghdr header,
	void *context
)
{
	int *result = context;
	if (opcode == gamesh_sdl_render_surface_position)
		*result = 0;
}

int gamesh_set_surface_position(int surface_id, int x, int y)
{
	if (!init_opcodes())
		return -1;

	int result = -1;

	int payload[] = { surface_id, x, y };
	writesrv(
		gamesh_sdl_render_surface_position,
		payload,
		sizeof(payload)
	);

	pollopsrv(set_surface_position_response, &result, -1);

	return result;
}

void gamesh_free_surface(int surface_id)
{
	writesrv(gamesh_sdl_render_surface_free, &surface_id, sizeof(surface_id));
}

void gamesh_free_buffer(int buffer_id)
{
	writesrv(gamesh_sdl_buffer_free, &buffer_id, sizeof(buffer_id));
}
