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

#define _MP_PUI_GLASS_CYAN  ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_GLASS_WHITE ((mp_pui_color_t)0x00FFFFFFu)
#define _MP_PUI_GLASS_BARS  8

static const mp_pui_color_t _mp_pui_glass_color[_MP_PUI_GLASS_BARS] = {
	(mp_pui_color_t)0x00FF0000u,
	(mp_pui_color_t)0x0000FF00u,
	(mp_pui_color_t)0x000000FFu,
	(mp_pui_color_t)0x0000FFFFu,
	(mp_pui_color_t)0x00FF00FFu,
	(mp_pui_color_t)0x00FFFF00u,
	(mp_pui_color_t)0x00FFFFFFu,
	(mp_pui_color_t)0x00FFB000u,
};

/* one step a second while Glass is on the tour: down, up, and back to full */
static const int _mp_pui_glass_lamp[] = { 20, 40, 60, 80, 100, 80, 60, 40, 20, 100 };

static mp_pui_page_t _mp_pui_glass_page;
static mp_pui_label_t _mp_pui_glass_caption;
static mp_pui_value_t _mp_pui_glass_row[7];
static char _mp_pui_glass_who[16];
static int _mp_pui_glass_on;
static int _mp_pui_glass_dim;
static int _mp_pui_glass_bar_y;
static int _mp_pui_glass_bar_h;
static int _mp_pui_glass_phase;

static void _mp_pui_glass_light(mp_pui_t *ui, int percent) {
	char line[MP_PUI_TEXT];

	if(percent < 0)
		percent = 0;
	if(percent > 100)
		percent = 100;
	if(ui && ui->display && ui->display->backlight)
		ui->display->backlight(ui->display, percent);
	snprintf(line, sizeof(line), "%s %d%%", _mp_pui_glass_who[0] ? _mp_pui_glass_who : "Glass", percent);
	mp_pui_label_set(&_mp_pui_glass_caption, line);
	_mp_pui_glass_dim = percent == 100 ? 0 : 1;
}

static const char *_mp_pui_glass_opt(mp_device_t *device, char *key) {
	char *value;

	if(!device || !device->options)
		return("--");
	value = mp_options_get(device->options, key);
	if(!value || !value[0])
		return("--");
	return(value);
}

/* A thin ribbon under the facts. The bright chip walks, so a stuck glass is obvious. */
static void _mp_pui_glass_decorate(mp_pui_page_t *page, unsigned char *buffer, int bufW, int bpp, int y) {
	int local;
	int margin;
	int span;
	int barW;
	int index;
	int x;

	(void)page;
	if(y < _mp_pui_glass_bar_y || y >= _mp_pui_glass_bar_y + _mp_pui_glass_bar_h)
		return;
	local = y - _mp_pui_glass_bar_y;
	margin = MP_PUI_DP(8);
	span = bufW - margin * 2;
	if(span < _MP_PUI_GLASS_BARS)
		return;
	barW = span / _MP_PUI_GLASS_BARS;
	for(index = 0; index < _MP_PUI_GLASS_BARS; index++) {
		int color = (index + _mp_pui_glass_phase) % _MP_PUI_GLASS_BARS;
		int lit = index == (_mp_pui_glass_phase % _MP_PUI_GLASS_BARS);
		int column;
		int gap = 2;

		if(gap >= barW)
			gap = 1;
		x = margin + index * barW;
		for(column = 0; column < barW - gap; column++) {
			mp_pui_color_t ink = _mp_pui_glass_color[color];

			if(lit && local == 0)
				ink = _MP_PUI_GLASS_WHITE;
			mp_pui_put(buffer, bufW, bpp, x + column, 0, ink);
		}
	}
}

static int _mp_pui_glass_fact(mp_pui_page_t *page, int index, int x, int y, int w, int h, const char *caption, const char *text) {
	mp_pui_value(&_mp_pui_glass_row[index], page, x, y, w, h, caption, text && text[0] ? text : "--");
	return(y + h);
}

static void _mp_pui_glass_build(mp_pui_page_t *page, void *user) {
	mp_pui_t *ui = user;
	mp_device_t *device = mp_device_find(MP_DEVICE_DISPLAY, 0);
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int bpp = ui && ui->display ? ui->display->bpp : 1;
	int body = _mp_pui_body();
	int x = MP_PUI_DP(8);
	int w = glassW - MP_PUI_DP(16);
	int y = 0;
	int row = 0;
	char line[MP_PUI_TEXT];
	char *dma;
	const char *who = device && device->label ? device->label : "Glass";
	const char *driver = device && device->driver && device->driver->name ? device->driver->name : NULL;

	/* one text line each, and a ribbon of whatever is left, at most 8 px */
	_mp_pui_glass_bar_h = glassH - body * 8;
	if(_mp_pui_glass_bar_h > 8)
		_mp_pui_glass_bar_h = 8;
	if(_mp_pui_glass_bar_h < 4)
		_mp_pui_glass_bar_h = 4;

	snprintf(_mp_pui_glass_who, sizeof(_mp_pui_glass_who), "%s", who);
	mp_pui_label(&_mp_pui_glass_caption, page, 0, y, glassW, body, who);
	mp_pui_label_align(&_mp_pui_glass_caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_glass_caption, _MP_PUI_GLASS_CYAN);
	_mp_pui_glass_phase = 0;
	_mp_pui_glass_light(ui, _mp_pui_glass_lamp[0]);
	y += body;

	if(driver && device->driver->version)
		snprintf(line, sizeof(line), "%s %s", driver, device->driver->version);
	else if(driver)
		snprintf(line, sizeof(line), "%s", driver);
	else
		snprintf(line, sizeof(line), "--");
	y = _mp_pui_glass_fact(page, row++, x, y, w, body, "Driver", line);
	y = _mp_pui_glass_fact(page, row++, x, y, w, body, "Model", device ? device->model : NULL);
	y = _mp_pui_glass_fact(page, row++, x, y, w, body, "Glass", _mp_pui_glass_opt(device, "glass"));
	y = _mp_pui_glass_fact(page, row++, x, y, w, body, "Orient", _mp_pui_glass_opt(device, "orientation"));
	snprintf(line, sizeof(line), "%d x %d, %d bit", glassW, glassH, bpp);
	y = _mp_pui_glass_fact(page, row++, x, y, w, body, "Size", line);
	y = _mp_pui_glass_fact(page, row++, x, y, w, body, "PPI", _mp_pui_glass_opt(device, "ppi"));
	dma = device && device->options ? mp_options_get(device->options, "dma") : NULL;
	y = _mp_pui_glass_fact(page, row++, x, y, w, body, "DMA", dma && mp_options_cmp(dma, "true") == TRUE ? "on" : "off");

	_mp_pui_glass_bar_y = y;
	page->decorate = _mp_pui_glass_decorate;
}

mp_ret_t mp_pui_page_glass(mp_pui_t *ui) {
	mp_ret_t ok = mp_pui_page_register(ui, &_mp_pui_glass_page, "miniPhi/Glass", _mp_pui_glass_build, ui);

	if(ok == TRUE)
		_mp_pui_glass_on = 1;
	return(ok);
}

void mp_pui_page_glass_release(mp_pui_t *ui) {
	if(!_mp_pui_glass_dim)
		return;
	if(ui && ui->display && ui->display->backlight)
		ui->display->backlight(ui->display, 100);
	_mp_pui_glass_dim = 0;
}

void mp_pui_page_glass_refresh(mp_pui_t *ui) {
	int step;

	if(!_mp_pui_glass_on || !ui || ui->current != &_mp_pui_glass_page)
		return;
	_mp_pui_glass_phase++;
	step = _mp_pui_glass_phase % (int)(sizeof(_mp_pui_glass_lamp) / sizeof(_mp_pui_glass_lamp[0]));
	_mp_pui_glass_light(ui, _mp_pui_glass_lamp[step]);
	mp_pui_refresh_rows(ui, _mp_pui_glass_bar_y, _mp_pui_glass_bar_y + _mp_pui_glass_bar_h - 1);
}

#endif
