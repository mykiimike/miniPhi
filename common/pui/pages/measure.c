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
#define _MP_PUI_MEASURE_CYAN  ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_MEASURE_GREEN ((mp_pui_color_t)0x0000FF00u)
#define _MP_PUI_MEASURE_AMBER ((mp_pui_color_t)0x00FFB000u)
#define _MP_PUI_MEASURE_RED   ((mp_pui_color_t)0x00FF3030u)
#define _MP_PUI_MEASURE_DIM   ((mp_pui_color_t)0x00303030u)

static mp_pui_page_t _mp_pui_measure_page;
static mp_pui_label_t _mp_pui_measure_caption;
static mp_pui_label_t _mp_pui_measure_unit;
static mp_pui_big_t _mp_pui_measure_value;
static mp_pui_bar_t _mp_pui_measure_bar;
static mp_pui_value_t _mp_pui_measure_x;
static mp_pui_value_t _mp_pui_measure_y;
static mp_pui_value_t _mp_pui_measure_z;
static int _mp_pui_measure_on;
static int _mp_pui_measure_tri;

static mp_device_registry_t *_mp_pui_measure_dev;
static mp_device_subscribe_t _mp_pui_measure_sub;
static mp_device_measure_t *_mp_pui_measure_face;
static mp_device_class_t _mp_pui_measure_cls;

static int _mp_pui_measure_span(float value, float lo, float hi) {
	if(hi <= lo)
		return(0);
	if(value < lo)
		value = lo;
	if(value > hi)
		value = hi;
	return((int)((value - lo) * 1000.0f / (hi - lo)));
}

static void _mp_pui_measure_range(mp_device_class_t cls, float *lo, float *hi) {
	static const struct { mp_device_class_t cls; float lo, hi; } r[] = {
		{ MP_DEVICE_CURRENT, 0, 3 }, { MP_DEVICE_BAROMETER, 950, 1050 },
		{ MP_DEVICE_ALTIMETER, 0, 2000 }, { MP_DEVICE_POWER, 0, 100 },
		{ MP_DEVICE_HUMIDITY, 0, 100 }, { MP_DEVICE_ILLUMINANCE, 0, 1000 },
	};
	int n;

	*lo = 0; *hi = 1;
	for(n = 0; n < (int)(sizeof(r) / sizeof(r[0])); n++) {
		if(r[n].cls == cls) {
			*lo = r[n].lo; *hi = r[n].hi;
			return;
		}
	}
}

/* where the reading sits in its scale: low, middle, high */
static mp_pui_color_t _mp_pui_measure_level(float value, float lo, float hi) {
	float t;

	if(hi <= lo)
		return(_MP_PUI_MEASURE_GREEN);
	t = (value - lo) / (hi - lo);
	if(t < 0.34f)
		return(_MP_PUI_MEASURE_GREEN);
	if(t < 0.67f)
		return(_MP_PUI_MEASURE_AMBER);
	return(_MP_PUI_MEASURE_RED);
}

static const char *_mp_pui_measure_name(mp_device_class_t cls) {
	static const struct { mp_device_class_t cls; const char *name; } names[] = {
		{ MP_DEVICE_CURRENT, "Curr" }, { MP_DEVICE_BAROMETER, "Baro" },
		{ MP_DEVICE_ALTIMETER, "Alt" }, { MP_DEVICE_POWER, "Power" },
		{ MP_DEVICE_HUMIDITY, "Humid" }, { MP_DEVICE_ILLUMINANCE, "Lux" },
		{ MP_DEVICE_ACCELEROMETER, "Accel" }, { MP_DEVICE_GYROSCOPE, "Gyro" },
		{ MP_DEVICE_MAGNETOMETER, "Mag" }, { MP_DEVICE_ORIENTATION, "Orient" },
	};
	int n;

	for(n = 0; n < (int)(sizeof(names) / sizeof(names[0])); n++) {
		if(names[n].cls == cls)
			return(names[n].name);
	}
	return("Sensor");
}

/* first measure that is not temperature and not voltage; those have their own pages */
static void _mp_pui_measure_bind(mp_kernel_t *kernel) {
	mp_device_registry_t *dev;

	if(_mp_pui_measure_face)
		return;
	if(!_mp_pui_measure_dev) {
		dev = NULL;
		while((dev = mp_device_measure_next(kernel, dev)) != NULL) {
			if(dev->class == MP_DEVICE_TEMPERATURE || dev->class == MP_DEVICE_VOLTAGE)
				continue;
			_mp_pui_measure_dev = dev;
			_mp_pui_measure_cls = dev->class;
			break;
		}
	}
	if(_mp_pui_measure_dev)
		_mp_pui_measure_face = mp_device_measure(_mp_pui_measure_dev, &_mp_pui_measure_sub);
}

static void _mp_pui_measure_refresh(mp_pui_t *ui) {
	float v[3];
	unsigned long age;
	char line[MP_PUI_TEXT];
	float lo, hi;
	mp_pui_color_t ink;

	if(ui && ui->kernel)
		_mp_pui_measure_bind(ui->kernel);
	if(!_mp_pui_measure_face || mp_device_measure_read(_mp_pui_measure_face, v, &age) != TRUE) {
		if(_mp_pui_measure_tri) {
			mp_pui_value_set(&_mp_pui_measure_x, "--");
			mp_pui_value_set(&_mp_pui_measure_y, "--");
			mp_pui_value_set(&_mp_pui_measure_z, "--");
			mp_pui_value_color(&_mp_pui_measure_x, _MP_PUI_MEASURE_DIM);
			mp_pui_value_color(&_mp_pui_measure_y, _MP_PUI_MEASURE_DIM);
			mp_pui_value_color(&_mp_pui_measure_z, _MP_PUI_MEASURE_DIM);
		} else {
			mp_pui_big_set(&_mp_pui_measure_value, "--");
			mp_pui_big_color(&_mp_pui_measure_value, _MP_PUI_MEASURE_DIM);
			mp_pui_bar_set(&_mp_pui_measure_bar, 0);
			mp_pui_bar_color(&_mp_pui_measure_bar, _MP_PUI_MEASURE_DIM, _MP_PUI_MEASURE_DIM);
		}
		return;
	}
	if(_mp_pui_measure_tri) {
		snprintf(line, sizeof(line), "%.2f", v[0]);
		mp_pui_value_set(&_mp_pui_measure_x, line);
		mp_pui_value_color(&_mp_pui_measure_x, _MP_PUI_MEASURE_CYAN);
		snprintf(line, sizeof(line), "%.2f", v[1]);
		mp_pui_value_set(&_mp_pui_measure_y, line);
		mp_pui_value_color(&_mp_pui_measure_y, _MP_PUI_MEASURE_GREEN);
		snprintf(line, sizeof(line), "%.2f", v[2]);
		mp_pui_value_set(&_mp_pui_measure_z, line);
		mp_pui_value_color(&_mp_pui_measure_z, _MP_PUI_MEASURE_AMBER);
		return;
	}
	_mp_pui_measure_range(_mp_pui_measure_cls, &lo, &hi);
	ink = _mp_pui_measure_level(v[0], lo, hi);
	snprintf(line, sizeof(line), "%.2f", v[0]);
	mp_pui_big_set(&_mp_pui_measure_value, line);
	mp_pui_big_color(&_mp_pui_measure_value, ink);
	mp_pui_bar_set(&_mp_pui_measure_bar, _mp_pui_measure_span(v[0], lo, hi));
	mp_pui_bar_color(&_mp_pui_measure_bar, ink, _MP_PUI_MEASURE_DIM);
}

static void _mp_pui_measure_build(mp_pui_page_t *page, void *user) {
	mp_pui_t *ui = user;
	mp_kernel_t *kernel = ui ? ui->kernel : NULL;
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int body = _mp_pui_body();
	int title = _mp_pui_title_h();
	const char *who = "Sensor";
	const char *unit;

	_mp_pui_measure_tri = 0;
	if(kernel)
		_mp_pui_measure_bind(kernel);
	if(_mp_pui_measure_dev && mp_device_measure_axes(_mp_pui_measure_cls) == 3)
		_mp_pui_measure_tri = 1;
	if(_mp_pui_measure_dev && _mp_pui_measure_dev->label && _mp_pui_measure_dev->label[0])
		who = _mp_pui_measure_dev->label;
	else if(_mp_pui_measure_dev)
		who = _mp_pui_measure_name(_mp_pui_measure_cls);

	mp_pui_label(&_mp_pui_measure_caption, page, 0, 0, glassW, body, who);
	mp_pui_label_align(&_mp_pui_measure_caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&_mp_pui_measure_caption, _MP_PUI_MEASURE_CYAN);

	if(_mp_pui_measure_tri) {
		int rowW = MP_PUI_DP(_mp_pui_units(glassW) > 128 ? 140 : 112);
		int y0 = MP_PUI_DP(_mp_pui_units(glassH) > 64 ? 20 : 16);
		int step = MP_PUI_DP(_mp_pui_units(glassH) > 64 ? 16 : 12);
		int x = (glassW - rowW) / 2;

		mp_pui_value(&_mp_pui_measure_x, page, x, y0, rowW, body, "X", "--");
		mp_pui_value_color(&_mp_pui_measure_x, _MP_PUI_MEASURE_CYAN);
		mp_pui_value(&_mp_pui_measure_y, page, x, y0 + step, rowW, body, "Y", "--");
		mp_pui_value_color(&_mp_pui_measure_y, _MP_PUI_MEASURE_GREEN);
		mp_pui_value(&_mp_pui_measure_z, page, x, y0 + step * 2, rowW, body, "Z", "--");
		mp_pui_value_color(&_mp_pui_measure_z, _MP_PUI_MEASURE_AMBER);
	} else {
		int tall = _mp_pui_units(glassH) > 64;
		int valueY = MP_PUI_DP(tall ? 20 : 16);
		int unitY = MP_PUI_DP(tall ? 40 : 32);
		int barW = MP_PUI_DP(_mp_pui_units(glassW) > 128 ? 120 : 96);
		int barY = MP_PUI_DP(tall ? 58 : 48);
		int barH = MP_PUI_DP(tall ? 8 : 6);

		unit = _mp_pui_measure_dev ? mp_device_measure_unit(_mp_pui_measure_cls) : "";
		mp_pui_big(&_mp_pui_measure_value, page, 0, valueY, glassW, title, "--");
		mp_pui_big_color(&_mp_pui_measure_value, _MP_PUI_MEASURE_GREEN);
		mp_pui_label(&_mp_pui_measure_unit, page, 0, unitY, glassW, body, unit ? unit : "");
		mp_pui_label_align(&_mp_pui_measure_unit, MP_PUI_ALIGN_CENTER);
		mp_pui_label_color(&_mp_pui_measure_unit, _MP_PUI_MEASURE_CYAN);
		mp_pui_bar(&_mp_pui_measure_bar, page, (glassW - barW) / 2, barY, barW, barH, 1000);
	}
	_mp_pui_measure_refresh(ui);
}

mp_ret_t mp_pui_page_measure(mp_pui_t *ui) {
	mp_ret_t ok = mp_pui_page_register(ui, &_mp_pui_measure_page, "miniPhi/Measure", _mp_pui_measure_build, ui);

	if(ok == TRUE)
		_mp_pui_measure_on = 1;
	return(ok);
}

void mp_pui_page_measure_refresh(mp_pui_t *ui) {
	if(!_mp_pui_measure_on || !ui || ui->current != &_mp_pui_measure_page)
		return;
	_mp_pui_measure_refresh(ui);
}

mp_pui_page_t *mp_pui_page_measure_page(void) {
	return(&_mp_pui_measure_page);
}

/* a measure that is not temperature and not voltage; those have their own pages */
int mp_pui_page_measure_present(mp_kernel_t *kernel) {
	mp_device_registry_t *dev = NULL;

	while((dev = mp_device_measure_next(kernel, dev)) != NULL) {
		if(mp_device_measure_class(dev->class) != YES)
			continue;
		if(dev->class == MP_DEVICE_TEMPERATURE || dev->class == MP_DEVICE_VOLTAGE)
			continue;
		return(1);
	}
	return(0);
}

#endif
