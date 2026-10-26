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
@defgroup mpCommonTUISwitch Switch

@ingroup mpCommonTUIWidgets

@{
*/

static void _mp_tui_switch_disc(mp_tui_draw_t *draw, int cx, int cy, int radius, mp_tui_color_t color) {
	int dy, span;

	for(dy = -radius; dy <= radius; dy++) {
		span = 0;
		while((span + 1) * (span + 1) + dy * dy <= radius * radius)
			span++;
		mp_tui_draw_hline(draw, cx - span, cy + dy, span * 2 + 1, color);
	}
}

static void _mp_tui_switch_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_switch_t *sw = (mp_tui_switch_t *)widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	const mp_tui_font_t *font = theme->font;
	mp_tui_rect_t *r = &widget->screen;
	int th = font->lineHeight + 4;
	int radius, tw, tx, cy, knob;
	mp_tui_color_t track;

	if(th > r->h)
		th = r->h;
	radius = th / 2;
	if(radius < 3)
		return;
	tw = radius * 4;
	tx = r->x + r->w - tw - 2;
	cy = r->y + r->h / 2;
	track = sw->on ? theme->accent : theme->border;

	mp_tui_draw_text(draw, font, r->x, r->y + (r->h - font->lineHeight) / 2, sw->text,
		widget->tui->focused == widget ? theme->accent : theme->foreground);
	if(widget->tui->focused == widget)
		mp_tui_draw_frame(draw, r->x, r->y, r->w, r->h, theme->accent);
	_mp_tui_switch_disc(draw, tx + radius, cy, radius, track);
	_mp_tui_switch_disc(draw, tx + tw - radius, cy, radius, track);
	mp_tui_draw_fill(draw, tx + radius, cy - radius, tw - radius * 2, radius * 2 + 1, track);
	knob = sw->on ? tx + tw - radius : tx + radius;
	_mp_tui_switch_disc(draw, knob, cy, radius - 2, sw->on ? theme->titleForeground : theme->muted);
}

static int _mp_tui_switch_event(mp_tui_widget_t *widget, const mp_tui_event_t *event) {
	mp_tui_switch_t *sw = (mp_tui_switch_t *)widget;

	if(event->type != MP_TUI_EVENT_CLICK)
		return(0);
	sw->on = !sw->on;
	mp_tui_invalidate(widget);
	if(sw->onChange)
		sw->onChange(sw);
	return(1);
}

const mp_tui_class_t mp_tui_switch_class = { "switch", _mp_tui_switch_draw, NULL, _mp_tui_switch_event };

void mp_tui_switch(mp_tui_switch_t *sw, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text, int on) {
	sw->on = on ? 1 : 0;
	sw->onChange = NULL;
	sw->user = NULL;
	snprintf(sw->text, sizeof(sw->text), "%s", text ? text : "");
	mp_tui_widget_init(&sw->widget, &mp_tui_switch_class, parent, x, y, w, h);
}

void mp_tui_switch_set(mp_tui_switch_t *sw, int on) {
	on = on ? 1 : 0;
	if(sw->on == on)
		return;
	sw->on = on;
	mp_tui_invalidate(&sw->widget);
}

/**@}*/

#endif
