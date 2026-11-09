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
#define _MP_PUI_CHARGE_CYAN  ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_CHARGE_GREEN ((mp_pui_color_t)0x0000FF00u)
#define _MP_PUI_CHARGE_AMBER ((mp_pui_color_t)0x00FFB000u)
#define _MP_PUI_CHARGE_RED   ((mp_pui_color_t)0x00FF3030u)
#define _MP_PUI_CHARGE_DIM   ((mp_pui_color_t)0x00303030u)

static mp_pui_page_t _mp_pui_charge_page;
static mp_pui_label_t _mp_pui_charge_caption;
static mp_pui_icon_t _mp_pui_charge_battery;
static mp_pui_big_t _mp_pui_charge_state;
static int _mp_pui_charge_on;

static mp_device_registry_t *_mp_pui_charge_dev;
static mp_device_subscribe_t _mp_pui_charge_sub;
static mp_device_charge_t *_mp_pui_charge_face;

/* a 16x8 battery: body cols 1-13, a tip nub cols 14-15 on rows 3-4 */
static void _mp_pui_charge_glyph(mp_device_charge_state_t st, unsigned char out[16]) {
	int row, col, fill;

	memset(out, 0, 16);
	fill = (st == MP_DEVICE_CHARGE_DONE) ? 11 : (st == MP_DEVICE_CHARGE_CHARGING) ? 6 : 0;
	for(row = 0; row < 8; row++) {
		int edge = (row == 0 || row == 7);
		for(col = 0; col < 16; col++) {
			int on = 0;
			if(col >= 1 && col <= 13 && edge) on = 1;
			else if(col == 1 && !edge) on = 1;
			else if(col == 13 && !edge) on = 1;
			else if(col >= 14 && col <= 15 && row >= 3 && row <= 4) on = 1;
			else if(!edge && col >= 2 && col < 2 + fill) on = 1;
			if(on)
				out[row * 2 + (col >> 3)] |= (unsigned char)(0x80 >> (col & 7));
		}
	}
}

static mp_pui_color_t _mp_pui_charge_color(mp_device_charge_state_t st) {
	if(st == MP_DEVICE_CHARGE_DONE)
		return(_MP_PUI_CHARGE_GREEN);
	if(st == MP_DEVICE_CHARGE_CHARGING)
		return(_MP_PUI_CHARGE_AMBER);
	return(_MP_PUI_CHARGE_RED);
}

static const char *_mp_pui_charge_name(mp_device_charge_state_t st) {
	if(st == MP_DEVICE_CHARGE_CHARGING)
		return("Charging");
	if(st == MP_DEVICE_CHARGE_DONE)
		return("Done");
	return("Waiting");
}

/* (re)bind the first charger if it arrived after the page was built */
static void _mp_pui_charge_bind(mp_kernel_t *kernel) {
	if(_mp_pui_charge_face)
		return;
	if(!_mp_pui_charge_dev)
		_mp_pui_charge_dev = mp_device_registry_find(kernel, MP_DEVICE_CHARGER, 0);
	if(_mp_pui_charge_dev)
		_mp_pui_charge_face = mp_device_charge(_mp_pui_charge_dev, &_mp_pui_charge_sub);
}

static void _mp_pui_charge_refresh(mp_pui_t *ui) {
	mp_device_charge_state_t st;
	static unsigned char glyph[16];
	mp_pui_color_t ink;

	if(ui && ui->kernel)
		_mp_pui_charge_bind(ui->kernel);
	if(!_mp_pui_charge_face || mp_device_charge_read(_mp_pui_charge_face, &st) != TRUE) {
		mp_pui_big_set(&_mp_pui_charge_state, "--");
		mp_pui_big_color(&_mp_pui_charge_state, _MP_PUI_CHARGE_DIM);
		_mp_pui_charge_glyph(MP_DEVICE_CHARGE_WAITING, glyph);
		mp_pui_icon_set(&_mp_pui_charge_battery, glyph, 16, 8);
		mp_pui_icon_color(&_mp_pui_charge_battery, _MP_PUI_CHARGE_DIM);
		return;
	}
	ink = _mp_pui_charge_color(st);
	mp_pui_big_set(&_mp_pui_charge_state, _mp_pui_charge_name(st));
	mp_pui_big_color(&_mp_pui_charge_state, ink);
	_mp_pui_charge_glyph(st, glyph);
	mp_pui_icon_set(&_mp_pui_charge_battery, glyph, 16, 8);
	mp_pui_icon_color(&_mp_pui_charge_battery, ink);
}

static void _mp_pui_charge_build(mp_pui_page_t *page, void *user) {
	mp_pui_t *ui = user;
	mp_kernel_t *kernel = ui ? ui->kernel : NULL;
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int body = _mp_pui_body();
	int title = _mp_pui_title_h();
	int iconY = glassH > 64 ? 22 : 14;
	int stateY = glassH > 64 ? 42 : 30;
	const char *who = "Battery";

	if(kernel)
		_mp_pui_charge_bind(kernel);
	if(_mp_pui_charge_dev && _mp_pui_charge_dev->label)
		who = _mp_pui_charge_dev->label;

	mp_pui_label(&_mp_pui_charge_caption, page, 0, 0, glassW, body, who);
	mp_pui_label_align(&_mp_pui_charge_caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_charge_caption, _MP_PUI_CHARGE_CYAN);

	mp_pui_icon(&_mp_pui_charge_battery, page, glassW / 2 - 8, iconY, 16, 8);
	mp_pui_icon_color(&_mp_pui_charge_battery, _MP_PUI_CHARGE_GREEN);

	mp_pui_big(&_mp_pui_charge_state, page, 0, stateY, glassW, title, "--");
	mp_pui_big_color(&_mp_pui_charge_state, _MP_PUI_CHARGE_GREEN);
	_mp_pui_charge_refresh(ui);
}

mp_ret_t mp_pui_page_charge(mp_pui_t *ui) {
	mp_ret_t ok = mp_pui_page_register(ui, &_mp_pui_charge_page, "miniPhi/Charge", _mp_pui_charge_build, ui);

	if(ok == TRUE)
		_mp_pui_charge_on = 1;
	return(ok);
}

void mp_pui_page_charge_refresh(mp_pui_t *ui) {
	if(!_mp_pui_charge_on || !ui || ui->current != &_mp_pui_charge_page)
		return;
	_mp_pui_charge_refresh(ui);
}

mp_pui_page_t *mp_pui_page_charge_page(void) {
	return(&_mp_pui_charge_page);
}

int mp_pui_page_charge_present(mp_kernel_t *kernel) {
	return(mp_device_registry_find(kernel, MP_DEVICE_CHARGER, 0) != NULL);
}

#endif
