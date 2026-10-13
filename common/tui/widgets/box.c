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
@defgroup mpCommonTUIBox Box

@ingroup mpCommonTUIWidgets

@{
*/

static void _mp_tui_box_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_box_t *box = (mp_tui_box_t *)widget;
	mp_tui_rect_t *r = &widget->screen;

	mp_tui_draw_fill(draw, r->x, r->y, r->w, r->h, box->color);
	if(box->border)
		mp_tui_draw_frame(draw, r->x, r->y, r->w, r->h, box->borderColor);
}

const mp_tui_class_t mp_tui_box_class = { "box", _mp_tui_box_draw, NULL };

void mp_tui_box(mp_tui_box_t *box, mp_tui_widget_t *parent, int x, int y, int w, int h, mp_tui_color_t color) {
	box->color = color;
	box->borderColor = parent->tui->theme.border;
	box->border = 0;
	mp_tui_widget_init(&box->widget, &mp_tui_box_class, parent, x, y, w, h);
}

/**@}*/

#endif
