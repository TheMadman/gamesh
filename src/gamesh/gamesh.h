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

/**
 * \brief Represents a keyboard event.
 */
struct gamesh_keyboard {
	/**
	 * \brief The tick number that this event was
	 * 	generated on.
	 */
	long timestamp;

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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GAMESH_GAMESH
