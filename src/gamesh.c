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
	OPERATION(gamesh_sdl_render_surface_buffer_add) \
	OPERATION(gamesh_sdl_render_surface_buffer_swap)


#define DECLARE_INT(MESSAGE) int MESSAGE = -1;
MESSAGE_LIST(DECLARE_INT)
#undef DECLARE_INT

struct gamesh_shared_buffer_s {
	int shmem_fd;
	SDL_Surface *surface;
};

typedef struct gamesh_surface_add_s {
	int surface_id;
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

static void create_render_surface(
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

int gamesh_create_render_surface()
{
	if (init_opcodes() < 0)
		return -1;

	if (writesrv(gamesh_sdl_render_surface, NULL, 0) < 0)
		return -1;

	int result = -1;
	pollopsrv(create_render_surface, &result, -1);

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

static void add_surface_buffer_response(
	int fd,
	int opcode,
	void *data,
	int length,
	struct msghdr header,
	void *context
)
{
	int *result = context;
	const bool error = opcode != gamesh_sdl_render_surface_buffer_add
		|| length != sizeof(int);

	if (error) {
		*result = -1;
		return;
	}

	*result = *(int*)data;
}

int gamesh_add_surface_buffer(int surface_id, gamesh_shared_buffer_t *buffer)
{
	if (init_opcodes() < 0)
		return -1;

	gamesh_surface_add_t network_surface = {
		.surface_id = surface_id,
		.sdl_surface = *buffer->surface,
	};

	if (
		send_fd(
			SRV_FILENO,
			gamesh_sdl_render_surface_buffer_add,
			&network_surface,
			sizeof(network_surface),
			buffer->shmem_fd
		) < 0
	) {
		return -1;
	}

	int result = -1;

	pollopsrv(add_surface_buffer_response, &result, -1);

	return result;
}

gamesh_recv_buffer_t gamesh_recv_shared_buffer(
	int opcode,
	void *buffer,
	int length,
	struct msghdr header
)
{
	static const gamesh_recv_buffer_t error_return = {
		.surface_id = -1,
		.buffer = NULL,
	};

	int shmem_fd = get_fd(header);

	const bool error = opcode != gamesh_sdl_render_surface_buffer_add
		|| length < sizeof(gamesh_recv_buffer_t);

	if (error)
		return error_return;

	gamesh_surface_add_t *network = buffer;

	gamesh_shared_buffer_t *result = create_shared_buffer_from(
		shmem_fd,
		&network->sdl_surface
	);

	if (!result)
		return error_return;

	return (gamesh_recv_buffer_t) {
		.surface_id = network->surface_id,
		.buffer = result,
	};
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
	if (opcode == gamesh_sdl_render_surface_buffer_swap)
		*result = 0;
}

int gamesh_set_surface_buffer(int surface_id, int buffer_id)
{
	int ids[] = { surface_id, buffer_id };

	if (writesrv(gamesh_sdl_render_surface_buffer_swap, ids, sizeof(ids)) < 0)
		return -1;

	int result = -1;
	pollopsrv(set_surface_buffer_response, &result, -1);
	return result;
}

