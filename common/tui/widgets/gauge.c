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
#include <stdio.h>

#ifdef SUPPORT_COMMON_TUI

/**
@defgroup mpCommonTUIGauge Gauge

@ingroup mpCommonTUIWidgets

@{
*/

/* sin of 0, 5, ... 90 degrees, in 1/1024. The dial has no libm. */
static const short _mp_tui_sin5[19] = {
	0, 89, 178, 265, 350, 433, 512, 587, 658, 724,
	784, 839, 887, 928, 962, 989, 1008, 1020, 1024
};

static int _mp_tui_isqrt(int n) {
	int root = 0;
	int bit = 1 << 30;

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

static void _mp_tui_gauge_plot(mp_tui_draw_t *draw, int x, int y, mp_tui_color_t color) {
	if(x < draw->clip.x || y < draw->clip.y)
		return;
	if(x >= draw->clip.x + draw->clip.w || y >= draw->clip.y + draw->clip.h)
		return;
	draw->pixels[(y - draw->band.y) * draw->band.w + (x - draw->band.x)] = color;
}

static void _mp_tui_gauge_disc(mp_tui_draw_t *draw, int cx, int cy, int radius, mp_tui_color_t color) {
	int dy, span;
	int y0 = draw->clip.y;
	int y1 = draw->clip.y + draw->clip.h;

	if(radius < 0)
		return;
	for(dy = -radius; dy <= radius; dy++) {
		if(cy + dy < y0 || cy + dy >= y1)
			continue;
		span = _mp_tui_isqrt(radius * radius - dy * dy);
		mp_tui_draw_hline(draw, cx - span, cy + dy, span * 2 + 1, color);
	}
}

static int _mp_tui_sin_deg(int deg) {
	int step, frac, s0, s1;

	if(deg <= 0)
		return(0);
	if(deg >= 90)
		return(1024);
	step = deg / 5;
	frac = deg - step * 5;
	s0 = _mp_tui_sin5[step];
	s1 = _mp_tui_sin5[step + 1];
	return(s0 + (s1 - s0) * frac / 5);
}

static void _mp_tui_sincos(int deg, int *sin_out, int *cos_out) {
	int d = deg % 360;

	if(d < 0)
		d += 360;
	if(d <= 90) {
		*sin_out = _mp_tui_sin_deg(d);
		*cos_out = _mp_tui_sin_deg(90 - d);
	}
	else if(d <= 180) {
		*sin_out = _mp_tui_sin_deg(180 - d);
		*cos_out = -_mp_tui_sin_deg(d - 90);
	}
	else if(d <= 270) {
		*sin_out = -_mp_tui_sin_deg(d - 180);
		*cos_out = -_mp_tui_sin_deg(270 - d);
	}
	else {
		*sin_out = -_mp_tui_sin_deg(360 - d);
		*cos_out = _mp_tui_sin_deg(d - 270);
	}
}

static int _mp_tui_atan_quarter(int ay, int ax) {
	int lo = 0, hi = 90;

	if(ax <= 0)
		return(90);
	if(ay <= 0)
		return(0);
	while(hi - lo > 1) {
		int mid = (lo + hi) >> 1;
		int s = _mp_tui_sin_deg(mid);
		int c = _mp_tui_sin_deg(90 - mid);

		if((long)s * ax < (long)c * ay)
			lo = mid;
		else
			hi = mid;
	}
	return(hi);
}

/* 0 is east, 90 is north, y grows up */
static int _mp_tui_atan2_deg(int y, int x) {
	int ax = x < 0 ? -x : x;
	int ay = y < 0 ? -y : y;
	int deg = _mp_tui_atan_quarter(ay, ax);

	if(x >= 0 && y >= 0)
		return(deg);
	if(x < 0 && y >= 0)
		return(180 - deg);
	if(x < 0)
		return(180 + deg);
	if(deg == 0)
		return(0);
	return(360 - deg);
}

/*
 * Clockwise from 210 degrees (bottom left) through the top to 330
 * (bottom right). The return is 0..240, or -1 in the bottom gap.
 */
static int _mp_tui_dial_along(int deg) {
	if(deg > 210 && deg < 330)
		return(-1);
	if(deg >= 330)
		return(570 - deg);
	return(210 - deg);
}

static void _mp_tui_dial_tip(int cx, int cy, int radius, int along, int *x, int *y) {
	int deg = 210 - along;
	int s, c;

	if(deg < 0)
		deg += 360;
	_mp_tui_sincos(deg, &s, &c);
	*x = cx + (int)((long)c * radius / 1024);
	*y = cy - (int)((long)s * radius / 1024);
}

static void _mp_tui_gauge_format(char *dst, int cap, long milli, int decimals, const char *unit, int valid) {
	long scale = 1, divisor = 1, rounded, whole, frac;
	int neg = 0, n;
	const char *gap;

	if(!dst || cap < 2)
		return;
	if(!valid) {
		if(unit && unit[0])
			snprintf(dst, (size_t)cap, "-- %s", unit);
		else
			snprintf(dst, (size_t)cap, "--");
		return;
	}
	if(decimals < 0)
		decimals = 0;
	if(decimals > 3)
		decimals = 3;
	for(n = 0; n < decimals; n++)
		scale *= 10;
	for(n = decimals; n < 3; n++)
		divisor *= 10;
	if(milli < 0) {
		neg = 1;
		milli = -milli;
	}
	rounded = (milli + divisor / 2) / divisor;
	whole = rounded / scale;
	frac = rounded % scale;
	if(whole == 0 && frac == 0)
		neg = 0;
	gap = unit && unit[0] ? " " : "";
	if(!unit)
		unit = "";
	if(decimals == 0)
		snprintf(dst, (size_t)cap, "%s%ld%s%s", neg ? "-" : "", whole, gap, unit);
	else if(decimals == 1)
		snprintf(dst, (size_t)cap, "%s%ld.%ld%s%s", neg ? "-" : "", whole, frac, gap, unit);
	else if(decimals == 2)
		snprintf(dst, (size_t)cap, "%s%ld.%02ld%s%s", neg ? "-" : "", whole, frac, gap, unit);
	else
		snprintf(dst, (size_t)cap, "%s%ld.%03ld%s%s", neg ? "-" : "", whole, frac, gap, unit);
}

static mp_tui_color_t _mp_tui_gauge_zone(const mp_tui_gauge_t *gauge, long at, const mp_tui_theme_t *theme) {
	if(gauge->warn == gauge->max && gauge->alarm == gauge->max)
		return(theme->accent);
	if(at < gauge->warn)
		return(MP_TUI_RGB(60, 200, 90));
	if(at <= gauge->alarm)
		return(MP_TUI_RGB(240, 180, 40));
	return(MP_TUI_RGB(230, 60, 50));
}

/* clamps into *value; 0 when there is nothing to show */
static int _mp_tui_gauge_read(const mp_tui_gauge_t *gauge, long *value) {
	long v;

	if(!gauge->valid || gauge->max <= gauge->min)
		return(0);
	v = gauge->value;
	if(v < gauge->min)
		v = gauge->min;
	if(v > gauge->max)
		v = gauge->max;
	*value = v;
	return(1);
}

static int _mp_tui_gauge_needle(const mp_tui_gauge_t *gauge, long value) {
	long span = gauge->max - gauge->min;

	if(span <= 0)
		return(0);
	return((int)((value - gauge->min) * 240 / span));
}

static void _mp_tui_gauge_center(mp_tui_draw_t *draw, const mp_tui_font_t *font, int cx, int y, const char *text, mp_tui_color_t color) {
	int width = mp_tui_text_width(font, text);

	mp_tui_draw_text(draw, font, cx - width / 2, y, text, color);
}

static void _mp_tui_gauge_arc_row(mp_tui_draw_t *draw, int y, int cx, int cy, int inner, int outer, int needle, mp_tui_color_t fill, mp_tui_color_t track) {
	int dy = y - cy;
	int dy2 = dy * dy;
	int o2 = outer * outer;
	int i2 = inner * inner;
	int ox, ix, x, x0, x1;
	int left = draw->clip.x;
	int right = draw->clip.x + draw->clip.w - 1;

	if(dy2 > o2 || outer <= 0)
		return;
	ox = _mp_tui_isqrt(o2 - dy2) + 1;
	if(dy2 >= i2)
		ix = 0;
	else {
		ix = _mp_tui_isqrt(i2 - dy2);
		if((long)ix * ix < (long)i2 - dy2)
			ix++;
	}

	x0 = cx - ox;
	x1 = cx - ix;
	if(x0 < left)
		x0 = left;
	if(x1 > right)
		x1 = right;
	for(x = x0; x <= x1; x++) {
		int dx = x - cx;
		long r2 = (long)dx * dx + (long)dy2;
		int along;

		if(r2 < i2 || r2 > o2)
			continue;
		along = _mp_tui_dial_along(_mp_tui_atan2_deg(cy - y, dx));
		if(along < 0)
			continue;
		_mp_tui_gauge_plot(draw, x, y, needle >= 0 && along <= needle ? fill : track);
	}
	x0 = ix > 0 ? cx + ix : cx + 1;
	x1 = cx + ox;
	if(x0 < left)
		x0 = left;
	if(x1 > right)
		x1 = right;
	for(x = x0; x <= x1; x++) {
		int dx = x - cx;
		long r2 = (long)dx * dx + (long)dy2;
		int along;

		if(r2 < i2 || r2 > o2)
			continue;
		along = _mp_tui_dial_along(_mp_tui_atan2_deg(cy - y, dx));
		if(along < 0)
			continue;
		_mp_tui_gauge_plot(draw, x, y, needle >= 0 && along <= needle ? fill : track);
	}
}

static void _mp_tui_gauge_dial(mp_tui_gauge_t *gauge, mp_tui_draw_t *draw) {
	mp_tui_widget_t *widget = &gauge->widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	const mp_tui_font_t *font = theme->font;
	mp_tui_rect_t *r = &widget->screen;
	char text[32];
	long clamped = gauge->value;
	int valid = _mp_tui_gauge_read(gauge, &clamped);
	int label_h, top, bottom, radius, thick, inner, outer, mid;
	int cx, cy, y, y0, y1, needle, tx, ty;
	mp_tui_color_t fill, track;

	if(!font)
		return;
	label_h = (gauge->label && gauge->label[0]) ? font->lineHeight : 0;
	top = r->y + 2;
	bottom = r->y + r->h - 2 - label_h;
	radius = (r->w - 8) / 2;
	if(radius > (bottom - top) * 2 / 3)
		radius = (bottom - top) * 2 / 3;
	_mp_tui_gauge_format(text, (int)sizeof(text), gauge->value, gauge->decimals, gauge->unit, valid);
	fill = valid ? _mp_tui_gauge_zone(gauge, gauge->value, theme) : theme->muted;
	track = theme->muted;
	needle = valid ? _mp_tui_gauge_needle(gauge, clamped) : -1;

	if(radius >= 16) {
		thick = radius >= 36 ? 6 : 5;
		if(thick >= radius)
			thick = radius / 2;
		outer = radius;
		inner = radius - thick;
		if(inner < 2)
			inner = 2;
		mid = (inner + outer) / 2;
		cx = r->x + r->w / 2;
		cy = top + radius;
		if(cy + radius / 2 > bottom - 1)
			cy = bottom - 1 - radius / 2;

		y0 = draw->clip.y;
		y1 = draw->clip.y + draw->clip.h;
		if(y0 < cy - outer - 1)
			y0 = cy - outer - 1;
		if(y1 > cy + outer + 2)
			y1 = cy + outer + 2;
		for(y = y0; y < y1; y++)
			_mp_tui_gauge_arc_row(draw, y, cx, cy, inner, outer, needle, fill, track);

		if(thick >= 4) {
			_mp_tui_dial_tip(cx, cy, mid, 0, &tx, &ty);
			_mp_tui_gauge_disc(draw, tx, ty, thick / 2, needle >= 0 ? fill : track);
			_mp_tui_dial_tip(cx, cy, mid, 240, &tx, &ty);
			_mp_tui_gauge_disc(draw, tx, ty, thick / 2, needle >= 240 ? fill : track);
		}
		{
			int x2, y2;

			_mp_tui_dial_tip(cx, cy, inner - 1, 0, &tx, &ty);
			_mp_tui_dial_tip(cx, cy, outer + 3, 0, &x2, &y2);
			mp_tui_draw_line(draw, tx, ty, x2, y2, theme->foreground);
			_mp_tui_dial_tip(cx, cy, inner - 1, 120, &tx, &ty);
			_mp_tui_dial_tip(cx, cy, outer + 3, 120, &x2, &y2);
			mp_tui_draw_line(draw, tx, ty, x2, y2, theme->foreground);
			_mp_tui_dial_tip(cx, cy, inner - 1, 240, &tx, &ty);
			_mp_tui_dial_tip(cx, cy, outer + 3, 240, &x2, &y2);
			mp_tui_draw_line(draw, tx, ty, x2, y2, theme->foreground);
		}

		if(needle >= 0) {
			int x0, y_0, x1, y_1, ax, ay, px = 0, py = 0;

			_mp_tui_dial_tip(cx, cy, 4, needle, &x0, &y_0);
			_mp_tui_dial_tip(cx, cy, mid, needle, &x1, &y_1);
			ax = x1 >= x0 ? x1 - x0 : x0 - x1;
			ay = y_1 >= y_0 ? y_1 - y_0 : y_0 - y_1;
			if(ax >= ay)
				py = 1;
			else
				px = 1;
			mp_tui_draw_line(draw, x0, y_0, x1, y_1, theme->foreground);
			mp_tui_draw_line(draw, x0 + px, y_0 + py, x1 + px, y_1 + py, theme->foreground);
		}
		_mp_tui_gauge_disc(draw, cx, cy, 4, theme->foreground);
		_mp_tui_gauge_disc(draw, cx, cy, 2, theme->surface);
		ty = cy + 6;
		if(ty + font->lineHeight > bottom)
			ty = bottom - font->lineHeight;
		if(ty < top)
			ty = top;
		_mp_tui_gauge_center(draw, font, cx, ty, text, fill);
	}
	else
		_mp_tui_gauge_center(draw, font, r->x + r->w / 2, r->y + (r->h - label_h - font->lineHeight) / 2, text, fill);

	if(label_h)
		_mp_tui_gauge_center(draw, font, r->x + r->w / 2, r->y + r->h - font->lineHeight, gauge->label, theme->muted);
}

static void _mp_tui_gauge_thermo(mp_tui_gauge_t *gauge, mp_tui_draw_t *draw) {
	mp_tui_widget_t *widget = &gauge->widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	const mp_tui_font_t *font = theme->font;
	mp_tui_rect_t *r = &widget->screen;
	char text[32];
	long value = gauge->value;
	int valid = _mp_tui_gauge_read(gauge, &value);
	int label_h, tw, bulb, tube_w, body, left, cx, top, bottom, cy;
	int tube_x, tube_top, tube_bot, text_x, i;
	mp_tui_color_t color;

	if(!font || r->w < 8 || r->h < 16)
		return;
	label_h = (gauge->label && gauge->label[0]) ? font->lineHeight : 0;
	_mp_tui_gauge_format(text, (int)sizeof(text), gauge->value, gauge->decimals, gauge->unit, valid);
	tw = mp_tui_text_width(font, text);
	color = valid ? _mp_tui_gauge_zone(gauge, gauge->value, theme) : theme->muted;

	bulb = (r->h - label_h) / 6;
	if(bulb > 11)
		bulb = 11;
	if(bulb < 6)
		bulb = 6;
	if(bulb * 2 > r->h - label_h - 6)
		bulb = (r->h - label_h - 6) / 2;
	if(bulb < 4)
		return;
	tube_w = bulb - 2;
	if(tube_w < 4)
		tube_w = 4;

	body = bulb * 2 + 12 + tw;
	left = r->x + (r->w - body) / 2;
	if(left < r->x + 4)
		left = r->x + 4;
	cx = left + bulb;
	text_x = cx + bulb + 8;
	top = r->y + 2;
	bottom = r->y + r->h - 2 - label_h;
	cy = bottom - bulb;
	tube_x = cx - tube_w / 2;
	tube_top = top;
	tube_bot = cy;
	if(tube_bot - tube_top < 4)
		tube_top = tube_bot - 4;

	mp_tui_draw_fill(draw, tube_x, tube_top, tube_w, tube_bot - tube_top, theme->background);
	if(valid && gauge->max > gauge->min) {
		int span = tube_bot - tube_top;
		int fill = (int)((value - gauge->min) * span / (gauge->max - gauge->min));

		if(fill < 0)
			fill = 0;
		if(fill > span)
			fill = span;
		if(fill > 0 && tube_w > 2)
			mp_tui_draw_fill(draw, tube_x + 1, tube_bot - fill, tube_w - 2, fill, color);
	}
	mp_tui_draw_frame(draw, tube_x, tube_top, tube_w, tube_bot - tube_top + bulb / 2, theme->border);
	_mp_tui_gauge_disc(draw, cx, cy, bulb, theme->border);
	_mp_tui_gauge_disc(draw, cx, cy, bulb - 2, valid ? color : theme->background);

	{
		int tick_bot = cy - bulb;

		if(tick_bot < tube_top)
			tick_bot = tube_top;
		for(i = 0; i < 4; i++) {
			int ty = tube_top + (tick_bot - tube_top) * i / 3;

			if(tube_x >= r->x + 4)
				mp_tui_draw_hline(draw, tube_x - 4, ty, 3, theme->muted);
		}
	}
	mp_tui_draw_text(draw, font, text_x, top + (bottom - top - font->lineHeight) / 2, text, color);
	if(label_h)
		_mp_tui_gauge_center(draw, font, r->x + r->w / 2, r->y + r->h - font->lineHeight, gauge->label, theme->muted);
}

static void _mp_tui_gauge_bar(mp_tui_gauge_t *gauge, mp_tui_draw_t *draw) {
	mp_tui_widget_t *widget = &gauge->widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	const mp_tui_font_t *font = theme->font;
	mp_tui_rect_t *r = &widget->screen;
	char text[32], end[32];
	long value = gauge->value;
	int valid = _mp_tui_gauge_read(gauge, &value);
	int line, segs, gaps, segw, used, x, bar_y, bar_h, filled, i, tw;
	mp_tui_color_t color;

	if(!font || r->w < 8 || r->h < 4)
		return;
	line = font->lineHeight;
	_mp_tui_gauge_format(text, (int)sizeof(text), gauge->value, gauge->decimals, gauge->unit, valid);
	color = valid ? _mp_tui_gauge_zone(gauge, gauge->value, theme) : theme->muted;
	tw = mp_tui_text_width(font, text);
	x = r->x + r->w - tw;
	if(x < r->x)
		x = r->x;
	mp_tui_draw_text(draw, font, x, r->y, text, color);

	segs = 16;
	if(r->w < 140)
		segs = 12;
	if(r->w < 90)
		segs = 10;
	gaps = segs - 1;
	segw = (r->w - gaps) / segs;
	while(segw < 2 && segs > 4) {
		segs--;
		gaps = segs - 1;
		segw = (r->w - gaps) / segs;
	}
	if(segw < 1)
		return;
	used = segs * segw + gaps;
	x = r->x + (r->w - used) / 2;
	bar_y = r->y + line + 2;
	bar_h = r->h - line * 2 - 6;
	if(bar_h < 6)
		bar_h = 6;

	filled = 0;
	if(valid) {
		long span = gauge->max - gauge->min;

		filled = (int)(((value - gauge->min) * segs + span - 1) / span);
		if(filled < 0)
			filled = 0;
		if(filled > segs)
			filled = segs;
	}
	for(i = 0; i < segs; i++) {
		mp_tui_color_t block = theme->background;
		int atx = x + i * (segw + 1);

		if(i < filled) {
			long at = gauge->min + (gauge->max - gauge->min) * (i + 1) / segs;

			block = _mp_tui_gauge_zone(gauge, at, theme);
		}
		mp_tui_draw_fill(draw, atx, bar_y, segw, bar_h, block);
	}
	mp_tui_draw_vline(draw, x, bar_y + bar_h, 3, theme->muted);
	mp_tui_draw_vline(draw, x + used - 1, bar_y + bar_h, 3, theme->muted);

	_mp_tui_gauge_format(end, (int)sizeof(end), gauge->min, gauge->decimals, NULL, 1);
	mp_tui_draw_text(draw, font, r->x, r->y + r->h - line, end, theme->muted);
	_mp_tui_gauge_format(end, (int)sizeof(end), gauge->max, gauge->decimals, NULL, 1);
	tw = mp_tui_text_width(font, end);
	x = r->x + r->w - tw;
	if(x < r->x)
		x = r->x;
	mp_tui_draw_text(draw, font, x, r->y + r->h - line, end, theme->muted);
}

static void _mp_tui_gauge_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_gauge_t *gauge = (mp_tui_gauge_t *)widget;

	if(gauge->style == MP_TUI_GAUGE_THERMO)
		_mp_tui_gauge_thermo(gauge, draw);
	else if(gauge->style == MP_TUI_GAUGE_BAR)
		_mp_tui_gauge_bar(gauge, draw);
	else
		_mp_tui_gauge_dial(gauge, draw);
}

const mp_tui_class_t mp_tui_gauge_class = { "gauge", _mp_tui_gauge_draw, NULL, NULL };

void mp_tui_gauge(mp_tui_gauge_t *gauge, mp_tui_widget_t *parent, int x, int y, int w, int h, mp_tui_gauge_style_t style, long min, long max, unsigned char decimals, const char *unit) {
	if(decimals > 3)
		decimals = 3;
	gauge->style = style;
	gauge->value = min;
	gauge->min = min;
	gauge->max = max;
	gauge->warn = max;
	gauge->alarm = max;
	gauge->decimals = decimals;
	gauge->unit = unit;
	gauge->label = NULL;
	gauge->valid = 0;
	mp_tui_widget_init(&gauge->widget, &mp_tui_gauge_class, parent, x, y, w, h);
}

void mp_tui_gauge_zones(mp_tui_gauge_t *gauge, long warn, long alarm) {
	if(gauge->warn == warn && gauge->alarm == alarm)
		return;
	gauge->warn = warn;
	gauge->alarm = alarm;
	mp_tui_invalidate(&gauge->widget);
}

void mp_tui_gauge_label(mp_tui_gauge_t *gauge, const char *label) {
	if(gauge->label == label)
		return;
	gauge->label = label;
	mp_tui_invalidate(&gauge->widget);
}

void mp_tui_gauge_set(mp_tui_gauge_t *gauge, long value, int valid) {
	valid = valid ? 1 : 0;
	if(gauge->value == value && gauge->valid == valid)
		return;
	gauge->value = value;
	gauge->valid = valid;
	mp_tui_invalidate(&gauge->widget);
}

/**@}*/

#endif
