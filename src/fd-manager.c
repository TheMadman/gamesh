#include "gamesh/fd-manager.h"

#include <stdbool.h>
#include <stdlib.h>

// just an int buffer with a free list
struct fd_manager {
	int capacity;
	int first;
	int fds[];
};

fd_manager_t *fd_manager(int capacity)
{
	if (capacity <= 0)
		return NULL;

	const size_t
		array_bytes = sizeof(int) * capacity,
		total = sizeof(fd_manager_t) + array_bytes;

	fd_manager_t *const result = malloc(total);
	if (!result)
		return NULL;

	result->first = -1;
	result->capacity = capacity;
	for (int i = 0; i < capacity; i++)
		result->fds[i] = -1;
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
