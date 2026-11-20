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

#include <string.h>
#include <tui/tui.h>
#include "splash.h"

#ifndef MP_KERNEL_VERSION
	#define MP_KERNEL_VERSION "0"
#endif

/* how long the settled splash stays, in ms */
#define _MP_TUI_SPLASH_MS 1500

/* a frame, including the band wakes, should land near 30 fps */
#define _MP_TUI_SPLASH_PERIOD_MS 33
/* value eases in over this many logo frames; saturation eases out over the last */
#define _MP_TUI_SPLASH_FADE 12
#define _MP_TUI_SPLASH_SETTLE 8
/* hue units per frame. Smaller than the PUI step of 6 so the wave drifts */
#define _MP_TUI_SPLASH_HUE_STEP 4

#define _MP_TUI_SWISS_RED MP_TUI_RGB(0xFF, 0x00, 0x00)
#define _MP_TUI_EU_BLUE MP_TUI_RGB(0x00, 0x33, 0x99)
#define _MP_TUI_EU_GOLD MP_TUI_RGB(0xFF, 0xCC, 0x00)
#define _MP_TUI_FLAG_WHITE MP_TUI_RGB(0xFF, 0xFF, 0xFF)

static unsigned long _mp_tui_splash_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz + 999) / 1000 : ms;
	return(ticks ? ticks : 1);
}

static int _mp_tui_splash_max(int a, int b) {
	return(a > b ? a : b);
}

void mp_tui_splash_place(mp_tui_splash_place_t *out, int glassW, int glassH,
		int markW, int markH, int wordW, int wordH, int wordSize, int verW, int verH) {
	int em, gap, hang, under;
	int logoW, logoH, lockW, lockH, lockX;
	int ruleH, ruleW, top, bottom;
	int flagH, swissS, euW, margin, stacked;

	if(!out)
		return;
	memset(out, 0, sizeof(*out));
	if(glassW < 1 || glassH < 1)
		return;
	if(wordH < 0)
		wordH = 0;
	if(wordW < 0)
		wordW = 0;
	if(verH < 0)
		verH = 0;
	if(verW < 0)
		verW = 0;
	if(markW < 0)
		markW = 0;
	if(markH < 0)
		markH = 0;
	em = wordSize > 0 ? wordSize : (wordH > 0 ? wordH : 14);
	gap = mp_tui_splash_gap(em);
	hang = mp_tui_splash_hang(em);
	under = (em * 3 + 7) / 14;
	if(under < 1)
		under = 1;

	logoH = mp_tui_splash_phi(em);
	if(markH > 0 && logoH > markH)
		logoH = markH;
	if(logoH > glassH)
		logoH = glassH;
	if(logoH < 1)
		logoH = 1;
	if(markW > 0 && markH > 0)
		logoW = (int)(((long)logoH * markW) / markH);
	else
		logoW = logoH;
	if(logoW < 1)
		logoW = 1;
	if(logoW > glassW) {
		logoW = glassW;
		if(markW > 0 && markH > 0)
			logoH = (int)(((long)logoW * markH) / markW);
		if(logoH < 1)
			logoH = 1;
	}

	stacked = logoW + gap + wordW > glassW;
	if(stacked) {
		lockW = _mp_tui_splash_max(logoW, wordW);
		lockH = logoH + gap + wordH;
	}
	else {
		lockW = logoW + gap + wordW;
		lockH = _mp_tui_splash_max(logoH, wordH);
	}

	/* the PUI rule is 32 px under a 14 px word, one pixel tall */
	ruleH = 1;
	ruleW = (em * 32 + 7) / 14;
	if(ruleW < 1)
		ruleW = 1;
	if(ruleW > glassW)
		ruleW = glassW;

	/* version pinned to the bottom, the lockup centred in the space above it */
	out->verW = verW;
	out->verH = verH;
	out->verY = glassH - verH;
	if(out->verY < 0)
		out->verY = 0;
	out->verX = (glassW - verW) / 2;
	if(out->verX < 0)
		out->verX = 0;
	top = (out->verY - lockH) / 2;
	if(top < 0)
		top = 0;

	out->stacked = stacked;
	out->logoW = logoW;
	out->logoH = logoH;
	if(!stacked) {
		lockX = (glassW - lockW) / 2;
		if(lockX < 0)
			lockX = 0;
		out->logoX = lockX;
		out->logoY = top;
		if(wordH > logoH)
			out->logoY = top + (wordH - logoH) / 2;
		out->wordX = lockX + logoW + gap;
		/* the stem hangs under the bowl */
		out->wordY = out->logoY + (logoH - wordH) / 2 + hang;
		if(wordH > logoH)
			out->wordY = top + hang;
	}
	else {
		out->logoX = (glassW - logoW) / 2;
		out->logoY = top;
		out->wordX = (glassW - wordW) / 2;
		out->wordY = top + logoH + gap;
	}
	if(out->logoX < 0)
		out->logoX = 0;
	if(out->wordX < 0)
		out->wordX = 0;
	if(out->logoY < 0)
		out->logoY = 0;
	if(out->wordY < 0)
		out->wordY = 0;

	bottom = out->logoY + logoH;
	if(out->wordY + wordH > bottom)
		bottom = out->wordY + wordH;
	out->ruleW = ruleW;
	out->ruleH = ruleH;
	out->ruleX = (glassW - ruleW) / 2;
	if(out->ruleX < 0)
		out->ruleX = 0;
	out->ruleY = bottom + under;
	if(verH > 0 && out->ruleY + ruleH + under > out->verY) {
		out->verY = out->ruleY + ruleH + under;
		if(out->verY + verH > glassH)
			out->verY = glassH - verH;
		if(out->verY < 0)
			out->verY = 0;
	}

	/* PUI corners: margin 8, Europe 3:2, Swiss a sixth shorter so the square
	 * reads the same size. A step under PUI's 22 px, still on that grid. */
	margin = 8;
	flagH = 20;
	if(glassH < 200)
		flagH = 18;
	if(glassH < 140 || glassW < 120)
		flagH = 0;
	swissS = flagH > 0 ? flagH * 5 / 6 : 0;
	euW = flagH > 0 ? flagH * 3 / 2 : 0;
	if(flagH < 1 || swissS < 1 || euW < 1 || margin * 2 + swissS + euW >= glassW)
		return;
	out->euH = flagH;
	out->euW = euW;
	out->swissS = swissS;
	out->swissX = margin;
	out->euX = glassW - margin - euW;
	out->euY = glassH - margin - flagH;
	out->swissY = out->euY + flagH - swissS;
	if(out->euY < out->ruleY + ruleH || out->swissY < 0 || out->euX < out->swissX + swissS) {
		out->euH = 0;
		out->euW = 0;
		out->swissS = 0;
	}
}

static mp_tui_color_t _mp_tui_splash_blend(mp_tui_color_t under, mp_tui_color_t color, unsigned int level) {
	unsigned int rest = 15 - level;
	unsigned int r = ((color >> 11) * level + (under >> 11) * rest + 7) / 15;
	unsigned int g = (((color >> 5) & 0x3F) * level + ((under >> 5) & 0x3F) * rest + 7) / 15;
	unsigned int b = ((color & 0x1F) * level + (under & 0x1F) * rest + 7) / 15;

	if(level >= 15)
		return(color);
	if(!level)
		return(under);
	return((mp_tui_color_t)((r << 11) | (g << 5) | b));
}

/* smoothstep on 0..1000 */
static int _mp_tui_splash_ease(int u) {
	int u2;

	if(u <= 0)
		return(0);
	if(u >= 1000)
		return(1000);
	u2 = u * u / 1000;
	return(u2 * 3 - u2 * u * 2 / 1000);
}

/* 0 before the step, 1000 once t has reached len */
static int _mp_tui_splash_rise(int t, int len) {
	if(t <= 0)
		return(0);
	if(len < 1)
		len = 1;
	if(t >= len)
		return(1000);
	return(_mp_tui_splash_ease(t * 1000 / len));
}

static mp_tui_color_t _mp_tui_splash_mix(mp_tui_color_t under, mp_tui_color_t color, unsigned int level, unsigned int full) {
	unsigned int rest, r, g, b;

	if(!full || !level)
		return(under);
	if(level >= full)
		return(color);
	rest = full - level;
	r = ((color >> 11) * level + (under >> 11) * rest + full / 2) / full;
	g = (((color >> 5) & 0x3F) * level + ((under >> 5) & 0x3F) * rest + full / 2) / full;
	b = ((color & 0x1F) * level + (under & 0x1F) * rest + full / 2) / full;
	return((mp_tui_color_t)((r << 11) | (g << 5) | b));
}

static void _mp_tui_splash_hsv(int h, int s, int v, int *r, int *g, int *b) {
	int region, rem, p, q, t;

	if(v < 0)
		v = 0;
	if(v > 255)
		v = 255;
	if(s < 0)
		s = 0;
	if(s > 255)
		s = 255;
	region = (h / 43) % 6;
	if(region < 0)
		region += 6;
	rem = (h - region * 43) * 6 / 256;
	if(rem < 0)
		rem = 0;
	p = v * (255 - s) / 255;
	q = v * (255 - s * rem / 255) / 255;
	t = v * (255 - s * (255 - rem) / 255) / 255;
	switch(region) {
		case 0: *r = v; *g = t; *b = p; break;
		case 1: *r = q; *g = v; *b = p; break;
		case 2: *r = p; *g = v; *b = t; break;
		case 3: *r = p; *g = q; *b = v; break;
		case 4: *r = t; *g = p; *b = v; break;
		default: *r = v; *g = p; *b = q; break;
	}
}

mp_tui_color_t mp_tui_splash_ink(int frame, int x, int y, mp_tui_color_t background, mp_tui_color_t foreground) {
	int hue, sat, opacity, r, g, b;
	mp_tui_color_t wave;

	if(frame < 0)
		frame = 0;
	if(frame >= MP_TUI_SPLASH_LOGO)
		return(foreground);
	opacity = _mp_tui_splash_rise(frame, _MP_TUI_SPLASH_FADE);
	if(!opacity)
		return(background);
	if(frame >= MP_TUI_SPLASH_LOGO - _MP_TUI_SPLASH_SETTLE)
		sat = _mp_tui_splash_rise(MP_TUI_SPLASH_LOGO - 1 - frame, _MP_TUI_SPLASH_SETTLE) * 255 / 1000;
	else
		sat = 255;
	hue = (x * 3 + y * 3 + frame * _MP_TUI_SPLASH_HUE_STEP) & 255;
	_mp_tui_splash_hsv(hue, 255, 255, &r, &g, &b);
	wave = MP_TUI_RGB(r, g, b);
	wave = _mp_tui_splash_mix(foreground, wave, (unsigned int)sat, 255);
	return(_mp_tui_splash_mix(background, wave, (unsigned int)opacity, 1000));
}

static void _mp_tui_splash_put(mp_tui_draw_t *draw, int x, int y, mp_tui_color_t color) {
	if(x < draw->clip.x || y < draw->clip.y)
		return;
	if(x >= draw->clip.x + draw->clip.w || y >= draw->clip.y + draw->clip.h)
		return;
	draw->pixels[(y - draw->band.y) * draw->band.w + (x - draw->band.x)] = color;
}

static unsigned int _mp_tui_splash_nibble(const mp_tui_bitmap_t *mark, int x, int y) {
	unsigned int index, byte;

	if(!mark->alpha || x < 0 || y < 0 || x >= mark->w || y >= mark->h)
		return(0);
	index = (unsigned int)(y * mark->w + x);
	byte = mark->alpha[index >> 1];
	return((index & 1) ? byte & 0x0F : byte >> 4);
}

/* 1:1 reads the raster; a smaller box averages the nibbles it covers */
static unsigned int _mp_tui_splash_coverage(int x, int y, int w, int h) {
	const mp_tui_bitmap_t *mark = &mp_tui_mark;
	unsigned int x0, x1, y0, y1, sx, sy, sum, n;

	if(w < 1 || h < 1 || mark->w < 1 || mark->h < 1)
		return(0);
	if(w == (int)mark->w && h == (int)mark->h)
		return(_mp_tui_splash_nibble(mark, x, y));
	x0 = (unsigned int)x * mark->w / (unsigned int)w;
	x1 = (unsigned int)(x + 1) * mark->w / (unsigned int)w;
	y0 = (unsigned int)y * mark->h / (unsigned int)h;
	y1 = (unsigned int)(y + 1) * mark->h / (unsigned int)h;
	if(x1 <= x0)
		x1 = x0 + 1;
	if(y1 <= y0)
		y1 = y0 + 1;
	if(x0 >= mark->w)
		x0 = (unsigned int)mark->w - 1;
	if(y0 >= mark->h)
		y0 = (unsigned int)mark->h - 1;
	if(x1 > mark->w)
		x1 = mark->w;
	if(y1 > mark->h)
		y1 = mark->h;
	sum = 0;
	n = 0;
	for(sy=y0; sy<y1; sy++) {
		for(sx=x0; sx<x1; sx++) {
			sum += _mp_tui_splash_nibble(mark, (int)sx, (int)sy);
			n++;
		}
	}
	return(n ? (sum + n / 2) / n : 0);
}

static void _mp_tui_splash_logo(mp_tui_draw_t *draw, const mp_tui_splash_place_t *place, int frame, mp_tui_color_t bg, mp_tui_color_t fg) {
	int x0, y0, x1, y1, px, py;

	if(place->logoW < 1 || place->logoH < 1)
		return;
	x0 = place->logoX > draw->clip.x ? place->logoX : draw->clip.x;
	y0 = place->logoY > draw->clip.y ? place->logoY : draw->clip.y;
	x1 = place->logoX + place->logoW;
	y1 = place->logoY + place->logoH;
	if(x1 > draw->clip.x + draw->clip.w)
		x1 = draw->clip.x + draw->clip.w;
	if(y1 > draw->clip.y + draw->clip.h)
		y1 = draw->clip.y + draw->clip.h;
	for(py=y0; py<y1; py++) {
		mp_tui_color_t *row = draw->pixels + (py - draw->band.y) * draw->band.w - draw->band.x;

		for(px=x0; px<x1; px++) {
			unsigned int level = _mp_tui_splash_coverage(px - place->logoX, py - place->logoY, place->logoW, place->logoH);
			mp_tui_color_t ink;

			if(!level)
				continue;
			ink = mp_tui_splash_ink(frame, px, py, bg, fg);
			if(level >= 15)
				row[px] = ink;
			else
				row[px] = _mp_tui_splash_blend(row[px], ink, level);
		}
	}
}

/* the word shares the wave, so a flat mp_tui_draw_text colour will not do */
static void _mp_tui_splash_glyph(mp_tui_draw_t *draw, const mp_tui_font_t *font, const mp_tui_glyph_t *glyph,
		int x, int y, int frame, mp_tui_color_t bg, mp_tui_color_t fg) {
	int left = x + glyph->dx;
	int top = y + glyph->dy;
	int c0 = draw->clip.x - left, c1 = draw->clip.x + draw->clip.w - left;
	int r0 = draw->clip.y - top, r1 = draw->clip.y + draw->clip.h - top;
	const uint8_t *bits = font->bitmap + glyph->offset;
	int c, r;

	if(c0 < 0)
		c0 = 0;
	if(r0 < 0)
		r0 = 0;
	if(c1 > glyph->w)
		c1 = glyph->w;
	if(r1 > glyph->h)
		r1 = glyph->h;
	for(r=r0; r<r1; r++) {
		mp_tui_color_t *row = draw->pixels + (top + r - draw->band.y) * draw->band.w + left - draw->band.x;

		for(c=c0; c<c1; c++) {
			unsigned int index = (unsigned int)(r * glyph->w + c);
			unsigned int level;
			mp_tui_color_t ink;

			if(font->bpp == 1) {
				if(!(bits[index >> 3] & (uint8_t)(0x80 >> (index & 7))))
					continue;
				level = 15;
			}
			else {
				level = (index & 1) ? bits[index >> 1] & 0x0F : bits[index >> 1] >> 4;
				if(!level)
					continue;
			}
			ink = mp_tui_splash_ink(frame, left + c, top + r, bg, fg);
			if(level >= 15)
				row[c] = ink;
			else
				row[c] = _mp_tui_splash_blend(row[c], ink, level);
		}
	}
}

static void _mp_tui_splash_word(mp_tui_draw_t *draw, const mp_tui_font_t *font, int x, int y,
		const char *text, int frame, mp_tui_color_t bg, mp_tui_color_t fg) {
	const mp_tui_glyph_t *glyph;
	int baseline;

	if(!font || !text || y - 2 >= draw->clip.y + draw->clip.h || y + font->lineHeight + 2 <= draw->clip.y)
		return;
	baseline = y + font->baseline;
	while(*text && *text != '\n') {
		uint32_t code = mp_tui_utf8_next(&text);

		glyph = mp_tui_font_glyph(font, code);
		if(!glyph) {
			x += font->size / 2 + 1;
			continue;
		}
		if(glyph->w && x + glyph->dx < draw->clip.x + draw->clip.w && x + glyph->dx + glyph->w > draw->clip.x)
			_mp_tui_splash_glyph(draw, font, glyph, x, baseline, frame, bg, fg);
		x += glyph->advance;
	}
}

/* sine of a multiple of 18 or 30 degrees, 1024 == 1 */
static int _mp_tui_splash_sin(int deg) {
	int neg = 0;
	int s;

	deg %= 360;
	if(deg < 0)
		deg += 360;
	if(deg > 180) {
		deg = 360 - deg;
		neg = 1;
	}
	if(deg > 90)
		deg = 180 - deg;
	switch(deg) {
		case 0: s = 0; break;
		case 18: s = 316; break;
		case 30: s = 512; break;
		case 36: s = 602; break;
		case 54: s = 828; break;
		case 60: s = 887; break;
		case 72: s = 974; break;
		case 90: s = 1024; break;
		default: s = 0; break;
	}
	return(neg ? -s : s);
}

static int _mp_tui_splash_cos(int deg) {
	return(_mp_tui_splash_sin(deg + 90));
}

/* Swiss cross, 4x4. Arms are 6 of 32 wide and 20 long, centred. */
static int _mp_tui_splash_swiss_cover(int x, int y, int side) {
	int sx, sy, n;

	if(side < 1 || x < 0 || y < 0 || x >= side || y >= side)
		return(0);
	n = 0;
	for(sy=0; sy<4; sy++) {
		for(sx=0; sx<4; sx++) {
			int un = (x * 8 + sx * 2 + 1) * 32;
			int vn = (y * 8 + sy * 2 + 1) * 32;
			int den = side * 8;
			int h = un >= 6 * den && un < 26 * den && vn >= 13 * den && vn < 19 * den;
			int v = vn >= 6 * den && vn < 26 * den && un >= 13 * den && un < 19 * den;

			if(h || v)
				n++;
		}
	}
	return(n);
}

/* upright five-pointed star. Template is in units of the outer radius, 1024 == 1. */
static void _mp_tui_splash_star_shape(int *vx, int *vy) {
	int k;

	for(k=0; k<10; k++) {
		int rad = (k & 1) ? 382 : 1000;
		int ang = k * 36;

		vx[k] = _mp_tui_splash_sin(ang) * rad / 1000;
		vy[k] = -_mp_tui_splash_cos(ang) * rad / 1000;
	}
}

static int _mp_tui_splash_inside(int px, int py, const int *vx, const int *vy) {
	int i, j, n;

	n = 0;
	for(i=0, j=9; i<10; j=i++) {
		int yi = vy[i], yj = vy[j];
		long den, num, x;

		if((yi > py) == (yj > py))
			continue;
		den = (long)yj - yi;
		num = ((long)py - yi) * ((long)vx[j] - vx[i]);
		x = vx[i] + num / den;
		if((long)px < x)
			n++;
	}
	return(n & 1);
}

/* gold samples, 0..16, of one flag pixel. Stars sit on a circle of radius h/3. */
static int _mp_tui_splash_eu_cover(int x, int y, int w, int h) {
	int shapeX[10], shapeY[10];
	int ring = h * 256 / 3;
	int rad = h * 256 / 18;

	/* height/18 is under two pixels below about 40 px, and the points
	 * vanish. Hold the star just over two pixels so twelve upright stars
	 * still read; a taller flag keeps the official radius. */
	if(rad < 560)
		rad = 560;
	int samples = h < 48 ? 8 : 4;
	int step, limit, sx, sy, n, i;

	if(w < 1 || h < 1 || rad < 1 || x < 0 || y < 0 || x >= w || y >= h)
		return(0);
	_mp_tui_splash_star_shape(shapeX, shapeY);
	step = 256 / samples;
	limit = rad + 256;
	n = 0;
	for(sy=0; sy<samples; sy++) {
		for(sx=0; sx<samples; sx++) {
			int px = x * 256 + sx * step + step / 2;
			int py = y * 256 + sy * step + step / 2;

			for(i=0; i<12; i++) {
				int cx = w * 128 + ring * _mp_tui_splash_sin(i * 30) / 1024;
				int cy = h * 128 - ring * _mp_tui_splash_cos(i * 30) / 1024;
				int dx = px - cx;
				int dy = py - cy;
				int vx[10], vy[10], k;

				if(dx < -limit || dx > limit || dy < -limit || dy > limit)
					continue;
				if((long)dx * dx + (long)dy * dy > (long)limit * limit)
					continue;
				for(k=0; k<10; k++) {
					vx[k] = cx + shapeX[k] * rad / 1024;
					vy[k] = cy + shapeY[k] * rad / 1024;
				}
				if(_mp_tui_splash_inside(px, py, vx, vy)) {
					n++;
					break;
				}
			}
		}
	}
	if(samples == 4)
		return(n);
	return(n * 16 / (samples * samples));
}

static void _mp_tui_splash_flags(mp_tui_draw_t *draw, const mp_tui_splash_place_t *place, mp_tui_color_t bg, int opacity) {
	int py;

	if(opacity <= 0 || place->euH < 1 || place->swissS < 1)
		return;
	for(py=draw->band.y; py<draw->band.y + draw->band.h; py++) {
		int x;

		if(py >= place->swissY && py < place->swissY + place->swissS) {
			int sy = py - place->swissY;

			for(x=0; x<place->swissS; x++) {
				int cover = _mp_tui_splash_swiss_cover(x, sy, place->swissS);
				mp_tui_color_t color = _mp_tui_splash_mix(_MP_TUI_SWISS_RED, _MP_TUI_FLAG_WHITE, (unsigned int)cover, 16);

				color = _mp_tui_splash_mix(bg, color, (unsigned int)opacity, 1000);
				_mp_tui_splash_put(draw, place->swissX + x, py, color);
			}
		}
		if(py >= place->euY && py < place->euY + place->euH) {
			int ly = py - place->euY;

			for(x=0; x<place->euW; x++) {
				int cover = _mp_tui_splash_eu_cover(x, ly, place->euW, place->euH);
				mp_tui_color_t color = _mp_tui_splash_mix(_MP_TUI_EU_BLUE, _MP_TUI_EU_GOLD, (unsigned int)cover, 16);

				color = _mp_tui_splash_mix(bg, color, (unsigned int)opacity, 1000);
				_mp_tui_splash_put(draw, place->euX + x, py, color);
			}
		}
	}
}

static void _mp_tui_splash_version(mp_tui_t *ui, char *dst, int cap) {
	const char *ver = MP_KERNEL_VERSION;
	int n = 0;

	if(ui->kernel && ui->kernel->version && ui->kernel->version[0])
		ver = ui->kernel->version;
	if(cap < 2) {
		if(cap > 0)
			dst[0] = 0;
		return;
	}
	dst[n++] = 'v';
	while(*ver && n + 1 < cap)
		dst[n++] = *ver++;
	dst[n] = 0;
}

static void _mp_tui_splash_lock(const mp_tui_splash_place_t *place, int wordH, int *y0, int *y1) {
	int a = place->logoY;
	int b = place->logoY + place->logoH;

	/* a glyph can sit a pixel outside the line box; the later frames must cover it */
	if(wordH > 0) {
		if(place->wordY - 2 < a)
			a = place->wordY - 2;
		if(place->wordY + wordH + 2 > b)
			b = place->wordY + wordH + 2;
	}
	if(b < a)
		b = a;
	*y0 = a;
	*y1 = b;
}

static void _mp_tui_splash_span(const mp_tui_splash_place_t *place, int frame, int wordH, int glassH, int *y0, int *y1) {
	int a, b;

	if(frame <= 0) {
		*y0 = 0;
		*y1 = glassH;
		return;
	}
	if(frame < MP_TUI_SPLASH_LOGO) {
		_mp_tui_splash_lock(place, wordH, y0, y1);
		return;
	}
	if(frame < MP_TUI_SPLASH_LOGO + MP_TUI_SPLASH_TEXT) {
		a = place->ruleY;
		b = place->verY + place->verH;
		if(place->verY < a)
			a = place->verY;
		*y0 = a > 0 ? a : 0;
		*y1 = b;
		return;
	}
	if(place->euH < 1 || place->swissS < 1) {
		*y0 = 0;
		*y1 = 0;
		return;
	}
	a = place->swissY < place->euY ? place->swissY : place->euY;
	b = place->swissY + place->swissS;
	if(place->euY + place->euH > b)
		b = place->euY + place->euH;
	*y0 = a > 0 ? a : 0;
	*y1 = b;
}

static void _mp_tui_splash_paint(mp_tui_t *ui, mp_tui_draw_t *draw, const mp_tui_splash_place_t *place,
		const mp_tui_font_t *word, const mp_tui_font_t *body, const char *version, int frame) {
	mp_tui_color_t bg = ui->theme.background;
	mp_tui_color_t fg = ui->theme.foreground;
	int opacity;

	mp_tui_draw_fill(draw, 0, 0, ui->display->width, ui->display->height, bg);
	if(frame < MP_TUI_SPLASH_LOGO) {
		_mp_tui_splash_logo(draw, place, frame, bg, fg);
		_mp_tui_splash_word(draw, word, place->wordX, place->wordY, "miniPhi", frame, bg, fg);
		return;
	}
	if(frame < MP_TUI_SPLASH_LOGO + MP_TUI_SPLASH_TEXT) {
		mp_tui_color_t ink;

		opacity = _mp_tui_splash_rise(frame - MP_TUI_SPLASH_LOGO, MP_TUI_SPLASH_TEXT);
		ink = _mp_tui_splash_mix(bg, fg, (unsigned int)opacity, 1000);
		mp_tui_draw_fill(draw, place->ruleX, place->ruleY, place->ruleW, place->ruleH, ink);
		if(body && version && version[0])
			mp_tui_draw_text(draw, body, place->verX, place->verY, version, ink);
		return;
	}
	opacity = _mp_tui_splash_rise(frame - MP_TUI_SPLASH_LOGO - MP_TUI_SPLASH_TEXT, MP_TUI_SPLASH_FLAG);
	/* the flag rows share the version's band, and the fill above cleared it */
	if(body && version && version[0])
		mp_tui_draw_text(draw, body, place->verX, place->verY, version, fg);
	_mp_tui_splash_flags(draw, place, bg, opacity);
}

/* rows of this frame already cost a 1 ms wake each, plus the wire time.
 * The pause makes the sum about _MP_TUI_SPLASH_PERIOD_MS on a 40 MHz bus. */
static int _mp_tui_splash_pause(int rows, int width) {
	int bands = (rows + 3) / 4;
	int spi, spent;

	if(bands < 1)
		bands = 1;
	if(width < 1)
		width = 1;
	spi = (int)(((unsigned long)rows * (unsigned long)width * 16UL) / 40000UL);
	spent = (bands - 1) + spi;
	if(_MP_TUI_SPLASH_PERIOD_MS > spent + 1)
		return(_MP_TUI_SPLASH_PERIOD_MS - spent);
	return(1);
}

static int _mp_tui_splash_next(mp_tui_t *ui, int rows) {
	int frame = ui->splashFrame;
	int pause;

	if(ui->display->flush)
		ui->display->flush(ui->display);
	/* the driver left the lamp off so this first frame was not a sweep */
	if(frame == 0 && ui->display->backlight)
		ui->display->backlight(ui->display, 100);
	ui->splashFrame = frame + 1;
	ui->splashRow = 0;
	if(ui->splashFrame >= MP_TUI_SPLASH_FRAMES) {
		ui->splashHold = 1;
		ui->splashUntil = mp_clock_ticks() + _mp_tui_splash_ticks(_MP_TUI_SPLASH_MS);
		if(ui->task)
			ui->task->delay = _mp_tui_splash_ticks(_MP_TUI_SPLASH_MS);
		return(1);
	}
	pause = _mp_tui_splash_pause(rows, ui->display->width);
	if(ui->task)
		ui->task->delay = _mp_tui_splash_ticks((unsigned long)pause);
	else
		mp_clock_delay((int)_mp_tui_splash_ticks((unsigned long)pause));
	return(1);
}

/* move the lockup so its ink, not its advance box, is centred. The shift is
 * the same for every band of a glass, so the scan runs once. */
static int _mp_tui_splash_nudge(mp_tui_splash_place_t *place, int glassW, int inkL, int inkR) {
	int shift;

	if(!place || inkR < inkL)
		return(0);
	shift = (glassW - inkL - inkR - 1) / 2;
	place->logoX += shift;
	place->wordX += shift;
	return(shift);
}

static int _mp_tui_splash_glyph_level(const mp_tui_font_t *font, const mp_tui_glyph_t *glyph, int col, int row) {
	unsigned int index;
	const uint8_t *bits;

	if(!glyph || col < 0 || row < 0 || col >= glyph->w || row >= glyph->h)
		return(0);
	bits = font->bitmap + glyph->offset;
	index = (unsigned int)(row * glyph->w + col);
	if(font->bpp == 1)
		return(bits[index >> 3] & (uint8_t)(0x80 >> (index & 7))) ? 1 : 0;
	return((index & 1) ? bits[index >> 1] & 0x0F : bits[index >> 1] >> 4);
}

static const mp_tui_font_t *_mp_tui_splash_ink_font;
static int _mp_tui_splash_ink_w;
static int _mp_tui_splash_ink_shift;

static void _mp_tui_splash_optically(mp_tui_splash_place_t *place, int glassW, const mp_tui_font_t *font) {
	int left, right, x, y, pen;
	const char *text = "miniPhi";

	if(!place || !font || !font->bitmap || glassW < 1)
		return;
	if(font == _mp_tui_splash_ink_font && glassW == _mp_tui_splash_ink_w) {
		place->logoX += _mp_tui_splash_ink_shift;
		place->wordX += _mp_tui_splash_ink_shift;
		return;
	}
	left = glassW;
	right = -1;
	for(x=0; x<place->logoW; x++) {
		for(y=0; y<place->logoH; y++) {
			if(!_mp_tui_splash_coverage(x, y, place->logoW, place->logoH))
				continue;
			if(place->logoX + x < left)
				left = place->logoX + x;
			if(place->logoX + x > right)
				right = place->logoX + x;
			break;
		}
	}
	pen = place->wordX;
	while(*text && *text != '\n') {
		uint32_t code = mp_tui_utf8_next(&text);
		const mp_tui_glyph_t *glyph = mp_tui_font_glyph(font, code);

		if(!glyph) {
			pen += font->size / 2 + 1;
			continue;
		}
		for(x=0; x<glyph->w; x++) {
			for(y=0; y<glyph->h; y++) {
				int ax;

				if(!_mp_tui_splash_glyph_level(font, glyph, x, y))
					continue;
				ax = pen + glyph->dx + x;
				if(ax < left)
					left = ax;
				if(ax > right)
					right = ax;
				break;
			}
		}
		pen += glyph->advance ? glyph->advance : 1;
	}
	_mp_tui_splash_ink_font = font;
	_mp_tui_splash_ink_w = glassW;
	_mp_tui_splash_ink_shift = _mp_tui_splash_nudge(place, glassW, left, right);
}

int mp_tui_splash_band(mp_tui_t *ui) {
	mp_tui_splash_place_t place;
	mp_tui_draw_t draw;
	const mp_tui_font_t *word = &mp_tui_font_wordmark;
	const mp_tui_font_t *body = ui->theme.font ? ui->theme.font : word;
	char version[24];
	int wordW, rows, limit, y0, y1;

	if(ui->splashHold) {
		unsigned long now = mp_clock_ticks();

		if(now < ui->splashUntil) {
			if(ui->task) {
				ui->task->delay = ui->splashUntil - now;
				return(1);
			}
			mp_clock_delay((int)(ui->splashUntil - now));
		}
		ui->splashHold = 0;
		ui->splash = 3;
		if(ui->task)
			ui->task->delay = 1;
		mp_tui_refresh(ui);
		return(0);
	}

	_mp_tui_splash_version(ui, version, (int)sizeof(version));
	wordW = mp_tui_text_width(word, "miniPhi");
	if(wordW > ui->display->width - 8 && body != word) {
		word = body;
		wordW = mp_tui_text_width(word, "miniPhi");
	}
	mp_tui_splash_place(&place, ui->display->width, ui->display->height,
		mp_tui_mark.w, mp_tui_mark.h, wordW, word->lineHeight, (int)word->size,
		mp_tui_text_width(body, version), body->lineHeight);
	_mp_tui_splash_optically(&place, ui->display->width, word);

	/* frame 0 fills the glass. Later frames repaint only the rows that move. */
	_mp_tui_splash_span(&place, ui->splashFrame, word->lineHeight, ui->display->height, &y0, &y1);
	if(y0 < 0)
		y0 = 0;
	if(y1 > ui->display->height)
		y1 = ui->display->height;
	if(ui->splashRow < y0)
		ui->splashRow = y0;
	limit = y1;
	if(ui->splashRow >= limit)
		return(_mp_tui_splash_next(ui, limit - y0));

	if(ui->task)
		ui->task->delay = 1;

	rows = (int)(ui->bufferSize / (unsigned int)ui->display->width);
	if(rows < 1)
		rows = 1;
	if(rows > limit - ui->splashRow)
		rows = limit - ui->splashRow;
	if(rows < 1)
		return(_mp_tui_splash_next(ui, limit - y0));

	draw.tui = ui;
	draw.pixels = ui->buffer;
	draw.band.x = 0;
	draw.band.y = ui->splashRow;
	draw.band.w = ui->display->width;
	draw.band.h = rows;
	draw.clip = draw.band;

	_mp_tui_splash_paint(ui, &draw, &place, word, body, version, ui->splashFrame);
	ui->display->write(ui->display, 0, ui->splashRow, ui->display->width, rows, ui->buffer);

	ui->splashRow += rows;
	if(ui->splashRow >= limit)
		return(_mp_tui_splash_next(ui, limit - y0));
	return(1);
}

#endif
