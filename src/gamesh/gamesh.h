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
#include <sys/socket.h>

typedef struct gamesh_shared_buffer_s gamesh_shared_buffer_t;

/**
 * \brief This function returns a file descriptor, which will
 * 	receive events with the given opcodes.
 *
 * This version allows a single file descriptor to receive
 * multiple event types.
 *
 * \param opcodes A pointer to an array of opcodes.
 * \param opcodesn The number of opcodes to register for this
 * 	event listener.
 * \returns A file descriptor for receiving the given events.
 */
int gamesh_events_listen(int *opcodes, int length);

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

/**
 * \brief Creates a new render surface.
 *
 * \returns -1 on error, a render surface ID on success.
 */
int gamesh_create_render_surface();

/**
 * \brief Creates a new buffer, ready for sharing.
 *
 * \param surface The original surface to create a shared surface from.
 * 	The pixel data from this surface is copied into the shared surface.
 * 	Freeing the shared surface does not free this surface.
 * 	It is safe to free this surface and continue using the shared surface.
 *
 * \returns A surface that is ready to be shared with other processes, or
 * 	NULL if an error occurred.
 */
gamesh_shared_buffer_t *gamesh_create_shared_buffer(SDL_Surface *surface);

/**
 * \brief Gets the SDL_Surface contained in a gamesh_shared_buffer_t.
 *
 * The SDL_Surface is managed by the gamesh_shared_buffer_t and should not
 * be destroyed. To destroy the allocated resources, use
 * gamesh_destroy_shared_buffer() on the gamesh_shared_buffer_t object.
 *
 * Once the gamesh_shared_buffer_t object is destroyed, its SDL_Surface pointers
 * become invalid and must not be used.
 *
 * \param buffer The buffer to get the surface for.
 * \returns The SDL_Surface associated with the given buffer.
 */
SDL_Surface *gamesh_get_shared_buffer_surface(gamesh_shared_buffer_t *buffer);

/**
 * \brief Destroys a shared buffer created with gamesh_create_shared_buffer().
 *
 * Using the buffer after destroying it is an error.
 *
 * \param buffer The buffer to destroy.
 */
void gamesh_destroy_shared_buffer(gamesh_shared_buffer_t *buffer);

/**
 * \brief Adds a shared buffer to a given surface.
 *
 * \param surface_id An ID returned by gamesh_create_render_surface().
 * \param buffer A shared buffer to add to the surface.
 *
 * \returns A buffer_id that is unique for this surface_id, or -1 on
 * 	failure.
 */
int gamesh_add_buffer(gamesh_shared_buffer_t *buffer);

/**
 * \brief Sets the buffer to use as the current active buffer.
 *
 * \param surface_id An ID returned by gamesh_create_render_surface().
 * \param buffer An ID returned by gamesh_add_buffer().
 *
 * \returns 0 on success, -1 on failure.
 */
int gamesh_set_surface_buffer(int surface_id, int buffer_id);

/**
 * \brief Take data from a pollop() callback and returns a
 * 	`gamesh_shared_buffer_t *`.
 *
 * \param opcode The opcode received from pollop().
 * \param buffer The buffer received from pollop().
 * \param length The length received from pollop().
 * \param header The header received from pollop().
 *
 * \returns If valid, this function returns a `gamesh_shared_buffer_t *`,
 * 	or NULL on failure.
 */
gamesh_shared_buffer_t *gamesh_recv_shared_buffer(
	int opcode,
	void *buffer,
	int length,
	struct msghdr header
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GAMESH_GAMESH
