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

#include "gamesh/fd-manager.h"
#include <assert.h>
#include <stdbool.h>

void test_fd_manager(void)
{
	{
		fd_manager_t *manager = fd_manager(1024);
		assert(manager);

		fd_manager_free(manager);
	}
	{
		fd_manager_t *manager = fd_manager(-1);
		assert(!manager);
	}
}

void test_fd_manager_operations(void)
{
	fd_manager_t *manager = fd_manager(1024);
	assert(manager);

	assert(fd_manager_first(manager) == -1);

	fd_manager_add(manager, 0);
	assert(fd_manager_first(manager) == 0);
	assert(fd_manager_next(manager, 0) == -1);

	fd_manager_add(manager, 4);
	assert(fd_manager_first(manager) == 4);
	assert(fd_manager_next(manager, 4) == 0);

	fd_manager_add(manager, 12);
	assert(fd_manager_first(manager) == 12);
	assert(fd_manager_next(manager, 12) == 4);

	fd_manager_remove(manager, 4);
	assert(fd_manager_first(manager) == 12);
	assert(fd_manager_next(manager, 12) == 0);
	assert(fd_manager_next(manager, 0) == -1);

	fd_manager_remove(manager, 0);
	assert(fd_manager_first(manager) == 12);
	assert(fd_manager_next(manager, 12) == -1);

	fd_manager_remove(manager, 12);
	assert(fd_manager_first(manager) == -1);

	fd_manager_free(manager);
}

int main()
{
	test_fd_manager();
	test_fd_manager_operations();
}
