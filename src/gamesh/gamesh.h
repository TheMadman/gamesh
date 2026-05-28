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

#ifndef GAMESH_GAMESH
#define GAMESH_GAMESH

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 */

#include <stddef.h>
#include <SDL3/SDL.h>

/**
 * \brief This function returns a file descriptor, which will
 * 	receive events with the given opcodes.
 *
 * This version allows a single file descriptor to receive
 * multiple event types.
 *
 * \param opcodesn The number of opcodes to register for this
 * 	event listener.
 * \param opcodes A pointer to an array of opcodes.
 * \returns A file descriptor for receiving the given events.
 */
int gamesh_events_listen(int opcodesn, int *opcodes);

/**
 * \brief This function returns a file descriptor, which will
 * 	receive events with the given opcode.
 *
 * This version returns a file descriptor that only receives
 * a single event type.
 *
 * \param opcode The opcode to register for this event listener.
 * \returns A file descriptor for receiving the given events.
 */
int gamesh_event_listen(int opcode);

/**
 * \brief Closes the event file descriptor.
 */
void gamesh_event_close(int fd);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GAMESH_GAMESH
