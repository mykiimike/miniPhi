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

/* accent colors; on mono panels these all collapse to lit pixels */
#define _MP_PUI_CLOCK_CYAN  ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_CLOCK_AMBER ((mp_pui_color_t)0x00FFB000u)
#define _MP_PUI_CLOCK_GREEN ((mp_pui_color_t)0x0000FF00u)
#define _MP_PUI_CLOCK_RED   ((mp_pui_color_t)0x00FF3030u)
#define _MP_PUI_CLOCK_DIM   ((mp_pui_color_t)0x00303030u)

static mp_pui_page_t _mp_pui_clock_page;
static mp_pui_label_t _mp_pui_clock_caption;
static mp_pui_label_t _mp_pui_clock_date;
static mp_pui_big_t _mp_pui_clock_time;
static mp_pui_badge_t _mp_pui_clock_state;
static int _mp_pui_clock_on;

static mp_device_registry_t *_mp_pui_clock_dev;
static mp_device_subscribe_t _mp_pui_clock_sub;
static mp_device_time_t *_mp_pui_clock_face;

/* (re)bind the first clock if it arrived after the page was built */
static void _mp_pui_clock_bind(mp_kernel_t *kernel) {
	if(_mp_pui_clock_face)
		return;
	if(!_mp_pui_clock_dev)
		_mp_pui_clock_dev = mp_device_registry_find(kernel, MP_DEVICE_CLOCK, 0);
	if(_mp_pui_clock_dev)
		_mp_pui_clock_face = mp_device_clock(_mp_pui_clock_dev, &_mp_pui_clock_sub);
}

static void _mp_pui_clock_refresh(mp_pui_t *ui) {
	mp_device_time_t now;
	char line[MP_PUI_TEXT];
	mp_pui_color_t ink;

	if(ui && ui->kernel)
		_mp_pui_clock_bind(ui->kernel);
	if(!_mp_pui_clock_face || mp_device_clock_read(_mp_pui_clock_face, &now) != TRUE) {
		mp_pui_label_set(&_mp_pui_clock_date, "--");
		mp_pui_label_color(&_mp_pui_clock_date, _MP_PUI_CLOCK_DIM);
		mp_pui_big_set(&_mp_pui_clock_time, "--:--:--");
		mp_pui_big_color(&_mp_pui_clock_time, _MP_PUI_CLOCK_DIM);
		mp_pui_badge_set(&_mp_pui_clock_state, "--");
		mp_pui_badge_color(&_mp_pui_clock_state, _MP_PUI_CLOCK_DIM, _MP_PUI_CLOCK_DIM);
		return;
	}
	snprintf(line, sizeof(line), "%04d-%02d-%02d", now.year, now.month, now.day);
	mp_pui_label_set(&_mp_pui_clock_date, line);
	mp_pui_label_color(&_mp_pui_clock_date, _MP_PUI_CLOCK_AMBER);
	snprintf(line, sizeof(line), "%02d:%02d:%02d", now.hour, now.minute, now.second);
	mp_pui_big_set(&_mp_pui_clock_time, line);
	mp_pui_big_color(&_mp_pui_clock_time, _MP_PUI_CLOCK_GREEN);
	ink = now.running == YES ? _MP_PUI_CLOCK_GREEN : _MP_PUI_CLOCK_RED;
	mp_pui_badge_set(&_mp_pui_clock_state, now.running == YES ? "running" : "stopped");
	mp_pui_badge_color(&_mp_pui_clock_state, ink, _MP_PUI_CLOCK_DIM);
}

static void _mp_pui_clock_build(mp_pui_page_t *page, void *user) {
	mp_pui_t *ui = user;
	mp_kernel_t *kernel = ui ? ui->kernel : NULL;
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int body = _mp_pui_body();
	int title = _mp_pui_title_h();
	int tall = _mp_pui_units(glassH) > 64;
	int dateY = MP_PUI_DP(tall ? 16 : 14);
	int timeY = MP_PUI_DP(tall ? 32 : 26);
	int badgeW = MP_PUI_DP(_mp_pui_units(glassW) > 128 ? 72 : 64);
	int badgeY = MP_PUI_DP(tall ? 62 : 50);
	const char *who = "Clock";

	if(kernel)
		_mp_pui_clock_bind(kernel);
	if(_mp_pui_clock_dev && _mp_pui_clock_dev->label)
		who = _mp_pui_clock_dev->label;

	mp_pui_label(&_mp_pui_clock_caption, page, 0, 0, glassW, body, who);
	mp_pui_label_align(&_mp_pui_clock_caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_clock_caption, _MP_PUI_CLOCK_CYAN);

	mp_pui_label(&_mp_pui_clock_date, page, 0, dateY, glassW, body, "--");
	mp_pui_label_align(&_mp_pui_clock_date, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_clock_date, _MP_PUI_CLOCK_AMBER);

	mp_pui_big(&_mp_pui_clock_time, page, 0, timeY, glassW, title, "--:--:--");
	mp_pui_big_color(&_mp_pui_clock_time, _MP_PUI_CLOCK_GREEN);

	mp_pui_badge(&_mp_pui_clock_state, page, (glassW - badgeW) / 2, badgeY, badgeW, body, "--");
	_mp_pui_clock_refresh(ui);
}

mp_ret_t mp_pui_page_clock(mp_pui_t *ui) {
	mp_ret_t ok = mp_pui_page_register(ui, &_mp_pui_clock_page, "miniPhi/Clock", _mp_pui_clock_build, ui);

	if(ok == TRUE)
		_mp_pui_clock_on = 1;
	return(ok);
}

void mp_pui_page_clock_refresh(mp_pui_t *ui) {
	if(!_mp_pui_clock_on || !ui || ui->current != &_mp_pui_clock_page)
		return;
	_mp_pui_clock_refresh(ui);
}

mp_pui_page_t *mp_pui_page_clock_page(void) {
	return(&_mp_pui_clock_page);
}

int mp_pui_page_clock_present(mp_kernel_t *kernel) {
	return(mp_device_registry_find(kernel, MP_DEVICE_CLOCK, 0) != NULL);
}

#endif
