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

#ifndef _HAVE_MP_TUI_FONT_H
	#define _HAVE_MP_TUI_FONT_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonTUIFont TUI fonts
	 * @ingroup mpCommonTUI
	 *
	 * Bitmap fonts rasterized from TrueType at build time, kept in flash.
	 * A glyph is w x h levels of bpp bits, row after row, its first byte
	 * at offset in the bitmap. dx and dy place its top left corner from
	 * the pen, which sits on the baseline.
	 * @{
	 */

	typedef struct {
		uint32_t offset;
		uint8_t w, h;
		int8_t dx, dy;
		/** 0 when the font has no such character */
		uint8_t advance;
	} mp_tui_glyph_t;

	typedef struct {
		uint16_t first;
		uint16_t count;
		const mp_tui_glyph_t *glyphs;
	} mp_tui_font_range_t;

	typedef struct {
		const char *name;
		/** em size in pixels */
		uint8_t size;
		/** 1 or 4 */
		uint8_t bpp;
		uint8_t lineHeight;
		/** from the top of a line to the baseline */
		uint8_t baseline;
		uint8_t rangeCount;
		const mp_tui_font_range_t *ranges;
		const uint8_t *bitmap;
	} mp_tui_font_t;

	/** the font picked in the configurator, MP_TUI_FONT */
	extern const mp_tui_font_t mp_tui_font_default;

	/** NULL when the font has no glyph for that code point */
	const mp_tui_glyph_t *mp_tui_font_glyph(const mp_tui_font_t *font, uint32_t code);

	/** reads one UTF-8 code point and moves *text past it */
	uint32_t mp_tui_utf8_next(const char **text);

	/** width in pixels of text up to its end or the first newline */
	int mp_tui_text_width(const mp_tui_font_t *font, const char *text);

	/** @} */

#endif
