/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM

/**
 * @defgroup mpCommonTerm Terminal codec
 * @ingroup mpCommon
 *
 * RGB to a terminal color index. The 16-color palette is xterm's
 * compiled-in color0..color15; the 6x6x6 cube is computed. Which one
 * the peer gets is a runtime choice in the emitter.
 * @{
 */

/* xterm defaults: 0 black through 15 bright white */
static const unsigned char _mp_term_pal16[16][3] = {
	{  0,   0,   0}, /* 0  black */
	{205,   0,   0}, /* 1  red */
	{  0, 205,   0}, /* 2  green */
	{205, 205,   0}, /* 3  yellow */
	{  0,   0, 238}, /* 4  blue */
	{205,   0, 205}, /* 5  magenta */
	{  0, 205, 205}, /* 6  cyan */
	{229, 229, 229}, /* 7  white */
	{127, 127, 127}, /* 8  bright black */
	{255,   0,   0}, /* 9  bright red */
	{  0, 255,   0}, /* 10 bright green */
	{255, 255,   0}, /* 11 bright yellow */
	{ 92,  92, 255}, /* 12 bright blue */
	{255,   0, 255}, /* 13 bright magenta */
	{  0, 255, 255}, /* 14 bright cyan */
	{255, 255, 255}, /* 15 bright white */
};

/* a tie keeps the lower index */
int mp_term_nearest16(unsigned char r, unsigned char g, unsigned char b) {
	long bestD = 0x7fffffffL;
	int best = 0;
	int i;

	for(i = 0; i < 16; i++) {
		long dr = (long)r - (long)_mp_term_pal16[i][0];
		long dg = (long)g - (long)_mp_term_pal16[i][1];
		long db = (long)b - (long)_mp_term_pal16[i][2];
		long d = dr * dr + dg * dg + db * db;

		if(d < bestD) {
			bestD = d;
			best = i;
		}
	}
	return(best);
}

/* one channel onto the cube steps 0, 95, 135, 175, 215, 255.
 * an exact midpoint goes to the higher step. */
static int _mp_term_cube(unsigned char c) {
	if(c < 48)
		return(0);
	if(c < 115)
		return(1);
	return((c - 35) / 40);
}

int mp_term_nearest256(unsigned char r, unsigned char g, unsigned char b) {
	return(16 + 36 * _mp_term_cube(r) + 6 * _mp_term_cube(g) + _mp_term_cube(b));
}

/** @} */

#endif
