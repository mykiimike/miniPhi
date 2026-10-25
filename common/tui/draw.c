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

/**
@defgroup mpCommonTUIDraw TUI drawing

@ingroup mpCommonTUI

Everything is cut by draw->clip, which never leaves the band in the
buffer, so a widget can draw its whole area whatever band is current.

@{
*/

void mp_tui_draw_fill(mp_tui_draw_t *draw, int x, int y, int w, int h, mp_tui_color_t color) {
	int x0 = x > draw->clip.x ? x : draw->clip.x;
	int y0 = y > draw->clip.y ? y : draw->clip.y;
	int x1 = x + w < draw->clip.x + draw->clip.w ? x + w : draw->clip.x + draw->clip.w;
	int y1 = y + h < draw->clip.y + draw->clip.h ? y + h : draw->clip.y + draw->clip.h;
	mp_tui_color_t *row;
	int px, py;

	for(py=y0; py<y1; py++) {
		row = draw->pixels + (py - draw->band.y) * draw->band.w - draw->band.x;
		for(px=x0; px<x1; px++)
			row[px] = color;
	}
}

void mp_tui_draw_hline(mp_tui_draw_t *draw, int x, int y, int w, mp_tui_color_t color) {
	mp_tui_draw_fill(draw, x, y, w, 1, color);
}

void mp_tui_draw_vline(mp_tui_draw_t *draw, int x, int y, int h, mp_tui_color_t color) {
	mp_tui_draw_fill(draw, x, y, 1, h, color);
}

void mp_tui_draw_line(mp_tui_draw_t *draw, int x0, int y0, int x1, int y1, mp_tui_color_t color) {
	int dx = x0 < x1 ? x1 - x0 : x0 - x1;
	int dy = y0 < y1 ? y1 - y0 : y0 - y1;
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx - dy;
	int steps = dx + dy;
	int x = x0, y = y0;
	int n;
	mp_tui_rect_t *clip = &draw->clip;

	for(n = 0; n <= steps; n++) {
		if(x >= clip->x && y >= clip->y && x < clip->x + clip->w && y < clip->y + clip->h)
			draw->pixels[(y - draw->band.y) * draw->band.w + (x - draw->band.x)] = color;
		if(x == x1 && y == y1)
			break;
		{
			int e2 = err * 2;

			if(e2 > -dy) {
				err -= dy;
				x += sx;
			}
			if(e2 < dx) {
				err += dx;
				y += sy;
			}
		}
	}
}

void mp_tui_draw_frame(mp_tui_draw_t *draw, int x, int y, int w, int h, mp_tui_color_t color) {
	if(w <= 0 || h <= 0)
		return;
	mp_tui_draw_hline(draw, x, y, w, color);
	mp_tui_draw_hline(draw, x, y + h - 1, w, color);
	mp_tui_draw_vline(draw, x, y + 1, h - 2, color);
	mp_tui_draw_vline(draw, x + w - 1, y + 1, h - 2, color);
}

/* level is 0 (background) to 15 (color) */
static mp_tui_color_t _mp_tui_blend(mp_tui_color_t under, mp_tui_color_t color, unsigned int level) {
	unsigned int rest = 15 - level;
	unsigned int r = ((color >> 11) * level + (under >> 11) * rest + 7) / 15;
	unsigned int g = (((color >> 5) & 0x3F) * level + ((under >> 5) & 0x3F) * rest + 7) / 15;
	unsigned int b = ((color & 0x1F) * level + (under & 0x1F) * rest + 7) / 15;
	return((mp_tui_color_t)((r << 11) | (g << 5) | b));
}

static void _mp_tui_draw_glyph(mp_tui_draw_t *draw, const mp_tui_font_t *font, const mp_tui_glyph_t *glyph, int x, int y, mp_tui_color_t color) {
	int left = x + glyph->dx;
	int top = y + glyph->dy;
	int c0 = draw->clip.x - left, c1 = draw->clip.x + draw->clip.w - left;
	int r0 = draw->clip.y - top, r1 = draw->clip.y + draw->clip.h - top;
	const uint8_t *bits = font->bitmap + glyph->offset;
	mp_tui_color_t *row;
	unsigned int index, level;
	int c, r;

	if(c0 < 0) c0 = 0;
	if(r0 < 0) r0 = 0;
	if(c1 > glyph->w) c1 = glyph->w;
	if(r1 > glyph->h) r1 = glyph->h;

	for(r=r0; r<r1; r++) {
		row = draw->pixels + (top + r - draw->band.y) * draw->band.w + left - draw->band.x;
		for(c=c0; c<c1; c++) {
			index = (unsigned int)(r * glyph->w + c);
			if(font->bpp == 1) {
				if(bits[index >> 3] & (0x80 >> (index & 7)))
					row[c] = color;
				continue;
			}
			level = (index & 1) ? bits[index >> 1] & 0x0F : bits[index >> 1] >> 4;
			if(level == 15)
				row[c] = color;
			else if(level)
				row[c] = _mp_tui_blend(row[c], color, level);
		}
	}
}

int mp_tui_draw_text(mp_tui_draw_t *draw, const mp_tui_font_t *font, int x, int y, const char *text, mp_tui_color_t color) {
	const mp_tui_glyph_t *glyph;
	int baseline = y + font->baseline;
	int box = font->size / 2;
	uint32_t code;

	/* a line entirely out of the band costs only the advances */
	if(y >= draw->clip.y + draw->clip.h || y + font->lineHeight <= draw->clip.y) {
		while(*text && *text != '\n')
			x += (glyph = mp_tui_font_glyph(font, mp_tui_utf8_next(&text))) ? glyph->advance : box + 1;
		return(x);
	}

	while(*text && *text != '\n') {
		code = mp_tui_utf8_next(&text);
		glyph = mp_tui_font_glyph(font, code);
		if(!glyph) {
			/* shows what the font misses instead of skipping it */
			mp_tui_draw_frame(draw, x, baseline - font->baseline * 3 / 4, box, font->baseline * 3 / 4, color);
			x += box + 1;
			continue;
		}
		if(glyph->w && x + glyph->dx < draw->clip.x + draw->clip.w && x + glyph->dx + glyph->w > draw->clip.x)
			_mp_tui_draw_glyph(draw, font, glyph, x, baseline, color);
		x += glyph->advance;
	}
	return(x);
}

void mp_tui_draw_bitmap(mp_tui_draw_t *draw, const mp_tui_bitmap_t *bitmap, int x, int y, int w, int h) {
	int x0 = x > draw->clip.x ? x : draw->clip.x;
	int y0 = y > draw->clip.y ? y : draw->clip.y;
	int x1 = x + w < draw->clip.x + draw->clip.w ? x + w : draw->clip.x + draw->clip.w;
	int y1 = y + h < draw->clip.y + draw->clip.h ? y + h : draw->clip.y + draw->clip.h;
	mp_tui_color_t *row;
	unsigned int index, level;
	int px, py, sx, sy;

	if(w <= 0 || h <= 0)
		return;
	for(py=y0; py<y1; py++) {
		sy = (py - y) * bitmap->h / h;
		row = draw->pixels + (py - draw->band.y) * draw->band.w - draw->band.x;
		for(px=x0; px<x1; px++) {
			sx = (px - x) * bitmap->w / w;
			index = (unsigned int)(sy * bitmap->w + sx);
			level = (index & 1) ? bitmap->alpha[index >> 1] & 0x0F : bitmap->alpha[index >> 1] >> 4;
			if(level == 15)
				row[px] = bitmap->pixels[index];
			else if(level)
				row[px] = _mp_tui_blend(row[px], bitmap->pixels[index], level);
		}
	}
}

void mp_tui_draw_image(mp_tui_draw_t *draw, const mp_tui_image_t *image, int x, int y, int w, int h) {
	int x0 = x > draw->clip.x ? x : draw->clip.x;
	int y0 = y > draw->clip.y ? y : draw->clip.y;
	int x1 = x + w < draw->clip.x + draw->clip.w ? x + w : draw->clip.x + draw->clip.w;
	int y1 = y + h < draw->clip.y + draw->clip.h ? y + h : draw->clip.y + draw->clip.h;
	int rgb565, px, py, sx, sy;
	uint32_t step, col;
	const unsigned char *bytes;
	const mp_tui_color_t *colors;

	if(!image || !image->pixels || !image->w || !image->h || w <= 0 || h <= 0)
		return;
	if(x1 <= x0 || y1 <= y0)
		return;

	rgb565 = image->format == MP_TUI_IMAGE_RGB565;
	bytes = image->pixels;
	colors = image->pixels;
	step = ((uint32_t)image->w << 16) / (uint32_t)w;
	for(py=y0; py<y1; py++) {
		mp_tui_color_t *row = draw->pixels + (py - draw->band.y) * draw->band.w - draw->band.x;
		unsigned int src;

		sy = (int)(((long)(py - y) * (long)image->h) / h);
		if(sy < 0)
			sy = 0;
		if(sy >= (int)image->h)
			sy = (int)image->h - 1;
		src = (unsigned int)sy * image->w;
		col = (uint32_t)(x0 - x) * step;
		for(px=x0; px<x1; px++) {
			sx = (int)(col >> 16);
			if(sx < 0)
				sx = 0;
			if(sx >= (int)image->w)
				sx = (int)image->w - 1;
			if(rgb565)
				row[px] = colors[src + (unsigned int)sx];
			else {
				unsigned char sample = bytes[src + (unsigned int)sx];

				row[px] = MP_TUI_RGB(sample, sample, sample);
			}
			col += step;
		}
	}
}

/**@}*/

#endif
