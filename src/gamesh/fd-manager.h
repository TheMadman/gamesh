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
typedef struct fd_manager fd_manager_t;

/**
 * \brief Calculates the size, in bytes, for an appropriate buffer to
 * 	initialize an fd manager.
 *
 * \param capacity The number of file descriptors to store.
 * \returns The size, in bytes, necessary for the store, or <0 on error.
 */
ssize_t fd_manager_size(int capacity);

/**
 * \brief Initializes the given memory buffer for a manager of the
 * 	given capacity.
 *
 * For most applications, consider using fd_manager(), which will
 * automatically allocate the correct amount of memory.
 *
 * It is a memory error to allocate less memory than fd_manager_size()
 * reports for the given capacity.
 *
 * \param manager A pointer to the memory to initialize.
 * \param capacity The number of file descriptors to store.
 *
 * \sa fd_manager()
 */
void fd_manager_init(fd_manager_t *manager, int capacity);

/**
 * \brief Allocates an fd_manager_t for the given capacity.
 *
 * Must be freed with fd_manager_free().
 *
 * \param capacity The number of file descriptors to store.
 * \returns A valid pointer on success or NULL on failure.
 *
 * \sa fd_manager_free()
 */
fd_manager_t *fd_manager(int capacity);


int fd_manager_add(fd_manager_t *manager, int fd);
int fd_manager_first(fd_manager_t *manager);
int fd_manager_next(fd_manager_t *manager, int fd);
void fd_manager_remove(fd_manager_t *manager, int fd);
void fd_manager_free(fd_manager_t *manager);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GAMESH_FD_MANAGER
