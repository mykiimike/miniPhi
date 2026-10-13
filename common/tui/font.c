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

#ifdef SUPPORT_COMMON_TUI

const mp_tui_glyph_t *mp_tui_font_glyph(const mp_tui_font_t *font, uint32_t code) {
	const mp_tui_font_range_t *range;
	const mp_tui_glyph_t *glyph;
	unsigned int a;

	for(a=0; a<font->rangeCount; a++) {
		range = &font->ranges[a];
		if(code < range->first || code >= (uint32_t)range->first + range->count)
			continue;
		glyph = &range->glyphs[code - range->first];
		return(glyph->advance || glyph->w ? glyph : NULL);
	}
	return(NULL);
}

uint32_t mp_tui_utf8_next(const char **text) {
	const unsigned char *p = (const unsigned char *)*text;
	uint32_t code;
	int extra, a;

	if(*p < 0x80) {
		*text += 1;
		return(*p);
	}
	if((*p & 0xE0) == 0xC0) {
		code = *p & 0x1F;
		extra = 1;
	}
	else if((*p & 0xF0) == 0xE0) {
		code = *p & 0x0F;
		extra = 2;
	}
	else if((*p & 0xF8) == 0xF0) {
		code = *p & 0x07;
		extra = 3;
	}
	else {
		*text += 1;
		return(0xFFFD);
	}

	for(a=1; a<=extra; a++) {
		if((p[a] & 0xC0) != 0x80) {
			*text += a;
			return(0xFFFD);
		}
		code = (code << 6) | (p[a] & 0x3F);
	}
	*text += extra + 1;
	return(code);
}

int mp_tui_text_width(const mp_tui_font_t *font, const char *text) {
	const mp_tui_glyph_t *glyph;
	int width = 0;

	while(*text && *text != '\n') {
		glyph = mp_tui_font_glyph(font, mp_tui_utf8_next(&text));
		width += glyph ? glyph->advance : font->size / 2 + 1;
	}
	return(width);
}

#endif
