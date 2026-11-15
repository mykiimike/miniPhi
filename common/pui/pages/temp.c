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
#define _MP_PUI_TEMP_CYAN  ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_TEMP_BLUE  ((mp_pui_color_t)0x000080FFu)
#define _MP_PUI_TEMP_GREEN ((mp_pui_color_t)0x0000FF00u)
#define _MP_PUI_TEMP_AMBER ((mp_pui_color_t)0x00FFB000u)
#define _MP_PUI_TEMP_RED   ((mp_pui_color_t)0x00FF3030u)
#define _MP_PUI_TEMP_DIM   ((mp_pui_color_t)0x00303030u)

static mp_pui_page_t _mp_pui_temp_page;
static mp_pui_label_t _mp_pui_temp_caption;
static mp_pui_label_t _mp_pui_temp_unit;
static mp_pui_big_t _mp_pui_temp_value;
static mp_pui_bar_t _mp_pui_temp_bar;
static mp_pui_badge_t _mp_pui_temp_band;
static int _mp_pui_temp_on;

static mp_device_registry_t *_mp_pui_temp_dev;
static mp_device_subscribe_t _mp_pui_temp_sub;
static mp_device_measure_t *_mp_pui_temp_measure;

static int _mp_pui_temp_span(float value, float lo, float hi) {
	if(hi <= lo)
		return(0);
	if(value < lo)
		value = lo;
	if(value > hi)
		value = hi;
	return((int)((value - lo) * 1000.0f / (hi - lo)));
}

static mp_pui_color_t _mp_pui_temp_color(float c) {
	if(c < 20.0f)
		return(_MP_PUI_TEMP_BLUE);
	if(c < 50.0f)
		return(_MP_PUI_TEMP_GREEN);
	if(c < 70.0f)
		return(_MP_PUI_TEMP_AMBER);
	return(_MP_PUI_TEMP_RED);
}

static const char *_mp_pui_temp_band_name(float c) {
	if(c < 20.0f)
		return("cold");
	if(c < 50.0f)
		return("warm");
	if(c < 70.0f)
		return("hot");
	return("crit");
}

/* (re)bind the first temperature device if it arrived after the page was built */
static void _mp_pui_temp_bind(mp_kernel_t *kernel) {
	if(_mp_pui_temp_measure)
		return;
	if(!_mp_pui_temp_dev)
		_mp_pui_temp_dev = mp_device_registry_find(kernel, MP_DEVICE_TEMPERATURE, 0);
	if(_mp_pui_temp_dev)
		_mp_pui_temp_measure = mp_device_measure(_mp_pui_temp_dev, &_mp_pui_temp_sub);
}

static void _mp_pui_temp_refresh(mp_pui_t *ui) {
	float v[3];
	unsigned long age;
	char line[MP_PUI_TEXT];

	if(ui && ui->kernel)
		_mp_pui_temp_bind(ui->kernel);
	if(!_mp_pui_temp_measure || mp_device_measure_read(_mp_pui_temp_measure, v, &age) != TRUE) {
		mp_pui_big_set(&_mp_pui_temp_value, "--");
		mp_pui_bar_set(&_mp_pui_temp_bar, 0);
		mp_pui_bar_color(&_mp_pui_temp_bar, _MP_PUI_TEMP_DIM, _MP_PUI_TEMP_DIM);
		mp_pui_badge_set(&_mp_pui_temp_band, "--");
		mp_pui_badge_color(&_mp_pui_temp_band, _MP_PUI_TEMP_DIM, _MP_PUI_TEMP_DIM);
		return;
	}
	snprintf(line, sizeof(line), "%.1f", v[0]);
	mp_pui_big_set(&_mp_pui_temp_value, line);
	mp_pui_bar_set(&_mp_pui_temp_bar, _mp_pui_temp_span(v[0], 0.0f, 85.0f));
	mp_pui_bar_color(&_mp_pui_temp_bar, _mp_pui_temp_color(v[0]), _MP_PUI_TEMP_DIM);
	mp_pui_badge_set(&_mp_pui_temp_band, _mp_pui_temp_band_name(v[0]));
	mp_pui_badge_color(&_mp_pui_temp_band, _mp_pui_temp_color(v[0]), _MP_PUI_TEMP_DIM);
}

static void _mp_pui_temp_build(mp_pui_page_t *page, void *user) {
	mp_pui_t *ui = user;
	mp_kernel_t *kernel = ui ? ui->kernel : NULL;
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int body = _mp_pui_body();
	int title = _mp_pui_title_h();
	int barW = MP_PUI_DP(_mp_pui_units(glassW) > 128 ? 120 : 96);
	int barY = MP_PUI_DP(_mp_pui_units(glassH) > 64 ? 44 : 42);
	int bandY = barY + MP_PUI_DP(10);
	int bandW = MP_PUI_DP(48);
	const char *who = "Temp";

	if(kernel)
		_mp_pui_temp_bind(kernel);
	if(_mp_pui_temp_dev && _mp_pui_temp_dev->label)
		who = _mp_pui_temp_dev->label;

	mp_pui_label(&_mp_pui_temp_caption, page, 0, 0, glassW, body, who);
	mp_pui_label_align(&_mp_pui_temp_caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_temp_caption, _MP_PUI_TEMP_CYAN);

	mp_pui_big(&_mp_pui_temp_value, page, 0, body + MP_PUI_DP(2), glassW, title, "--");
	mp_pui_big_color(&_mp_pui_temp_value, _MP_PUI_TEMP_GREEN);

	mp_pui_label(&_mp_pui_temp_unit, page, 0, body + MP_PUI_DP(2) + title, glassW, body, "C");
	mp_pui_label_align(&_mp_pui_temp_unit, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_temp_unit, _MP_PUI_TEMP_CYAN);

	mp_pui_bar(&_mp_pui_temp_bar, page, (glassW - barW) / 2, barY, barW, MP_PUI_DP(8), 1000);
	mp_pui_badge(&_mp_pui_temp_band, page, (glassW - bandW) / 2, bandY, bandW, body, "--");
	_mp_pui_temp_refresh(ui);
}

mp_ret_t mp_pui_page_temp(mp_pui_t *ui) {
	mp_ret_t ok = mp_pui_page_register(ui, &_mp_pui_temp_page, "miniPhi/Temp", _mp_pui_temp_build, ui);

	if(ok == TRUE)
		_mp_pui_temp_on = 1;
	return(ok);
}

void mp_pui_page_temp_refresh(mp_pui_t *ui) {
	if(!_mp_pui_temp_on || !ui || ui->current != &_mp_pui_temp_page)
		return;
	_mp_pui_temp_refresh(ui);
}

mp_pui_page_t *mp_pui_page_temp_page(void) {
	return(&_mp_pui_temp_page);
}

int mp_pui_page_temp_present(mp_kernel_t *kernel) {
	return(mp_device_registry_find(kernel, MP_DEVICE_TEMPERATURE, 0) != NULL);
}

#endif
