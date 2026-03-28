/*
 * GameSH - A shell library and runner for sprite-based games
 * Copyright (C) 2025  Marcus Harrison
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
\
#ifndef GAMESH_FD_MANAGER
#define GAMESH_FD_MANAGER

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/**
 * \file
 *
 * This module provides a container interface for file descriptors.
 */

/**
 * \brief The type representing a file descriptor manager object.
 */
typedef struct fd_manager {
	int capacity;
	int first;
	int *fds;
} fd_manager_t;

/**
 * \brief Creates a new file descriptor manager from the given buffer, resetting
 * 	the buffer for a brand new manager.
 *
 * \param fd_buffer An array capable of holding file descriptors.
 * \param fd_capacity The amount of file descriptors the fd_buffer can hold.
 *
 * \returns A manager for the buffer.
 */
fd_manager_t fd_manager(int *fd_buffer, int fd_capacity);

/**
 * \brief Attaches a manager to an existing buffer without reinitializing
 * 	the buffer's values.
 * 
 * \param fd_buffer An array capable of holding file descriptors.
 * \param fd_capacity The amount of file descriptors the fd_buffer can hold.
 *
 * \returns A manager for the buffer.
 */
fd_manager_t fd_manager_manage(int *fd_buffer, int fd_capacity);

/**
 * \brief Sets all the file descriptors in the buffer to -1, ready
 * 	to be used by a manager.
 *
 * \param fd_buffer The buffer to initialize.
 * \param fd_capacity The number of file descriptors the fd_buffer can hold.
 */
void fd_manager_init_buffer(int *fd_buffer, int fd_capacity);

int fd_manager_add(fd_manager_t *manager, int fd);
int fd_manager_first(fd_manager_t *manager);
int fd_manager_next(fd_manager_t *manager, int fd);
void fd_manager_remove(fd_manager_t *manager, int fd);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GAMESH_FD_MANAGER
