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

#ifdef SUPPORT_COMMON_PUI

#include "pages.h"
#include <gen/pui_logo.h>

static mp_pui_page_t _mp_pui_about_page;
static mp_pui_big_t _mp_pui_about_title;
static mp_pui_label_t _mp_pui_about_version;
static mp_pui_icon_t _mp_pui_about_logo;
static mp_pui_bar_t _mp_pui_about_rule;

/* boot splash: a rainbow rides the logo block (glyph + wordmark) in place
 * for a short animation, then the normal white about page takes over. */
#define _MP_PUI_ABOUT_LOGO_FRAMES 48
#define _MP_PUI_ABOUT_SPLASH_FADE 14
#define _MP_PUI_ABOUT_SPLASH_SETTLE 10
#define _MP_PUI_ABOUT_TEXT_FADE 14
#define _MP_PUI_ABOUT_FLAG_FADE 14
#define _MP_PUI_ABOUT_SPLASH_FRAMES (_MP_PUI_ABOUT_LOGO_FRAMES + _MP_PUI_ABOUT_TEXT_FADE + _MP_PUI_ABOUT_FLAG_FADE)
#define _MP_PUI_ABOUT_VERSION_H 14
/* the logo block the splash can hold: 160 by 32 units, one bit a pixel */
#define _MP_PUI_ABOUT_LOGO_MAXW MP_PUI_DP(160)
#define _MP_PUI_ABOUT_LOGO_MAXH MP_PUI_DP(32)
#define _MP_PUI_ABOUT_LOGO_BMP (((_MP_PUI_ABOUT_LOGO_MAXW + 7) / 8) * _MP_PUI_ABOUT_LOGO_MAXH)

static int _mp_pui_about_frame;
static int _mp_pui_about_splash_done;
static int _mp_pui_about_logoW;
static int _mp_pui_about_logoH;
static int _mp_pui_about_logoX;
static int _mp_pui_about_logoY;
static int _mp_pui_about_flagY;
static int _mp_pui_about_flagH;
static int _mp_pui_about_swissH;
static int _mp_pui_about_swissX;
static int _mp_pui_about_euX;
static int _mp_pui_about_euW;
static int _mp_pui_about_ruleX;
static int _mp_pui_about_ruleY;
static int _mp_pui_about_ruleW;
static int _mp_pui_about_ruleH;
static int _mp_pui_about_verY;
static int _mp_pui_about_verH;
static char _mp_pui_about_ver[MP_PUI_TEXT];
static unsigned char _mp_pui_about_logoBmp[_MP_PUI_ABOUT_LOGO_BMP];
static mp_task_t *_mp_pui_about_tick_task;

static const char *_mp_pui_version(mp_pui_t *ui) {
	if(ui && ui->kernel && ui->kernel->version && ui->kernel->version[0])
		return(ui->kernel->version);
	return(MP_KERNEL_VERSION);
}

static void _mp_pui_about_hsv(int h, int s, int v, unsigned char *r, unsigned char *g, unsigned char *b) {
	unsigned char p, q, t, region, rem;

	if(v < 0) v = 0;
	if(v > 255) v = 255;
	if(s < 0) s = 0;
	if(s > 255) s = 255;
	region = (unsigned char)(((unsigned)h / 43u) % 6u);
	rem = (unsigned char)(((unsigned)h - (unsigned)region * 43u) * 6u / 256u);
	p = (unsigned char)((unsigned)v * (255u - (unsigned)s) / 255u);
	q = (unsigned char)((unsigned)v * (255u - (unsigned)s * (unsigned)rem / 255u) / 255u);
	t = (unsigned char)((unsigned)v * (255u - (unsigned)s * (255u - (unsigned)rem) / 255u) / 255u);
	switch(region) {
		case 0: *r = (unsigned char)v; *g = t; *b = p; break;
		case 1: *r = q; *g = (unsigned char)v; *b = p; break;
		case 2: *r = p; *g = (unsigned char)v; *b = t; break;
		case 3: *r = p; *g = q; *b = (unsigned char)v; break;
		case 4: *r = t; *g = p; *b = (unsigned char)v; break;
		default: *r = (unsigned char)v; *g = p; *b = q; break;
	}
}

/* Swiss cross on the 32-unit grid: arms 6 wide, span 20, centered. */
static int _mp_pui_about_cross(int x, int y, int side) {
	int u, v;

	if(side < 1)
		return(0);
	u = x * 32 / side;
	v = y * 32 / side;
	if(u > 31)
		u = 31;
	if(v > 31)
		v = 31;
	if(u >= 6 && u <= 25 && v >= 13 && v <= 18)
		return(1);
	if(v >= 6 && v <= 25 && u >= 13 && u <= 18)
		return(1);
	return(0);
}

/* twelve upright stars. The bitmap is a point-up star, 5 by 5. */
static int _mp_pui_about_star(int x, int y, int w, int h) {
	static const signed char ring[12][2] = {
		{ 0, -100 }, { 50, -87 }, { 87, -50 }, { 100, 0 },
		{ 87, 50 }, { 50, 87 }, { 0, 100 }, { -50, 87 },
		{ -87, 50 }, { -100, 0 }, { -87, -50 }, { -50, -87 },
	};
	static const unsigned char bits[5] = { 0x04, 0x0E, 0x1F, 0x0A, 0x11 };
	int radius = h * 2 / 5;
	int i;

	if(radius < 5)
		radius = 5;
	for(i = 0; i < 12; i++) {
		int cx = w / 2 + ring[i][0] * radius / 100;
		int cy = h / 2 + ring[i][1] * radius / 100;
		int lx = x - (cx - 2);
		int ly = y - (cy - 2);

		if(lx >= 0 && ly >= 0 && lx < 5 && ly < 5 && (bits[ly] & (0x10 >> lx)))
			return(1);
	}
	return(0);
}

static void _mp_pui_about_place_flags(int glassW, int glassH) {
	int margin = MP_PUI_DP(8);
	int side = MP_PUI_DP(22);
	int euW;

	if(margin < 2)
		margin = 2;
	if(side < 16)
		side = 16;
	if(side + margin > glassH / 2)
		side = glassH / 2 - margin;
	if(side < 12)
		side = 12;
	euW = side * 3 / 2;
	if(euW + side + margin * 3 > glassW) {
		side = (glassW - margin * 3) * 2 / 5;
		if(side < 12)
			side = 12;
		euW = side * 3 / 2;
	}
	_mp_pui_about_flagH = side;
	/* a square reads taller than a 3:2 flag of the same pixel height */
	_mp_pui_about_swissH = side * 5 / 6;
	if(_mp_pui_about_swissH < 12)
		_mp_pui_about_swissH = 12;
	_mp_pui_about_euW = euW;
	_mp_pui_about_flagY = glassH - side - margin;
	if(_mp_pui_about_flagY < 0)
		_mp_pui_about_flagY = 0;
	_mp_pui_about_swissX = margin;
	_mp_pui_about_euX = glassW - margin - euW;
}

/* logo wave only. After the color run the mark is white. */
static void _mp_pui_about_levels(int *v, int *s) {
	*v = _mp_pui_about_frame < _MP_PUI_ABOUT_SPLASH_FADE
		? (_mp_pui_about_frame + 1) * (255 / _MP_PUI_ABOUT_SPLASH_FADE) : 255;
	if(_mp_pui_about_frame >= _MP_PUI_ABOUT_LOGO_FRAMES - _MP_PUI_ABOUT_SPLASH_SETTLE) {
		int sp = _MP_PUI_ABOUT_LOGO_FRAMES - 1 - _mp_pui_about_frame;

		if(sp < 0)
			sp = 0;
		*s = 255 * sp / _MP_PUI_ABOUT_SPLASH_SETTLE;
	}
	else
		*s = 255;
}

static mp_pui_color_t _mp_pui_about_wave(int x, int y) {
	int hue, v, s;
	unsigned char r, g, b;

	if(_mp_pui_about_frame >= _MP_PUI_ABOUT_LOGO_FRAMES)
		return(MP_PUI_COLOR_ON);
	_mp_pui_about_levels(&v, &s);
	hue = (x * 3 + y * 3 + _mp_pui_about_frame * 6) & 0xFF;
	_mp_pui_about_hsv(hue, s, v, &r, &g, &b);
	return((mp_pui_color_t)(((unsigned)r << 16) | ((unsigned)g << 8) | b));
}

/* 0 before the step, 1..255 while it rises, 255 once it has landed */
static int _mp_pui_about_rise(int start, int len) {
	int t = _mp_pui_about_frame - start;

	if(t < 0)
		return(0);
	if(len < 1)
		len = 1;
	if(t >= len)
		return(255);
	return((t + 1) * 255 / len);
}

static mp_pui_color_t _mp_pui_about_dim(mp_pui_color_t color, int v) {
	unsigned rr, gg, bb;

	if(v <= 0)
		return(MP_PUI_COLOR_OFF);
	if(v >= 255)
		return(color);
	rr = (((unsigned)color >> 16) & 255u) * (unsigned)v / 255u;
	gg = (((unsigned)color >> 8) & 255u) * (unsigned)v / 255u;
	bb = ((unsigned)color & 255u) * (unsigned)v / 255u;
	return((mp_pui_color_t)((rr << 16) | (gg << 8) | bb));
}

/* one row of the two flags: Swiss at the left, Europe at the right */
static void _mp_pui_about_flags(unsigned char *buffer, int bufW, int bpp, int y, int v) {
	int local, x;
	mp_pui_color_t color;

	if(v <= 0 || _mp_pui_about_flagH < 1)
		return;
	if(y < _mp_pui_about_flagY || y >= _mp_pui_about_flagY + _mp_pui_about_flagH)
		return;
	local = y - _mp_pui_about_flagY;
	/* same bottom as Europe; the square is the shorter of the two boxes */
	if(local >= _mp_pui_about_flagH - _mp_pui_about_swissH) {
		int sy = local - (_mp_pui_about_flagH - _mp_pui_about_swissH);

		for(x = 0; x < _mp_pui_about_swissH; x++) {
			int px = _mp_pui_about_swissX + x;

			color = _mp_pui_about_cross(x, sy, _mp_pui_about_swissH) ? MP_PUI_COLOR_ON : (mp_pui_color_t)0x00FF0000u;
			mp_pui_put(buffer, bufW, bpp, px, 0, _mp_pui_about_dim(color, v));
		}
	}
	for(x = 0; x < _mp_pui_about_euW; x++) {
		int px = _mp_pui_about_euX + x;

		color = _mp_pui_about_star(x, local, _mp_pui_about_euW, _mp_pui_about_flagH)
			? (mp_pui_color_t)0x00FFCC00u : (mp_pui_color_t)0x00003399u;
		mp_pui_put(buffer, bufW, bpp, px, 0, _mp_pui_about_dim(color, v));
	}
}

static void _mp_pui_about_rule_row(unsigned char *buffer, int bufW, int bpp, int y, int v) {
	int x;
	mp_pui_color_t color;

	if(v <= 0 || _mp_pui_about_ruleH < 1)
		return;
	if(y < _mp_pui_about_ruleY || y >= _mp_pui_about_ruleY + _mp_pui_about_ruleH)
		return;
	color = _mp_pui_about_dim(MP_PUI_COLOR_ON, v);
	for(x = 0; x < _mp_pui_about_ruleW; x++)
		mp_pui_put(buffer, bufW, bpp, _mp_pui_about_ruleX + x, 0, color);
}

/* the version line, centered like the label */
static void _mp_pui_about_version_row(unsigned char *buffer, int bufW, int bpp, int y, int v) {
	const mp_pui_font_t *font = &mp_pui_font_default;
	const char *cursor;
	int line, offset, row, width, pen;

	if(v <= 0 || _mp_pui_about_verH < 1 || _mp_pui_about_ver[0] == 0)
		return;
	if(y < _mp_pui_about_verY || y >= _mp_pui_about_verY + _mp_pui_about_verH)
		return;
	line = font->lineHeight > 0 ? font->lineHeight : 1;
	offset = _mp_pui_about_verH > line ? (_mp_pui_about_verH - line) / 2 : 0;
	row = (y - _mp_pui_about_verY) - offset;
	if(row < 0 || row >= line)
		return;
	width = mp_pui_text_width(font, _mp_pui_about_ver);
	pen = (bufW - width) / 2;
	if(pen < 0)
		pen = 0;
	cursor = _mp_pui_about_ver;
	while(*cursor && *cursor != '\n' && pen < bufW) {
		uint32_t code = mp_pui_utf8_next(&cursor);
		const mp_pui_glyph_t *glyph = mp_pui_font_glyph(font, code);
		int advance = glyph ? glyph->advance : font->size / 2 + 1;
		int grow, column;

		if(glyph && glyph->w) {
			grow = row - (int)font->baseline - (int)glyph->dy;
			if(grow >= 0 && grow < glyph->h) {
				const uint8_t *bits = font->bitmap + glyph->offset;

				for(column = 0; column < glyph->w; column++) {
					int px = pen + glyph->dx + column;
					unsigned int index;

					if(px < 0 || px >= bufW)
						continue;
					index = (unsigned int)(grow * glyph->w + column);
					if(bits[index >> 3] & (uint8_t)(0x80 >> (index & 7)))
						mp_pui_put(buffer, bufW, bpp, px, 0, _mp_pui_about_dim(MP_PUI_COLOR_ON, v));
				}
			}
		}
		pen += advance;
	}
}

static void _mp_pui_about_span(mp_pui_t *ui, int y0, int y1) {
	if(!ui || y1 < y0)
		return;
	mp_pui_refresh_rows(ui, y0, y1);
}

/* logo rows while the color runs, then the rule and the version, then the flags */
static void _mp_pui_about_mark(mp_pui_t *ui) {
	int frame = _mp_pui_about_frame;

	if(frame < _MP_PUI_ABOUT_LOGO_FRAMES) {
		_mp_pui_about_span(ui, _mp_pui_about_logoY,
			_mp_pui_about_logoY + _mp_pui_about_logoH - 1);
		return;
	}
	if(frame < _MP_PUI_ABOUT_LOGO_FRAMES + _MP_PUI_ABOUT_TEXT_FADE) {
		int y0 = _mp_pui_about_ruleY;
		int y1 = _mp_pui_about_verY + _mp_pui_about_verH - 1;

		if(_mp_pui_about_verY < y0)
			y0 = _mp_pui_about_verY;
		_mp_pui_about_span(ui, y0, y1);
		return;
	}
	_mp_pui_about_span(ui, _mp_pui_about_flagY, _mp_pui_about_flagY + _mp_pui_about_flagH - 1);
}

static void _mp_pui_about_decorate(mp_pui_page_t *page, unsigned char *buffer, int bufW, int bpp, int y) {
	(void)page;
	_mp_pui_about_flags(buffer, bufW, bpp, y, 255);
}

/* the boot splash painter. The logo rides its color, then the rule and the
 * version fade in, then the two flags. */
static void _mp_pui_about_paint(mp_pui_page_t *page, unsigned char *buffer, int bufW, int bpp, int y) {
	int strideBmp = (_mp_pui_about_logoW + 7) / 8;
	int x, lx, ly, lit, text, flags;

	(void)page;
	for(x = 0; x < bufW; x++) {
		lx = x - _mp_pui_about_logoX;
		ly = y - _mp_pui_about_logoY;
		lit = 0;
		if(_mp_pui_about_logoW > 0 && lx >= 0 && lx < _mp_pui_about_logoW && ly >= 0 && ly < _mp_pui_about_logoH)
			if(_mp_pui_about_logoBmp[ly * strideBmp + (lx >> 3)] & (0x80 >> (lx & 7)))
				lit = 1;
		if(!lit) {
			mp_pui_put(buffer, bufW, bpp, x, 0, MP_PUI_COLOR_OFF);
			continue;
		}
		mp_pui_put(buffer, bufW, bpp, x, 0, _mp_pui_about_wave(x, y));
	}
	text = _mp_pui_about_rise(_MP_PUI_ABOUT_LOGO_FRAMES, _MP_PUI_ABOUT_TEXT_FADE);
	flags = _mp_pui_about_rise(_MP_PUI_ABOUT_LOGO_FRAMES + _MP_PUI_ABOUT_TEXT_FADE, _MP_PUI_ABOUT_FLAG_FADE);
	_mp_pui_about_rule_row(buffer, bufW, bpp, y, text);
	_mp_pui_about_version_row(buffer, bufW, bpp, y, text);
	_mp_pui_about_flags(buffer, bufW, bpp, y, flags);
}

static void _mp_pui_about_frame_next(mp_task_t *task);

static MP_TASK(_mp_pui_about_tick) {
	int _mp_pui_key = mp_pui_lock();
	_mp_pui_about_frame_next(task);
	mp_pui_unlock(_mp_pui_key);
}

static void _mp_pui_about_frame_next(mp_task_t *task) {
	mp_pui_page_t *page = task->user;
	mp_pui_t *ui = page->ui;
	unsigned long hz;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	/* the frame on screen has to finish before the next one starts,
	 * otherwise the rows already sent keep the old color */
	if(!ui->task || ui->pending) {
		mp_task_delay(task, 1);
		return;
	}
	/* a key moved the glass elsewhere: the frames would land on that page */
	if(ui->current != page) {
		page->paint = NULL;
		_mp_pui_about_splash_done = 1;
		_mp_pui_about_tick_task = NULL;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	/* 0 restarts the tour's dwell after this band lands, so the tour
	 * cannot cut the animation and the dwell follows its last frame */
	ui->rotateAt = 0;
	_mp_pui_about_frame++;
	if(_mp_pui_about_frame < _MP_PUI_ABOUT_SPLASH_FRAMES) {
		_mp_pui_about_mark(ui);
		hz = mp_clock_tick_hz();
		mp_task_delay(task, hz ? (hz * 30UL) / 1000UL : 30);
		return;
	}
	page->paint = NULL;
	_mp_pui_about_splash_done = 1;
	_mp_pui_about_tick_task = NULL;
	{
		int y1 = _mp_pui_about_verY + _mp_pui_about_verH - 1;
		int bot = _mp_pui_about_flagY + _mp_pui_about_flagH - 1;

		if(bot > y1)
			y1 = bot;
		_mp_pui_about_span(ui, _mp_pui_about_logoY, y1);
	}
	task->signal = MP_TASK_SIG_DEAD;
}

static void _mp_pui_about_build(mp_pui_page_t *page, void *user) {
	const mp_pui_font_t *word = &mp_pui_font_wordmark;
	int glassW = page->ui && page->ui->display ? page->ui->display->width : 128;
	int glassH = page->ui && page->ui->display ? page->ui->display->height : 64;
	int title = word->lineHeight > 0 ? word->lineHeight : 1;
	int textW = mp_pui_text_width(word, "miniPhi");
	int gap = MP_PUI_DP(4);
	int blockW = MP_PUI_LOGO_W + gap + textW;
	int blockH = MP_PUI_LOGO_H > title ? MP_PUI_LOGO_H : title;
	int versionH = MP_PUI_DP(_MP_PUI_ABOUT_VERSION_H);
	int ruleW = MP_PUI_DP(32);
	int versionY = glassH - versionH;
	int x, y, textX, textY, ruleY, ruleH;

	(void)user;
	_mp_pui_about_place_flags(glassW, glassH);
	page->decorate = _mp_pui_about_decorate;
	if(versionY < blockH)
		versionY = glassH > versionH ? glassH - versionH : 0;
	if(blockW > glassW)
		blockW = glassW;
	/* the pair sits in the middle of the glass, above the version */
	x = (glassW - blockW) / 2;
	y = (versionY - blockH) / 2;
	if(x < 0)
		x = 0;
	if(y < 0)
		y = 0;
	textX = x + MP_PUI_LOGO_W + gap;
	/* the stem hangs under the bowl, so the word sits one pixel under the box center */
	textY = y + (MP_PUI_LOGO_H - title) / 2 + 1;
	if(textY < 0)
		textY = 0;
	if(textX >= glassW)
		textX = glassW - 1;
	if(textX + textW > glassW)
		textW = glassW - textX;
	ruleY = y + blockH + MP_PUI_DP(3);
	if(ruleY > versionY - MP_PUI_DP(2))
		ruleY = versionY - MP_PUI_DP(2);

	/* render the logo block (glyph + wordmark) once into a 1-bit bitmap so the
	 * splash painter can sample it row by row — only on the first open */
	if(!_mp_pui_about_splash_done && blockW <= _MP_PUI_ABOUT_LOGO_MAXW && blockH <= _MP_PUI_ABOUT_LOGO_MAXH &&
			(blockW + 7) / 8 * blockH <= _MP_PUI_ABOUT_LOGO_BMP) {
		/* each logo row starts on a byte, like the icon widget reads it */
		int logoStride = (MP_PUI_LOGO_W + 7) / 8;
		int gx, gy;
		memset(_mp_pui_about_logoBmp, 0, sizeof(_mp_pui_about_logoBmp));
		for(gy = 0; gy < MP_PUI_LOGO_H; gy++)
			for(gx = 0; gx < MP_PUI_LOGO_W; gx++) {
				if(mp_pui_logo_bits[gy * logoStride + (gx >> 3)] & (0x80 >> (gx & 7)))
					mp_pui_put(_mp_pui_about_logoBmp, blockW, 1, gx, gy, MP_PUI_COLOR_ON);
			}
		mp_pui_draw_text(_mp_pui_about_logoBmp, blockW, 1, MP_PUI_LOGO_W + gap,
			(blockH - title) / 2 + 1, word, "miniPhi", MP_PUI_COLOR_ON);
		_mp_pui_about_logoW = blockW;
		_mp_pui_about_logoH = blockH;
		_mp_pui_about_logoX = x;
		_mp_pui_about_logoY = y;
	}

	/* the phi and the name share one line, centered */
	mp_pui_icon(&_mp_pui_about_logo, page, x, y, MP_PUI_LOGO_W, MP_PUI_LOGO_H);
	mp_pui_icon_set(&_mp_pui_about_logo, mp_pui_logo_bits, MP_PUI_LOGO_W, MP_PUI_LOGO_H);
	mp_pui_big(&_mp_pui_about_title, page, textX, textY, textW > 0 ? textW : 1, title, "miniPhi");
	mp_pui_big_font(&_mp_pui_about_title, word);

	ruleH = MP_PUI_DP(1);
	if(ruleH < 1)
		ruleH = 1;
	_mp_pui_about_ruleX = (glassW - ruleW) / 2;
	_mp_pui_about_ruleY = ruleY;
	_mp_pui_about_ruleW = ruleW;
	_mp_pui_about_ruleH = ruleH;
	_mp_pui_about_verY = versionY;
	_mp_pui_about_verH = versionH;
	snprintf(_mp_pui_about_ver, sizeof(_mp_pui_about_ver), "v%s", _mp_pui_version(page->ui));
	mp_pui_bar(&_mp_pui_about_rule, page, _mp_pui_about_ruleX, ruleY, ruleW, ruleH, 1);
	mp_pui_bar_set(&_mp_pui_about_rule, 1);
	mp_pui_label(&_mp_pui_about_version, page, 0, versionY, glassW, versionH, _mp_pui_about_ver);
	mp_pui_label_align(&_mp_pui_about_version, MP_PUI_ALIGN_CENTER);

	/* the splash paints the logo in a flowing rainbow, then clears the
	 * painter so the widgets above render as the normal white about page */
	_mp_pui_about_frame = 0;
	page->paint = _mp_pui_about_splash_done ? NULL : _mp_pui_about_paint;
}

mp_ret_t mp_pui_page_about(mp_pui_t *ui) {
	return(mp_pui_page_register(ui, &_mp_pui_about_page, "miniPhi", _mp_pui_about_build, ui));
}

/* start the splash animation; call after mp_pui_attach so the pager renders the frames */
void mp_pui_page_about_start(mp_pui_t *ui) {
	if(_mp_pui_about_tick_task || !ui || !ui->kernel)
		return;
	_mp_pui_about_tick_task = mp_task_create(&ui->kernel->tasks, "pui splash",
		_mp_pui_about_tick, &_mp_pui_about_page, 1);
}

#endif
