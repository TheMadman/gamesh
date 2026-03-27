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
#ifndef GAMESH_GAMESH
#define GAMESH_GAMESH

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 */

#include <stddef.h>

/**
 * \brief Type definition for sprite IDs, for use with damage/sync
 * 	events.
 */
typedef int gamesh_sprite_id_t;

/**
 * \brief Represents a keyboard event.
 */
struct gamesh_keyboard {
	/**
	 * \brief The tick number that this event was
	 * 	generated on.
	 */
	unsigned long timestamp;

	/**
	 * \brief Represents the physical keycode,
	 * 	independently of the keyboard layout
	 * 	or locale.
	 */
	int code;

	/**
	 * \brief Represents the key pressed, taking
	 * 	keyboard layout/locale into consideration.
	 */
	int key;

	/**
	 * \brief Represents whether this key was pressed
	 * 	or released.
	 */
	int pressed;
};

/**
 * \brief Represents a sprite request.
 */
struct gamesh_sprite {
	/**
	 * \brief The height of the sprite in pixels.
	 */
	size_t height;

	/**
	 * \brief The width of the sprite in pixels.
	 */
	size_t width;

	/**
	 * \brief The colour depth of the sprite in bits.
	 */
	unsigned bit_depth;
};

/**
 * \brief Converts a number between 0.0 and 1.0 to
 * 	its byte representation.
 *
 * Out-of-bounds numbers are capped.
 */
static inline unsigned char gamesh_to_byte(double value)
{
	if (value <= 0.0)
		return 0;
	if (value >= 1.0)
		return 255;
	return (unsigned char)(value * 255);
}

/**
 * \returns A file descriptor for events.
 *
 * The file descriptor is never read by the server.
 */
int gamesh_event_fd(void);

/**
 * \brief Closes the event file descriptor.
 */
void gamesh_event_fd_close(int fd);

/**
 * \brief Creates a sprite buffer that is shared
 * 	with the server.
 *
 * \param fd The file descriptor of the sprite. This
 * 	can be shared memory or a file.
 * \param height The height of the sprite in pixels.
 * \param width The width of the sprite in pixels.
 * \param bit_depth The bit depth of a single pixel.
 *
 * \returns A unique ID for the sprite on success, or
 * 	-1 on failure.
 */
gamesh_sprite_id_t gamesh_sprite(
	int fd,
	size_t height,
	size_t width,
	unsigned bit_depth
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GAMESH_GAMESH
