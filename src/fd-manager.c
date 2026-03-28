#include "gamesh/fd-manager.h"

#include <stdbool.h>
#include <stdlib.h>

#define ssizeof (ssize_t)sizeof

fd_manager_t fd_manager_manage(int *fds, int capacity)
{
	return (fd_manager_t) {
		capacity,
		-1,
		fds,
	};
}

void fd_manager_init_buffer(int *fds, int capacity)
{
	for (int *end = &fds[capacity]; fds < end; fds++)
		*fds = -1;
}

fd_manager_t fd_manager(int *fds, int capacity)
{
	fd_manager_init_buffer(fds, capacity);
	return fd_manager_manage(fds, capacity);
}

static bool error(fd_manager_t *manager, int fd)
{
	return fd < 0
		|| fd > manager->capacity;
}

int fd_manager_add(fd_manager_t *manager, int fd)
{
	if (error(manager, fd))
		return -1;

	manager->fds[fd] = manager->first;
	manager->first = fd;
	return fd;
}

int fd_manager_first(fd_manager_t *manager)
{
	return manager->first;
}

int fd_manager_next(fd_manager_t *manager, int fd)
{
	if (error(manager, fd))
		return -1;
	return manager->fds[fd];
}

void fd_manager_remove(fd_manager_t *manager, int fd)
{
	if (error(manager, fd))
		return;

	const int next = fd_manager_next(manager, fd);

	int *current;
	for (current = &manager->first; *current != fd; current = &manager->fds[*current]) {
		if (*current < 0)
			return;
	}

	*current = next;
	manager->fds[fd] = -1;
}

