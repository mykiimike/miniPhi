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

#ifdef SUPPORT_COMMON_TUI

/**
@defgroup mpCommonTUIBar Bar

@ingroup mpCommonTUIWidgets

@{
*/

static void _mp_tui_bar_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_bar_t *bar = (mp_tui_bar_t *)widget;
	mp_tui_rect_t *r = &widget->screen;
	int inner = r->w - 2, fill = 0;

	if(bar->max > 0 && inner > 0)
		fill = (int)((long)inner * bar->value / bar->max);

	mp_tui_draw_frame(draw, r->x, r->y, r->w, r->h, widget->tui->theme.border);
	mp_tui_draw_fill(draw, r->x + 1, r->y + 1, fill, r->h - 2, bar->color);
	mp_tui_draw_fill(draw, r->x + 1 + fill, r->y + 1, inner - fill, r->h - 2, bar->background);
}

const mp_tui_class_t mp_tui_bar_class = { "bar", _mp_tui_bar_draw, NULL };

void mp_tui_bar(mp_tui_bar_t *bar, mp_tui_widget_t *parent, int x, int y, int w, int h, int max) {
	bar->color = parent->tui->theme.accent;
	bar->background = parent->tui->theme.surface;
	bar->value = 0;
	bar->max = max;
	mp_tui_widget_init(&bar->widget, &mp_tui_bar_class, parent, x, y, w, h);
}

void mp_tui_bar_set(mp_tui_bar_t *bar, int value) {
	if(value < 0)
		value = 0;
	if(value > bar->max)
		value = bar->max;
	if(bar->value == value)
		return;
	bar->value = value;
	mp_tui_invalidate(&bar->widget);
}

/**@}*/

#endif
