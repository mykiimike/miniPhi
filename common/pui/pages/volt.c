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
#define _MP_PUI_VOLT_CYAN  ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_VOLT_AMBER ((mp_pui_color_t)0x00FFB000u)
#define _MP_PUI_VOLT_GREEN ((mp_pui_color_t)0x0000FF00u)
#define _MP_PUI_VOLT_RED   ((mp_pui_color_t)0x00FF3030u)
#define _MP_PUI_VOLT_DIM   ((mp_pui_color_t)0x00303030u)

static mp_pui_page_t _mp_pui_volt_page;
static mp_pui_label_t _mp_pui_volt_caption;
static mp_pui_label_t _mp_pui_volt_name;
static mp_pui_big_t _mp_pui_volt_value;
static mp_pui_label_t _mp_pui_volt_span;
static mp_pui_bar_t _mp_pui_volt_bar;
static int _mp_pui_volt_on;

static mp_device_registry_t *_mp_pui_volt_dev;
static mp_device_subscribe_t _mp_pui_volt_sub;
static mp_device_measure_t *_mp_pui_volt_measure;

static int _mp_pui_volt_span_of(float value, float lo, float hi) {
	if(hi <= lo)
		return(0);
	if(value < lo)
		value = lo;
	if(value > hi)
		value = hi;
	return((int)((value - lo) * 1000.0f / (hi - lo)));
}

/* low rail is the alarming end of a 0..5 V scale */
static mp_pui_color_t _mp_pui_volt_color(float v) {
	if(v < 3.0f)
		return(_MP_PUI_VOLT_RED);
	if(v < 4.2f)
		return(_MP_PUI_VOLT_AMBER);
	return(_MP_PUI_VOLT_GREEN);
}

/* (re)bind the first voltage sensor if it arrived after the page was built */
static void _mp_pui_volt_bind(mp_kernel_t *kernel) {
	if(_mp_pui_volt_measure)
		return;
	if(!_mp_pui_volt_dev)
		_mp_pui_volt_dev = mp_device_registry_find(kernel, MP_DEVICE_VOLTAGE, 0);
	if(_mp_pui_volt_dev)
		_mp_pui_volt_measure = mp_device_measure(_mp_pui_volt_dev, &_mp_pui_volt_sub);
}

static void _mp_pui_volt_refresh(mp_pui_t *ui) {
	float v[3];
	unsigned long age;
	char line[MP_PUI_TEXT];
	mp_pui_color_t ink;

	if(ui && ui->kernel)
		_mp_pui_volt_bind(ui->kernel);
	if(!_mp_pui_volt_measure || mp_device_measure_read(_mp_pui_volt_measure, v, &age) != TRUE) {
		mp_pui_big_set(&_mp_pui_volt_value, "--");
		mp_pui_big_color(&_mp_pui_volt_value, _MP_PUI_VOLT_DIM);
		mp_pui_bar_set(&_mp_pui_volt_bar, 0);
		mp_pui_bar_color(&_mp_pui_volt_bar, _MP_PUI_VOLT_DIM, _MP_PUI_VOLT_DIM);
		return;
	}
	snprintf(line, sizeof(line), "%.2f", v[0]);
	ink = _mp_pui_volt_color(v[0]);
	mp_pui_big_set(&_mp_pui_volt_value, line);
	mp_pui_big_color(&_mp_pui_volt_value, ink);
	mp_pui_bar_set(&_mp_pui_volt_bar, _mp_pui_volt_span_of(v[0], 0.0f, 5.0f));
	mp_pui_bar_color(&_mp_pui_volt_bar, ink, _MP_PUI_VOLT_DIM);
}

static void _mp_pui_volt_build(mp_pui_page_t *page, void *user) {
	mp_pui_t *ui = user;
	mp_kernel_t *kernel = ui ? ui->kernel : NULL;
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int body = _mp_pui_body();
	int title = _mp_pui_title_h();
	int nameY = glassH > 64 ? 14 : 12;
	int valueY = glassH > 64 ? 28 : 24;
	int spanY = glassH > 64 ? 48 : 40;
	int barW = glassW > 128 ? 140 : 112;
	int barY = glassH > 64 ? 64 : 54;
	const char *who = "Voltage";

	if(kernel)
		_mp_pui_volt_bind(kernel);
	if(_mp_pui_volt_dev && _mp_pui_volt_dev->label)
		who = _mp_pui_volt_dev->label;

	mp_pui_label(&_mp_pui_volt_caption, page, 0, 0, glassW, body, "Voltage");
	mp_pui_label_align(&_mp_pui_volt_caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_volt_caption, _MP_PUI_VOLT_CYAN);

	mp_pui_label(&_mp_pui_volt_name, page, 0, nameY, glassW, body, who);
	mp_pui_label_align(&_mp_pui_volt_name, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_volt_name, _MP_PUI_VOLT_AMBER);

	mp_pui_big(&_mp_pui_volt_value, page, 0, valueY, glassW, title, "--");
	mp_pui_big_color(&_mp_pui_volt_value, _MP_PUI_VOLT_GREEN);

	mp_pui_label(&_mp_pui_volt_span, page, 0, spanY, glassW, body, "0 .. 5 V");
	mp_pui_label_align(&_mp_pui_volt_span, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_volt_span, _MP_PUI_VOLT_CYAN);

	mp_pui_bar(&_mp_pui_volt_bar, page, (glassW - barW) / 2, barY, barW, 8, 1000);
	_mp_pui_volt_refresh(ui);
}

mp_ret_t mp_pui_page_volt(mp_pui_t *ui) {
	mp_ret_t ok = mp_pui_page_register(ui, &_mp_pui_volt_page, "miniPhi/Volt", _mp_pui_volt_build, ui);

	if(ok == TRUE)
		_mp_pui_volt_on = 1;
	return(ok);
}

void mp_pui_page_volt_refresh(mp_pui_t *ui) {
	if(!_mp_pui_volt_on || !ui || ui->current != &_mp_pui_volt_page)
		return;
	_mp_pui_volt_refresh(ui);
}

mp_pui_page_t *mp_pui_page_volt_page(void) {
	return(&_mp_pui_volt_page);
}

int mp_pui_page_volt_present(mp_kernel_t *kernel) {
	return(mp_device_registry_find(kernel, MP_DEVICE_VOLTAGE, 0) != NULL);
}

#endif
