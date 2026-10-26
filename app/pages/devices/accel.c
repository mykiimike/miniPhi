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
 * Accelerometer view of Devices. Numbers and a bubble level, and the switch
 * for SYSINFO_KEY_ACCEL. The accelerometers only run while this view is on
 * a lit screen.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN
#ifdef SUPPORT_COMMON_DEVICE

static int _sysinfo_isqrt(int n) {
	int root = 0;
	int bit = 1 << 14;

	if(n <= 0)
		return(0);
	while(bit > n)
		bit >>= 2;
	while(bit) {
		if(root + bit <= n) {
			n -= root + bit;
			root = (root >> 1) + bit;
		}
		else
			root >>= 1;
		bit >>= 2;
	}
	return(root);
}

static void _sysinfo_disc(mp_tui_draw_t *draw, int cx, int cy, int radius, mp_tui_color_t color) {
	int dy, span;
	int y0 = draw->clip.y;
	int y1 = draw->clip.y + draw->clip.h;

	for(dy = -radius; dy <= radius; dy++) {
		if(cy + dy < y0 || cy + dy >= y1)
			continue;
		span = _sysinfo_isqrt(radius * radius - dy * dy);
		mp_tui_draw_hline(draw, cx - span, cy + dy, span * 2 + 1, color);
	}
}

static void _sysinfo_ring(mp_tui_draw_t *draw, int cx, int cy, int radius, mp_tui_color_t color) {
	int dy, span;
	int y0 = draw->clip.y;
	int y1 = draw->clip.y + draw->clip.h;

	if(radius < 1)
		return;
	for(dy = -radius; dy <= radius; dy++) {
		if(cy + dy < y0 || cy + dy >= y1)
			continue;
		span = _sysinfo_isqrt(radius * radius - dy * dy);
		mp_tui_draw_hline(draw, cx - span, cy + dy, 2, color);
		if(span > 1)
			mp_tui_draw_hline(draw, cx + span - 1, cy + dy, 2, color);
	}
}

/* the glass is ±1 g; the bubble is where X (right) and Y (up) pull */
static void _sysinfo_level_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	sysinfo_level_t *level = (sysinfo_level_t *)widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	int x = widget->screen.x;
	int y = widget->screen.y;
	int w = widget->screen.w;
	int cx = x + w / 2;
	int cy = y + w / 2;
	int radius = w / 2 - 4;
	int travel, px, py, bx, by;

	if(radius < 8)
		return;
	travel = radius - 8;
	px = level->mx;
	py = level->my;
	if(px > 100)
		px = 100;
	if(px < -100)
		px = -100;
	if(py > 100)
		py = 100;
	if(py < -100)
		py = -100;
	bx = cx + px * travel / 100;
	by = cy - py * travel / 100;

	mp_tui_draw_fill(draw, x, y, w, widget->screen.h, theme->surface);
	_sysinfo_disc(draw, cx, cy, radius, theme->background);
	_sysinfo_ring(draw, cx, cy, radius, theme->border);
	_sysinfo_ring(draw, cx, cy, travel, theme->muted);
	mp_tui_draw_hline(draw, cx - radius, cy, radius * 2 + 1, theme->foreground);
	mp_tui_draw_vline(draw, cx, cy - radius, radius * 2 + 1, theme->foreground);
	mp_tui_draw_text(draw, theme->font, cx + travel - 8, cy - theme->font->lineHeight - 1, "X", theme->muted);
	mp_tui_draw_text(draw, theme->font, cx + 4, cy - travel, "Y", theme->muted);
	_sysinfo_disc(draw, bx, by, 6, theme->accent);
	{
		const char *scale = "+/- 1 g";
		int tw = mp_tui_text_width(theme->font, scale);

		mp_tui_draw_text(draw, theme->font, x + (w - tw) / 2, y + w + 2, scale, theme->muted);
	}
}

static const mp_tui_class_t _sysinfo_level_class = { "level", _sysinfo_level_draw, NULL, NULL };

static mp_device_registry_t *_sysinfo_accel_temperature(mp_kernel_t *kernel, mp_device_registry_t *axis) {
	mp_device_registry_t *sensor = NULL;

	if(!axis)
		return(NULL);
	while((sensor = mp_device_measure_next(kernel, sensor)) != NULL) {
		if(_sysinfo_measure_beside(axis, sensor))
			return(sensor);
	}
	return(NULL);
}

static int _sysinfo_cg(float g) {
	int v;

	if(g >= 0.0f)
		v = (int)(g * 100.0f + 0.5f);
	else
		v = (int)(g * 100.0f - 0.5f);
	if(v > 400)
		v = 400;
	if(v < -400)
		v = -400;
	return(v);
}

static void _sysinfo_accel_values(sysinfo_t *sysinfo) {
	mp_device_registry_t *axis = NULL;
	mp_device_t *legacy = NULL;
	mp_device_registry_t *temperature;
	sysinfo_level_t *level = &sysinfo->screen.level;
	char caption[32];
	float g[3], t[3];
	int mx, my, mz;

	_sysinfo_dev_current(sysinfo, &axis, &legacy);
	temperature = _sysinfo_accel_temperature(&sysinfo->kernel, axis);
	_sysinfo_dev_caption(sysinfo, axis, legacy, caption, (int)sizeof(caption));
	mp_tui_switch_set(&sysinfo->screen.accelSwitch, sysinfo->accelEnabled == YES);
	if(sysinfo->accelEnabled == NO || !axis || mp_device_measure_read(axis->instance, g, NULL) != TRUE) {
		mp_tui_label_set(&sysinfo->screen.accelName, "%s", sysinfo->accelEnabled == NO ? "Accelerometer off" :
			axis ? caption : sysinfo->accelRunning == YES ? "No accelerometer" : "Starting");
		mp_tui_label_set(&sysinfo->screen.accelX, "%s", "X  --");
		mp_tui_label_set(&sysinfo->screen.accelY, "%s", "Y  --");
		mp_tui_label_set(&sysinfo->screen.accelZ, "%s", "Z  --");
		mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s", "");
		if(sysinfo->screen.measureGauge.widget.parent && sysinfo->screen.measureGauge.style == MP_TUI_GAUGE_THERMO)
			mp_tui_gauge_set(&sysinfo->screen.measureGauge, 0, 0);
		if(level->mx || level->my || level->mz) {
			level->mx = 0;
			level->my = 0;
			level->mz = 0;
			mp_tui_invalidate(&level->widget);
		}
		return;
	}

	mp_tui_label_set(&sysinfo->screen.accelName, "%s", caption);
	_sysinfo_format_fixed(caption, (int)sizeof(caption), g[0], 2, YES);
	mp_tui_label_set(&sysinfo->screen.accelX, "X  %s g", caption);
	_sysinfo_format_fixed(caption, (int)sizeof(caption), g[1], 2, YES);
	mp_tui_label_set(&sysinfo->screen.accelY, "Y  %s g", caption);
	_sysinfo_format_fixed(caption, (int)sizeof(caption), g[2], 2, YES);
	mp_tui_label_set(&sysinfo->screen.accelZ, "Z  %s g", caption);
	if(sysinfo->screen.measureGauge.widget.parent && sysinfo->screen.measureGauge.style == MP_TUI_GAUGE_THERMO) {
		if(temperature && mp_device_measure_read(temperature->instance, t, NULL) == TRUE)
			mp_tui_gauge_set(&sysinfo->screen.measureGauge, _sysinfo_milli(t[0]), 1);
		else
			mp_tui_gauge_set(&sysinfo->screen.measureGauge, 0, 0);
	}
	else if(temperature && mp_device_measure_read(temperature->instance, t, NULL) == TRUE) {
		_sysinfo_format_fixed(caption, (int)sizeof(caption), t[0], 1, NO);
		mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s °C", caption);
	}
	else
		mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s", "");

	mx = _sysinfo_cg(g[0]);
	my = _sysinfo_cg(g[1]);
	mz = _sysinfo_cg(g[2]);
	level->mz = mz;
	if(mx == level->mx && my == level->my)
		return;
	level->mx = mx;
	level->my = my;
	mp_tui_invalidate(&level->widget);
}

static void _sysinfo_on_accel_switch(mp_tui_switch_t *sw) {
	sysinfo_t *sysinfo = sw->user;

	_sysinfo_accel_enable(sysinfo, sw->on ? YES : NO);
	/* stays where it was when the change did not happen */
	mp_tui_switch_set(sw, sysinfo->accelEnabled == YES);
}

/* the level, then the readings; on a portrait screen the readings go under it */
static const mp_tui_column_spec_t _sysinfo_accel_layout[] = {
	{ 168, 120 },
	{ 0, 120 },
};

void _sysinfo_dev_accel_build(mp_tui_widget_t *page, sysinfo_t *sysinfo) {
	mp_tui_t *tui = page->tui;
	const mp_tui_font_t *font = tui->theme.font;
	mp_tui_columns_t *layout = &sysinfo->screen.accelLayout;
	mp_tui_widget_t *left, *right;
	int line = font->lineHeight + 1;
	int side, y, w;

	mp_tui_columns(layout, page, 4, 4, page->rect.w - 8, _sysinfo_accel_layout, 2, 12);
	left = mp_tui_columns_at(layout, 0);
	right = mp_tui_columns_at(layout, 1);

	/* side by side, the glass and its caption also fit the page height */
	side = left->rect.w;
	if(!layout->stacked && side > page->rect.h - 8 - line)
		side = page->rect.h - 8 - line;
	if(side > 168)
		side = 168;
	if(side < 96)
		side = 96;

	sysinfo->screen.level.mx = 0;
	sysinfo->screen.level.my = 0;
	sysinfo->screen.level.mz = 0;
	mp_tui_widget_init(&sysinfo->screen.level.widget, &_sysinfo_level_class, left, (left->rect.w - side) / 2, 0, side, side + line + 2);

	y = 4;
	w = right->rect.w;
	mp_tui_label_wrap(&sysinfo->screen.accelName, right, 0, &y, w, line, font, "");
	mp_tui_label_color(&sysinfo->screen.accelName, tui->theme.accent);
	y += 4;
	mp_tui_label_wrap(&sysinfo->screen.accelX, right, 0, &y, w, line, font, "X");
	mp_tui_label_wrap(&sysinfo->screen.accelY, right, 0, &y, w, line, font, "Y");
	mp_tui_label_wrap(&sysinfo->screen.accelZ, right, 0, &y, w, line, font, "Z");
	y += 4;
	{
		mp_device_registry_t *axis = NULL;
		mp_device_registry_t *heat;

		_sysinfo_dev_current(sysinfo, &axis, NULL);
		heat = _sysinfo_accel_temperature(&sysinfo->kernel, axis);
		if(heat && w >= 104) {
			int gw = w > 156 ? 156 : w;
			int gh = 76;

			mp_tui_gauge(&sysinfo->screen.measureGauge, right, (w - gw) / 2, y, gw, gh,
				MP_TUI_GAUGE_THERMO, -20000, 85000, 1, "°C");
			mp_tui_gauge_zones(&sysinfo->screen.measureGauge, 50000, 70000);
			y += gh;
		}
		else {
			mp_tui_label_wrap(&sysinfo->screen.accelTemperature, right, 0, &y, w, line, font, "");
			mp_tui_label_color(&sysinfo->screen.accelTemperature, tui->theme.muted);
		}
	}
	y += 8;
	mp_tui_switch(&sysinfo->screen.accelSwitch, right, 0, y, w, tui->pointer ? line + 10 : font->lineHeight, "Enabled", sysinfo->accelEnabled == YES);
	sysinfo->screen.accelSwitch.onChange = _sysinfo_on_accel_switch;
	sysinfo->screen.accelSwitch.user = sysinfo;

	mp_tui_columns_fit(layout);
	sysinfo->screen.accelReady = 1;
	_sysinfo_accel_values(sysinfo);

	/* the page is only opened after this: the screen task starts the accelerometers then */
	if(sysinfo->screen.task)
		mp_task_wake(sysinfo->screen.task);
}

void _sysinfo_dev_accel_refresh(sysinfo_t *sysinfo) {
	if(sysinfo->screen.accelReady && _sysinfo_dev_accel_shown(sysinfo) == YES)
		_sysinfo_accel_values(sysinfo);
}

#endif
#endif
