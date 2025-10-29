#include "gamesh/fd-manager.h"

#include <stdbool.h>
#include <stdlib.h>

// just an int buffer with a free list
struct fd_manager {
	int capacity;
	int first;
	int fds[];
};

ssize_t fd_manager_size(int capacity)
{
	if (capacity <= 0)
		return -1;

	const size_t array_bytes = sizeof(int) * capacity;
	return sizeof(fd_manager_t) + array_bytes;
}

void fd_manager_init(fd_manager_t *manager, int capacity)
{
	manager->first = -1;
	manager->capacity = capacity;
	for (int i = 0; i < capacity; i++)
		manager->fds[i] = -1;
}

fd_manager_t *fd_manager(int capacity)
{
	const ssize_t size = fd_manager_size(capacity);
	if (size < 0)
		return NULL;

	fd_manager_t *const result = malloc(size);
	if (!result)
		return NULL;

	fd_manager_init(result, capacity);

	return result;
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

void fd_manager_free(fd_manager_t *manager)
{
	free(manager);
}
