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
#define _MP_PUI_ABOUT_SPLASH_FRAMES 48
#define _MP_PUI_ABOUT_SPLASH_FADE 14
#define _MP_PUI_ABOUT_SPLASH_SETTLE 10
#define _MP_PUI_ABOUT_VERSION_H 14
#define _MP_PUI_ABOUT_LOGO_BMP 640

static int _mp_pui_about_frame;
static int _mp_pui_about_splash_done;
static int _mp_pui_about_logoW;
static int _mp_pui_about_logoH;
static int _mp_pui_about_logoX;
static int _mp_pui_about_logoY;
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

/* the boot splash painter: row y of the logo block in a flowing rainbow.
 * Lit pixels come from the 1-bit block bitmap built once; the rest stays off. */
static void _mp_pui_about_paint(mp_pui_page_t *page, unsigned char *buffer, int bufW, int bpp, int y) {
	int strideBmp = (_mp_pui_about_logoW + 7) / 8;
	int x, lx, ly, hue, v, s, lit;
	unsigned char r, g, b;
	mp_pui_color_t color;

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
		hue = (x * 3 + y * 3 + _mp_pui_about_frame * 6) & 0xFF;
		v = _mp_pui_about_frame < _MP_PUI_ABOUT_SPLASH_FADE
			? (_mp_pui_about_frame + 1) * (255 / _MP_PUI_ABOUT_SPLASH_FADE) : 255;
		if(_mp_pui_about_frame >= _MP_PUI_ABOUT_SPLASH_FRAMES - _MP_PUI_ABOUT_SPLASH_SETTLE) {
			int sp = _MP_PUI_ABOUT_SPLASH_FRAMES - 1 - _mp_pui_about_frame;
			s = 255 * sp / _MP_PUI_ABOUT_SPLASH_SETTLE;
		}
		else
			s = 255;
		_mp_pui_about_hsv(hue, s, v, &r, &g, &b);
		color = (mp_pui_color_t)(((unsigned)r << 16) | ((unsigned)g << 8) | b);
		mp_pui_put(buffer, bufW, bpp, x, 0, color);
	}
}

static MP_TASK(_mp_pui_about_tick) {
	mp_pui_page_t *page = task->user;
	mp_pui_t *ui = page->ui;
	unsigned long hz;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(!ui->task) {
		mp_task_delay(task, 1);
		return;
	}
	_mp_pui_about_frame++;
	if(_mp_pui_about_frame < _MP_PUI_ABOUT_SPLASH_FRAMES) {
		mp_pui_refresh_rows(ui, _mp_pui_about_logoY, _mp_pui_about_logoY + _mp_pui_about_logoH - 1);
		hz = mp_clock_tick_hz();
		mp_task_delay(task, hz ? (hz * 30UL) / 1000UL : 30);
		return;
	}
	page->paint = NULL;
	_mp_pui_about_splash_done = 1;
	_mp_pui_about_tick_task = NULL;
	mp_pui_refresh(ui);
	task->signal = MP_TASK_SIG_DEAD;
}

static void _mp_pui_about_build(mp_pui_page_t *page, void *user) {
	const mp_pui_font_t *word = &mp_pui_font_wordmark;
	int glassW = page->ui && page->ui->display ? page->ui->display->width : 128;
	int glassH = page->ui && page->ui->display ? page->ui->display->height : 64;
	int title = word->lineHeight > 0 ? word->lineHeight : 1;
	int textW = mp_pui_text_width(word, "miniPhi");
	int gap = 4;
	int blockW = MP_PUI_LOGO_W + gap + textW;
	int blockH = MP_PUI_LOGO_H > title ? MP_PUI_LOGO_H : title;
	int versionH = 14;
	int versionY = glassH - versionH;
	int x, y, textX, textY, ruleY;
	char line[MP_PUI_TEXT];

	(void)user;
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
	ruleY = y + blockH + 3;
	if(ruleY > versionY - 2)
		ruleY = versionY - 2;

	/* render the logo block (glyph + wordmark) once into a 1-bit bitmap so the
	 * splash painter can sample it row by row — only on the first open */
	if(!_mp_pui_about_splash_done && blockW <= 160 && blockH <= 32 && (blockW + 7) / 8 * blockH <= _MP_PUI_ABOUT_LOGO_BMP) {
		int gx, gy, bit;
		memset(_mp_pui_about_logoBmp, 0, sizeof(_mp_pui_about_logoBmp));
		for(gy = 0; gy < MP_PUI_LOGO_H; gy++)
			for(gx = 0; gx < MP_PUI_LOGO_W; gx++) {
				bit = gy * MP_PUI_LOGO_W + gx;
				if(mp_pui_logo_bits[bit >> 3] & (0x80 >> (bit & 7)))
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

	mp_pui_bar(&_mp_pui_about_rule, page, (glassW - 32) / 2, ruleY, 32, 1, 1);
	mp_pui_bar_set(&_mp_pui_about_rule, 1);
	snprintf(line, sizeof(line), "v%s", _mp_pui_version(page->ui));
	mp_pui_label(&_mp_pui_about_version, page, 0, versionY, glassW, versionH, line);
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
