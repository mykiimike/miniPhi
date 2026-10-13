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
@defgroup mpCommonTUIPage Page

@ingroup mpCommonTUIWidgets

@{
*/

static void _mp_tui_page_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_page_t *page = (mp_tui_page_t *)widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	mp_tui_rect_t *r = &widget->screen;
	int header = page->header;

	mp_tui_draw_fill(draw, r->x, r->y, r->w, r->h, theme->background);
	if(header > 0) {
		mp_tui_draw_fill(draw, r->x, r->y, r->w, header, theme->titleBackground);
		mp_tui_draw_hline(draw, r->x, r->y + header - 1, r->w, theme->border);
	}
}

const mp_tui_class_t mp_tui_page_class = { "page", _mp_tui_page_draw, NULL };

void mp_tui_page(mp_tui_page_t *page, mp_tui_widget_t *parent, int x, int y, int w, int h, int header) {
	page->header = header > 0 ? header : 0;
	mp_tui_widget_init(&page->widget, &mp_tui_page_class, parent, x, y, w, h);
}

/**@}*/

#endif
