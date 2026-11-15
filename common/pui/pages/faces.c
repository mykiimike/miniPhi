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

/**
 * One page per measure device. Temperature keeps its own page.
 * A chip with two rails therefore turns as 3V3 voltage, 3V3 current,
 * 3V3 power, then the same three for 5V, instead of one combined meter.
 */

#define _MP_PUI_FACES 8

#define _MP_PUI_FACE_CYAN  ((mp_pui_color_t)0x00FFFF00u)
#define _MP_PUI_FACE_AMBER ((mp_pui_color_t)0x00FFB000u)
#define _MP_PUI_FACE_GREEN ((mp_pui_color_t)0x0000FF00u)
#define _MP_PUI_FACE_RED   ((mp_pui_color_t)0x00FF3030u)
#define _MP_PUI_FACE_DIM   ((mp_pui_color_t)0x00303030u)

typedef struct {
	int used;
	mp_pui_page_t page;
	char path[MP_PUI_PATH];
	mp_device_registry_t *dev;
	mp_device_class_t cls;
	mp_device_subscribe_t sub;
	mp_device_measure_t *meas;
	mp_pui_label_t caption;
	mp_pui_label_t kind;
	mp_pui_big_t value;
	mp_pui_label_t unit;
	mp_pui_bar_t bar;
} _mp_pui_face_t;

static _mp_pui_face_t _mp_pui_face[_MP_PUI_FACES];

static const char *_mp_pui_face_kind(mp_device_class_t cls) {
	switch(cls) {
		case MP_DEVICE_VOLTAGE: return("Voltage");
		case MP_DEVICE_CURRENT: return("Current");
		case MP_DEVICE_POWER: return("Power");
		case MP_DEVICE_BAROMETER: return("Pressure");
		case MP_DEVICE_ALTIMETER: return("Altitude");
		case MP_DEVICE_HUMIDITY: return("Humidity");
		case MP_DEVICE_ILLUMINANCE: return("Light");
		default: return("Sensor");
	}
}

static const char *_mp_pui_face_mark(mp_device_class_t cls) {
	switch(cls) {
		case MP_DEVICE_VOLTAGE: return("V");
		case MP_DEVICE_CURRENT: return("A");
		case MP_DEVICE_POWER: return("W");
		default: return(mp_device_measure_unit(cls));
	}
}

static void _mp_pui_face_scale(mp_device_class_t cls, float *lo, float *hi) {
	*lo = 0.0f;
	*hi = 1.0f;
	if(cls == MP_DEVICE_VOLTAGE)
		*hi = 6.0f;
	else if(cls == MP_DEVICE_CURRENT)
		*hi = 2.0f;
	else if(cls == MP_DEVICE_POWER)
		*hi = 10.0f;
	else if(cls == MP_DEVICE_HUMIDITY)
		*hi = 100.0f;
}

static int _mp_pui_face_span(float value, float lo, float hi) {
	if(hi <= lo)
		return(0);
	if(value < lo)
		value = lo;
	if(value > hi)
		value = hi;
	return((int)((value - lo) * 1000.0f / (hi - lo)));
}

static void _mp_pui_face_who(mp_device_registry_t *dev, char *dst, int cap) {
	const char *src;
	int n = 0;

	src = (dev && dev->label && dev->label[0]) ? dev->label : "Sensor";
	if(cap < 2)
		return;
	while(src[n] && n < cap - 1) {
		dst[n] = src[n] == '/' ? '-' : src[n];
		n++;
	}
	dst[n] = 0;
}

static _mp_pui_face_t *_mp_pui_face_find(mp_device_registry_t *dev) {
	int i;

	for(i = 0; i < _MP_PUI_FACES; i++)
		if(_mp_pui_face[i].used && _mp_pui_face[i].dev == dev)
			return(&_mp_pui_face[i]);
	return(NULL);
}

static void _mp_pui_face_refresh_one(_mp_pui_face_t *face) {
	float v[3];
	unsigned long age;
	char line[MP_PUI_TEXT];
	char unit[8];
	float lo, hi, shown;
	mp_pui_color_t ink;
	int have;

	have = face->meas && mp_device_measure_read(face->meas, v, &age) == TRUE;
	if(!have) {
		mp_pui_big_set(&face->value, "--");
		mp_pui_big_color(&face->value, _MP_PUI_FACE_DIM);
		mp_pui_bar_set(&face->bar, 0);
		mp_pui_bar_color(&face->bar, _MP_PUI_FACE_DIM, _MP_PUI_FACE_DIM);
		return;
	}
	shown = v[0];
	snprintf(unit, sizeof(unit), "%s", _mp_pui_face_mark(face->cls));
	if(face->cls == MP_DEVICE_CURRENT && shown < 1.0f && shown > -1.0f) {
		shown *= 1000.0f;
		snprintf(unit, sizeof(unit), "mA");
	}
	if(face->cls == MP_DEVICE_CURRENT && (v[0] >= 1.0f || v[0] <= -1.0f))
		snprintf(line, sizeof(line), "%.2f", v[0]);
	else if(face->cls == MP_DEVICE_CURRENT)
		snprintf(line, sizeof(line), "%.0f", shown);
	else
		snprintf(line, sizeof(line), "%.2f", shown);
	if(face->cls == MP_DEVICE_VOLTAGE)
		ink = v[0] < 0.4f ? _MP_PUI_FACE_RED : _MP_PUI_FACE_GREEN;
	else {
		_mp_pui_face_scale(face->cls, &lo, &hi);
		shown = (v[0] < 0.0f ? -v[0] : v[0]);
		if(hi > lo && shown / hi < 0.34f)
			ink = _MP_PUI_FACE_GREEN;
		else if(hi > lo && shown / hi < 0.67f)
			ink = _MP_PUI_FACE_AMBER;
		else
			ink = _MP_PUI_FACE_RED;
	}
	mp_pui_big_set(&face->value, line);
	mp_pui_big_color(&face->value, ink);
	mp_pui_label_set(&face->unit, unit);
	mp_pui_label_color(&face->unit, _MP_PUI_FACE_CYAN);
	_mp_pui_face_scale(face->cls, &lo, &hi);
	mp_pui_bar_set(&face->bar, _mp_pui_face_span(v[0] < 0.0f ? 0.0f : v[0], lo, hi));
	mp_pui_bar_color(&face->bar, ink, _MP_PUI_FACE_DIM);
}

static void _mp_pui_face_build(mp_pui_page_t *page, void *user) {
	_mp_pui_face_t *face = user;
	mp_pui_t *ui = page ? page->ui : NULL;
	int glassW = ui && ui->display ? ui->display->width : 128;
	int glassH = ui && ui->display ? ui->display->height : 64;
	int body = _mp_pui_body();
	int title = _mp_pui_title_h();
	int tall = _mp_pui_units(glassH) > 64;
	int nameY = MP_PUI_DP(tall ? 14 : 12);
	int valueY = MP_PUI_DP(tall ? 28 : 24);
	int unitY = MP_PUI_DP(tall ? 48 : 40);
	int barW = MP_PUI_DP(_mp_pui_units(glassW) > 128 ? 140 : 112);
	int barY = MP_PUI_DP(tall ? 64 : 54);
	char who[16];

	if(!face)
		return;
	_mp_pui_face_who(face->dev, who, (int)sizeof(who));
	if(face->dev)
		face->meas = mp_device_measure(face->dev, &face->sub);

	mp_pui_label(&face->caption, page, 0, 0, glassW, body, who);
	mp_pui_label_align(&face->caption, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&face->caption, _MP_PUI_FACE_CYAN);

	mp_pui_label(&face->kind, page, 0, nameY, glassW, body, _mp_pui_face_kind(face->cls));
	mp_pui_label_align(&face->kind, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&face->kind, _MP_PUI_FACE_AMBER);

	mp_pui_big(&face->value, page, 0, valueY, glassW, title, "--");
	mp_pui_big_color(&face->value, _MP_PUI_FACE_GREEN);

	mp_pui_label(&face->unit, page, 0, unitY, glassW, body, _mp_pui_face_mark(face->cls));
	mp_pui_label_align(&face->unit, MP_PUI_ALIGN_CENTER);
	mp_pui_label_color(&face->unit, _MP_PUI_FACE_CYAN);

	mp_pui_bar(&face->bar, page, (glassW - barW) / 2, barY, barW, MP_PUI_DP(8), 1000);
	_mp_pui_face_refresh_one(face);
}

static void _mp_pui_face_drop(_mp_pui_face_t *face) {
	if(face->page.ui)
		mp_pui_page_unregister(face->page.ui, &face->page);
	mp_device_subscribe_end(&face->sub);
	memset(face, 0, sizeof(*face));
}

static int _mp_pui_face_claim(mp_pui_t *ui, mp_device_registry_t *dev) {
	_mp_pui_face_t *face = NULL;
	char who[16];
	int i;

	for(i = 0; i < _MP_PUI_FACES; i++) {
		if(!_mp_pui_face[i].used) {
			face = &_mp_pui_face[i];
			break;
		}
	}
	if(!face)
		return(0);
	memset(face, 0, sizeof(*face));
	face->dev = dev;
	face->cls = dev->class;
	face->meas = mp_device_measure(dev, &face->sub);
	_mp_pui_face_who(dev, who, (int)sizeof(who));
	snprintf(face->path, sizeof(face->path), "miniPhi/%s %s", who, _mp_pui_face_mark(dev->class));
	if(mp_pui_page_register(ui, &face->page, face->path, _mp_pui_face_build, face) != TRUE) {
		snprintf(face->path, sizeof(face->path), "miniPhi/%s %s %d", who, _mp_pui_face_mark(dev->class), dev->index);
		if(mp_pui_page_register(ui, &face->page, face->path, _mp_pui_face_build, face) != TRUE) {
			mp_device_subscribe_end(&face->sub);
			memset(face, 0, sizeof(*face));
			return(0);
		}
	}
	face->used = 1;
	return(1);
}

static void _mp_pui_page_faces_sync(mp_pui_t *ui);

void mp_pui_page_faces_sync(mp_pui_t *ui) {
	int _mp_pui_key = mp_pui_lock();
	_mp_pui_page_faces_sync(ui);
	mp_pui_unlock(_mp_pui_key);
}

static void _mp_pui_page_faces_sync(mp_pui_t *ui) {
	mp_device_registry_t *dev = NULL;
	unsigned char seen[_MP_PUI_FACES];
	int i;

	if(!ui || !ui->kernel)
		return;
	memset(seen, 0, sizeof(seen));
	while((dev = mp_device_measure_next(ui->kernel, dev)) != NULL) {
		_mp_pui_face_t *face;

		if(dev->class == MP_DEVICE_TEMPERATURE)
			continue;
		face = _mp_pui_face_find(dev);
		if(!face) {
			if(!_mp_pui_face_claim(ui, dev))
				continue;
			face = _mp_pui_face_find(dev);
		}
		if(!face)
			continue;
		seen[face - _mp_pui_face] = 1;
	}
	for(i = 0; i < _MP_PUI_FACES; i++) {
		if(_mp_pui_face[i].used && !seen[i])
			_mp_pui_face_drop(&_mp_pui_face[i]);
	}
}

void mp_pui_page_faces_refresh(mp_pui_t *ui) {
	int i;

	if(!ui)
		return;
	for(i = 0; i < _MP_PUI_FACES; i++) {
		if(_mp_pui_face[i].used && ui->current == &_mp_pui_face[i].page)
			_mp_pui_face_refresh_one(&_mp_pui_face[i]);
	}
}

void mp_pui_page_faces(mp_pui_t *ui) {
	mp_pui_page_faces_sync(ui);
}

#endif
