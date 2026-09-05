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
#include <stdint.h>

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
int gamesh_events_listen(int *opcodes, unsigned length);

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
 * \brief Creates a new render surface at the given position.
 *
 * \param x How far the left edge of the surface is positioned
 * 	from the left boundary in pixels. If `x` is negative,
 * 	then the absolute value is how far the right edge is
 * 	positioned from the right boundary in pixels.
 * \param y How far the top edge of the surface is positioned
 * 	from the top bounary in pixels. If `y` is negative,
 * 	then the absolute value is how far the right edge is
 * 	positioned from the right boundary in pixels.
 *
 * \returns -1 on error, a render surface ID on success.
 */
int gamesh_create_render_surface(int x, int y, int w, int h);

/**
 * \brief Repositions an existing surface.
 *
 * \param surface_id The surface_id as returned from
 * 	gamesh_create_render_surface().
 * \param x The new x position.
 * \param y The new y position.
 *
 * \returns 0 on success, -1 on error.
 */
int gamesh_set_surface_position(int surface_id, int x, int y);

/**
 * \brief Frees the given surface.
 */
void gamesh_free_surface(int surface_id);

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
 * \brief Frees the given buffer.
 */
void gamesh_free_buffer(int buffer_id);

/**
 * \brief Sets the given surface's active buffer to the given buffer.
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

/**
 * \brief Returns a file descriptor that provides game
 * 	ticks.
 *
 * \returns a file descriptor returning game tick events,
 * 	or -1 on failure.
 */
int gamesh_get_tick_fd(void);

/**
 * \brief Blocks on an event file descriptor produced
 * 	by gamesh_get_tick_fd().
 *
 * \param tick_fd An event file descriptor returned by
 * 	gamesh_get_tick_fd().
 * \returns A 64-bit integer representing milliseconds
 * 	since runner start, or (uint64_t)-1 on failure.
 */
uint64_t gamesh_get_tick(int tick_fd);

/**
 * \brief A convenience type for creating a single, updatable
 * 	image.
 */
typedef struct gamesh_graphic_s {
	int surface_id;
	int buffer_id;
	gamesh_shared_buffer_t *buffer;
} gamesh_graphic_t;

/**
 * \brief Creates a graphic that can be updated with
 * 	gamesh_graphic_blit() and gamesh_graphic_commit().
 *
 * \param x The `x` position of the resulting graphic, as
 * 	passed to gamesh_create_render_surface().
 * \param y The `y` position of the resulting graphic, as
 * 	passed to gamesh_create_render_surface().
 * \param w The width of the graphic to create.
 * \param h The height of the graphic to create.
 * \param format The SDL pixel format to use for the graphic.
 *
 * \returns On error, a struct with .surface_id == -1,
 * 	.buffer_id == -1, and .buffer = NULL is returned.
 * 	On success, a valid value is returned and can be
 * 	used.
 */
gamesh_graphic_t gamesh_create_graphic(
	int x,
	int y,
	int w,
	int h,
	SDL_PixelFormat format
);

gamesh_graphic_t gamesh_create_graphic_from(
	int x,
	int y,
	int w,
	int h,
	SDL_Surface *surface
);

/**
 * \brief Blit the given canvas with the given brush, where
 * 	the x and y coordinates are the top-left of the brush's
 * 	location.
 *
 * \param canvas The graphic to blit
 * \param brush The prush to blit the canvas with
 * \param x The leftmost coordinate of the brush location
 * \param y The topmost coordinate of the brush location
 *
 * \returns 0 on success, -1 on failure.
 */
inline int gamesh_graphic_blit(
	gamesh_graphic_t canvas,
	SDL_Surface *brush,
	int x,
	int y
)
{
	SDL_Rect dest = {.x = x, .y = y};
	if (!SDL_BlitSurface(
		brush,
		NULL,
		gamesh_get_shared_buffer_surface(canvas.buffer),
		&dest
	))
		return -1;
	return 0;
}

/**
 * \brief Declare all pending changes for rendering.
 *
 * \param graphic The graphic to render.
 *
 * \returns 0 on success, -1 on failure.
 */
inline int gamesh_graphic_commit(gamesh_graphic_t graphic)
{
	if (gamesh_set_surface_buffer(graphic.surface_id, graphic.buffer_id) < 0)
		return -1;
	return 0;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GAMESH_GAMESH
