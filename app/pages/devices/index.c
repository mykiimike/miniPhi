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

/*
 * Home page. Categories that have a device, then the devices of the one
 * that was opened, then that device's own view. A board device that has
 * not been started yet is still listed: opening it starts it.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN
#ifdef SUPPORT_COMMON_DEVICE

/* same order as mp_device_class_t, and not MP_DEVICE_ANY */
static const char *_sysinfo_dev_class_name[] = {
	"Display", "Touch", "Accelerometer", "LED", "Camera",
	"Temperature", "Gyroscope", "Magnetometer", "Voltage", "Current",
	"CPU", "Button", "Barometer", "Altimeter", "Power",
	"Humidity", "Illuminance", "Orientation", "Clock"
};

typedef char _sysinfo_dev_class_ok[(sizeof(_sysinfo_dev_class_name) / sizeof(_sysinfo_dev_class_name[0]) == MP_DEVICE_CLASS_COUNT) ? 1 : -1];

static int _sysinfo_dev_same(sysinfo_t *sysinfo, int class, void *self) {
	int n, count;

	if(!self)
		return(0);
	count = mp_device_registry_count(&sysinfo->kernel);
	for(n = 0; n < count; n++) {
		mp_device_registry_t *device = mp_device_registry_at(&sysinfo->kernel, n);

		if(device && device->class == class && device->driver && device->driver->self == self)
			return(1);
	}
	return(0);
}

/* rank walks registered devices of this class, then board devices not registered as one */
static int _sysinfo_dev_visit(sysinfo_t *sysinfo, int class, int rank, mp_device_registry_t **reg, mp_device_t **legacy) {
	int n, count, seen = 0;

	count = mp_device_registry_count(&sysinfo->kernel);
	for(n = 0; n < count; n++) {
		mp_device_registry_t *device = mp_device_registry_at(&sysinfo->kernel, n);

		if(!device || device->class != class)
			continue;
		if(seen == rank) {
			if(reg)
				*reg = device;
			if(legacy)
				*legacy = NULL;
			return(1);
		}
		seen++;
	}
	count = mp_device_count();
	for(n = 0; n < count; n++) {
		mp_device_t *device = mp_device_at(n);

		/* listed once its driver gave it a name */
		if(!device || !device->driver || device->driver->type != class || !device->label)
			continue;
		if(_sysinfo_dev_same(sysinfo, class, device->self))
			continue;
		if(seen == rank) {
			if(reg)
				*reg = NULL;
			if(legacy)
				*legacy = device;
			return(1);
		}
		seen++;
	}
	return(0);
}

static int _sysinfo_dev_count(sysinfo_t *sysinfo, int class) {
	int n = 0;

	while(_sysinfo_dev_visit(sysinfo, class, n, NULL, NULL))
		n++;
	return(n);
}

void _sysinfo_dev_current(sysinfo_t *sysinfo, mp_device_registry_t **reg, mp_device_t **legacy) {
	if(reg)
		*reg = NULL;
	if(legacy)
		*legacy = NULL;
	if(!sysinfo || sysinfo->screen.devClass < 0)
		return;
	_sysinfo_dev_visit(sysinfo, sysinfo->screen.devClass, sysinfo->screen.devRank, reg, legacy);
}

void _sysinfo_dev_caption(sysinfo_t *sysinfo, mp_device_registry_t *reg, mp_device_t *legacy, char *dst, int cap) {
	char type[16];
	char model[32];
	char serial[32];
	int num = 0;

	(void)sysinfo;
	dst[0] = 0;
	if(cap < 2)
		return;
	/* the name the driver registered the device with */
	if(legacy && legacy->label) {
		snprintf(dst, (size_t)cap, "%s", legacy->label);
		return;
	}
	if(reg && reg->label && reg->label[0]) {
		snprintf(dst, (size_t)cap, "%s", reg->label);
		return;
	}
	if(reg && mp_device_address_split(reg->address, type, (int)sizeof(type), &num, model, (int)sizeof(model), serial, (int)sizeof(serial)) == TRUE && model[0]) {
		snprintf(dst, (size_t)cap, "%s", model);
		return;
	}
	snprintf(dst, (size_t)cap, "%s", "Device");
}

static int _sysinfo_dev_button(mp_tui_widget_t *page, sysinfo_t *sysinfo, int slot, int y, const char *text, int id, mp_tui_button_on_t onPress) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;
	int h = line + 18;

	if(slot < 0 || slot >= MP_CRYPTO_MAX)
		return(y);
	mp_tui_button(&sysinfo->screen.menu.device[slot], page, 8, y, width, h, text ? text : "");
	sysinfo->screen.menu.device[slot].chevron = 1;
	sysinfo->screen.menu.device[slot].onPress = onPress;
	sysinfo->screen.menu.device[slot].user = sysinfo;
	sysinfo->screen.menu.device[slot].id = id;
	return(y + h + 6);
}

static void _sysinfo_on_class(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;
	int class = button->id;

	if(!sysinfo || sysinfo->screen.devList < 0)
		return;
	if(class < 0 || class >= MP_DEVICE_CLASS_COUNT)
		return;
	sysinfo->screen.devClass = class;
	sysinfo->screen.tui.app[sysinfo->screen.devList].name = _sysinfo_dev_class_name[class];
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.devList);
}

static void _sysinfo_on_device(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;
	mp_device_registry_t *reg = NULL;
	mp_device_t *legacy = NULL;

	if(!sysinfo || sysinfo->screen.devView < 0)
		return;
	sysinfo->screen.devRank = button->id;
	_sysinfo_dev_current(sysinfo, &reg, &legacy);
	_sysinfo_dev_caption(sysinfo, reg, legacy, sysinfo->screen.devTitle, (int)sizeof(sysinfo->screen.devTitle));
	sysinfo->screen.tui.app[sysinfo->screen.devView].name = sysinfo->screen.devTitle;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.devView);
	if(sysinfo->screen.task)
		mp_task_wake(sysinfo->screen.task);
}

static void _sysinfo_dev_classes(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int slot = 0;
	int y;
	int class;

	y = 8;
	for(class = 0; class < MP_DEVICE_CLASS_COUNT && slot < MP_CRYPTO_MAX; class++) {
		char label[32];
		int n = _sysinfo_dev_count(sysinfo, class);

		if(n < 1)
			continue;
		snprintf(label, sizeof(label), "%s (%d)", _sysinfo_dev_class_name[class], n);
		y = _sysinfo_dev_button(page, sysinfo, slot, y, label, class, _sysinfo_on_class);
		slot++;
	}
	if(slot == 0) {
		mp_tui_label(&sysinfo->screen.accelName, page, 8, 12, page->rect.w - 16, line, "No device");
		mp_tui_label_color(&sysinfo->screen.accelName, page->tui->theme.muted);
	}
}

static void _sysinfo_dev_list(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int count = _sysinfo_dev_count(sysinfo, sysinfo->screen.devClass);
	int n;
	int y = 8;
	char text[32];

	if(count < 1) {
		mp_tui_label(&sysinfo->screen.accelName, page, 8, 12, page->rect.w - 16, line, "No device");
		mp_tui_label_color(&sysinfo->screen.accelName, page->tui->theme.muted);
		return;
	}
	for(n = 0; n < count && n < MP_CRYPTO_MAX; n++) {
		mp_device_registry_t *reg = NULL;
		mp_device_t *legacy = NULL;

		_sysinfo_dev_visit(sysinfo, sysinfo->screen.devClass, n, &reg, &legacy);
		_sysinfo_dev_caption(sysinfo, reg, legacy, text, (int)sizeof(text));
		y = _sysinfo_dev_button(page, sysinfo, n, y, text, n, _sysinfo_on_device);
	}
}

static void _sysinfo_dev_lines(sysinfo_t *sysinfo, mp_tui_widget_t *page, const char *a, const char *b, const char *c) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 32;
	int y = 28;

	mp_tui_label(&sysinfo->screen.accelName, page, 16, y, width, line, a ? a : "");
	mp_tui_label_align(&sysinfo->screen.accelName, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelName, page->tui->theme.muted);
	y += line + 18;
	mp_tui_label(&sysinfo->screen.accelX, page, 16, y, width, line, b ? b : "");
	mp_tui_label_align(&sysinfo->screen.accelX, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelX, page->tui->theme.accent);
	y += line + 8;
	mp_tui_label(&sysinfo->screen.accelY, page, 16, y, width, line, c ? c : "");
	mp_tui_label_align(&sysinfo->screen.accelY, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelY, page->tui->theme.muted);
}

static int _sysinfo_vdda_like(const char *label) {
	const char *p;

	if(!label)
		return(0);
	for(p = label; p[0] && p[1] && p[2] && p[3]; p++) {
		char c0 = p[0], c1 = p[1], c2 = p[2], c3 = p[3];

		if(c0 >= 'a' && c0 <= 'z')
			c0 = (char)(c0 - 32);
		if(c1 >= 'a' && c1 <= 'z')
			c1 = (char)(c1 - 32);
		if(c2 >= 'a' && c2 <= 'z')
			c2 = (char)(c2 - 32);
		if(c3 >= 'a' && c3 <= 'z')
			c3 = (char)(c3 - 32);
		if(c0 == 'V' && c1 == 'D' && c2 == 'D' && c3 == 'A')
			return(1);
	}
	return(0);
}

static void _sysinfo_dev_measure_gauge(mp_tui_widget_t *page, sysinfo_t *sysinfo, int *y) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int class = sysinfo->screen.devClass;
	mp_device_registry_t *reg = NULL;
	mp_tui_gauge_style_t style = MP_TUI_GAUGE_DIAL;
	long min, max, warn, alarm;
	unsigned char decimals;
	const char *unit;
	const char *label = NULL;
	int gw, gh, limit;

	_sysinfo_dev_current(sysinfo, &reg, NULL);
	if(reg && reg->label && reg->label[0])
		label = reg->label;
	if(class == MP_DEVICE_TEMPERATURE) {
		min = -20000;
		max = 85000;
		warn = 50000;
		alarm = 70000;
		decimals = 1;
		unit = "°C";
	}
	else if(_sysinfo_vdda_like(label)) {
		min = 0;
		max = 3600;
		warn = max;
		alarm = max;
		decimals = 2;
		unit = "V";
	}
	else {
		min = 0;
		max = 5000;
		warn = max;
		alarm = max;
		decimals = 3;
		unit = "V";
	}

	gw = page->rect.w - 72;
	if(gw > 176)
		gw = 176;
	if(gw < 16)
		gw = 16;
	gh = gw * 3 / 4 + line;
	limit = page->rect.h - line * 2 - 16;
	if(limit < gh)
		gh = limit;
	if(gw < 100 || gh < 88) {
		style = MP_TUI_GAUGE_BAR;
		gw = page->rect.w - 32;
		if(gw < 16)
			gw = 16;
		gh = line * 3 + 8;
	}
	mp_tui_gauge(&sysinfo->screen.measureGauge, page, (page->rect.w - gw) / 2, *y, gw, gh, style, min, max, decimals, unit);
	mp_tui_gauge_zones(&sysinfo->screen.measureGauge, warn, alarm);
	if(label)
		mp_tui_gauge_label(&sysinfo->screen.measureGauge, label);
	*y += gh + 8;
}

static void _sysinfo_dev_measure_build(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int class = sysinfo->screen.devClass;
	int axes = mp_device_measure_axes(class);
	int width = page->rect.w - 48;
	int x = 24;
	int y;

	if(axes >= 3)
		y = 24;
	else if(class == MP_DEVICE_TEMPERATURE || class == MP_DEVICE_VOLTAGE) {
		y = 8;
		_sysinfo_dev_measure_gauge(page, sysinfo, &y);
	}
	else
		y = 48;

	mp_tui_label(&sysinfo->screen.accelName, page, x, y, width, line, "");
	mp_tui_label_align(&sysinfo->screen.accelName, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.accelName, page->tui->theme.accent);
	y += line + 10;
	mp_tui_label(&sysinfo->screen.accelX, page, x, y, width, line, "");
	mp_tui_label_align(&sysinfo->screen.accelX, MP_TUI_ALIGN_CENTER);
	y += line + 4;
	mp_tui_label(&sysinfo->screen.accelY, page, x, y, width, line, "");
	mp_tui_label_align(&sysinfo->screen.accelY, MP_TUI_ALIGN_CENTER);
	y += line + 4;
	mp_tui_label(&sysinfo->screen.accelZ, page, x, y, width, line, "");
	mp_tui_label_align(&sysinfo->screen.accelZ, MP_TUI_ALIGN_CENTER);
}

static void _sysinfo_dev_measure_text(char *dst, int cap, int class, float value) {
	const char *unit = mp_device_measure_unit(class);
	char num[16];
	int decimals = 2;

	if(class == MP_DEVICE_TEMPERATURE)
		decimals = 1;
	else if(class == MP_DEVICE_VOLTAGE)
		decimals = 3;
	_sysinfo_format_fixed(num, (int)sizeof(num), value, decimals, NO);
	if(class == MP_DEVICE_TEMPERATURE)
		snprintf(dst, (size_t)cap, "%s °%s", num, unit);
	else
		snprintf(dst, (size_t)cap, "%s %s", num, unit);
}

static void _sysinfo_dev_measure_refresh(sysinfo_t *sysinfo) {
	mp_device_registry_t *reg = NULL;
	float v[3];
	char text[32];
	int class = sysinfo->screen.devClass;
	int axes = mp_device_measure_axes(class);

	_sysinfo_dev_current(sysinfo, &reg, NULL);
	if(!reg || mp_device_measure_read(reg->instance, v, NULL) != TRUE) {
		mp_tui_label_set(&sysinfo->screen.accelName, "%s", "--");
		mp_tui_label_set(&sysinfo->screen.accelX, "%s", "");
		mp_tui_label_set(&sysinfo->screen.accelY, "%s", "");
		mp_tui_label_set(&sysinfo->screen.accelZ, "%s", "");
		if((class == MP_DEVICE_TEMPERATURE || class == MP_DEVICE_VOLTAGE) && sysinfo->screen.measureGauge.widget.parent)
			mp_tui_gauge_set(&sysinfo->screen.measureGauge, 0, 0);
		return;
	}
	if(axes >= 3) {
		_sysinfo_dev_measure_text(text, (int)sizeof(text), class, v[0]);
		mp_tui_label_set(&sysinfo->screen.accelX, "X  %s", text);
		_sysinfo_dev_measure_text(text, (int)sizeof(text), class, v[1]);
		mp_tui_label_set(&sysinfo->screen.accelY, "Y  %s", text);
		_sysinfo_dev_measure_text(text, (int)sizeof(text), class, v[2]);
		mp_tui_label_set(&sysinfo->screen.accelZ, "Z  %s", text);
		mp_tui_label_set(&sysinfo->screen.accelName, "%s", "");
		return;
	}
	_sysinfo_dev_measure_text(text, (int)sizeof(text), class, v[0]);
	mp_tui_label_set(&sysinfo->screen.accelName, "%s", text);
	mp_tui_label_set(&sysinfo->screen.accelX, "%s", "");
	mp_tui_label_set(&sysinfo->screen.accelY, "%s", "");
	mp_tui_label_set(&sysinfo->screen.accelZ, "%s", "");
	if((class == MP_DEVICE_TEMPERATURE || class == MP_DEVICE_VOLTAGE) && sysinfo->screen.measureGauge.widget.parent)
		mp_tui_gauge_set(&sysinfo->screen.measureGauge, _sysinfo_milli(v[0]), 1);
}

static void _sysinfo_dev_card(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	mp_device_registry_t *reg = NULL;
	mp_device_t *legacy = NULL;
	char model[32];
	char serial[32];
	char type[16];
	int num = 0;
	const char *modelText = NULL;
	const char *serialText = NULL;

	_sysinfo_dev_current(sysinfo, &reg, &legacy);
	model[0] = 0;
	serial[0] = 0;
	if(legacy) {
		modelText = legacy->model;
		serialText = legacy->serial;
	}
	else if(reg && mp_device_address_split(reg->address, type, (int)sizeof(type), &num, model, (int)sizeof(model), serial, (int)sizeof(serial)) == TRUE) {
		modelText = model;
		serialText = serial;
	}
	if(sysinfo->screen.devClass >= 0 && sysinfo->screen.devClass < MP_DEVICE_CLASS_COUNT)
		_sysinfo_dev_lines(sysinfo, page, _sysinfo_dev_class_name[sysinfo->screen.devClass], modelText, serialText);
	else
		_sysinfo_dev_lines(sysinfo, page, sysinfo->screen.devTitle, modelText, serialText);
}

static void _sysinfo_dev_view(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;

	if(sysinfo->screen.devClass == MP_DEVICE_ACCELEROMETER)
		_sysinfo_dev_accel_build(page, sysinfo);
	else if(sysinfo->screen.devClass == MP_DEVICE_CAMERA)
		_sysinfo_dev_camera_build(page, sysinfo);
	else if(sysinfo->screen.devClass == MP_DEVICE_CLOCK)
		_sysinfo_dev_clock_build(page, sysinfo);
	else if(mp_device_measure_class(sysinfo->screen.devClass) == YES) {
		_sysinfo_dev_measure_build(page, sysinfo);
		_sysinfo_dev_measure_refresh(sysinfo);
	}
	else
		_sysinfo_dev_card(page, sysinfo);
}

void _sysinfo_dev_open(sysinfo_t *sysinfo) {
	mp_tui_t *tui = &sysinfo->screen.tui;

	sysinfo->screen.devIndex = -1;
	sysinfo->screen.devList = -1;
	sysinfo->screen.devView = -1;
	sysinfo->screen.devClass = -1;
	sysinfo->screen.devRank = 0;
	sysinfo->screen.devTitle[0] = 0;
	if(mp_tui_app(tui, "Devices", _sysinfo_dev_classes, sysinfo))
		sysinfo->screen.devIndex = tui->appCount - 1;
	if(mp_tui_child(tui, "Devices", _sysinfo_dev_list, sysinfo))
		sysinfo->screen.devList = tui->appCount - 1;
	if(mp_tui_child(tui, "Device", _sysinfo_dev_view, sysinfo))
		sysinfo->screen.devView = tui->appCount - 1;
}

void _sysinfo_dev_refresh(sysinfo_t *sysinfo) {
	/* the overlay reads this camera; leaving its page drops the image */
	if(_sysinfo_dev_camera_shown(sysinfo) == NO && mp_tui_fullscreen_shown(&sysinfo->screen.tui))
		mp_tui_fullscreen_close(&sysinfo->screen.tui);
	if(_sysinfo_dev_clock_shown(sysinfo) == NO)
		_sysinfo_dev_clock_release(sysinfo);
	_sysinfo_dev_camera_refresh(sysinfo);
	if(sysinfo->screen.devView < 0 || sysinfo->screen.tui.opened != sysinfo->screen.devView)
		return;
	if(sysinfo->screen.devClass == MP_DEVICE_ACCELEROMETER)
		_sysinfo_dev_accel_refresh(sysinfo);
	else if(sysinfo->screen.devClass == MP_DEVICE_CLOCK)
		_sysinfo_dev_clock_refresh(sysinfo);
	else if(sysinfo->screen.devClass != MP_DEVICE_CAMERA && mp_device_measure_class(sysinfo->screen.devClass) == YES)
		_sysinfo_dev_measure_refresh(sysinfo);
}

mp_bool_t _sysinfo_dev_accel_shown(sysinfo_t *sysinfo) {
	if(!sysinfo || sysinfo->screen.devView < 0)
		return(NO);
	if(sysinfo->screen.tui.opened != sysinfo->screen.devView)
		return(NO);
	return(sysinfo->screen.devClass == MP_DEVICE_ACCELEROMETER ? YES : NO);
}

mp_bool_t _sysinfo_dev_camera_shown(sysinfo_t *sysinfo) {
	if(!sysinfo || sysinfo->screen.devView < 0)
		return(NO);
	if(sysinfo->screen.tui.opened != sysinfo->screen.devView)
		return(NO);
	return(sysinfo->screen.devClass == MP_DEVICE_CAMERA ? YES : NO);
}

#else

void _sysinfo_dev_open(sysinfo_t *sysinfo) {
	(void)sysinfo;
}

void _sysinfo_dev_refresh(sysinfo_t *sysinfo) {
	(void)sysinfo;
}

mp_bool_t _sysinfo_dev_accel_shown(sysinfo_t *sysinfo) {
	(void)sysinfo;
	return(NO);
}

mp_bool_t _sysinfo_dev_camera_shown(sysinfo_t *sysinfo) {
	(void)sysinfo;
	return(NO);
}

#endif
#endif
