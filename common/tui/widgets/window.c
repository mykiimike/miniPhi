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
#include <stdarg.h>
#include <stdio.h>

#ifdef SUPPORT_COMMON_TUI

#include "widget.h"

/**
@defgroup mpCommonTUIWindow Window

@ingroup mpCommonTUIWidgets

@{
*/

static int _mp_tui_window_title_height(mp_tui_window_t *window) {
	return(_mp_tui_font(&window->widget, window->font)->lineHeight + 4);
}

static void _mp_tui_window_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_window_t *window = (mp_tui_window_t *)widget;
	const mp_tui_font_t *font = _mp_tui_font(widget, window->font);
	mp_tui_theme_t *theme = &widget->tui->theme;
	mp_tui_rect_t *r = &widget->screen;
	int title = _mp_tui_window_title_height(window);
	int width = mp_tui_text_width(font, window->title);
	int x = r->x + (r->w - width) / 2;

	mp_tui_draw_fill(draw, r->x + 1, r->y + 1, r->w - 2, title - 1, theme->titleBackground);
	mp_tui_draw_fill(draw, r->x + 1, r->y + title, r->w - 2, r->h - title - 1, theme->surface);
	mp_tui_draw_hline(draw, r->x + 1, r->y + title, r->w - 2, theme->border);
	mp_tui_draw_frame(draw, r->x, r->y, r->w, r->h, theme->border);
	if(x < r->x + 4)
		x = r->x + 4;
	mp_tui_draw_text(draw, font, x, r->y + 2, window->title, theme->titleForeground);
}

static void _mp_tui_window_client(mp_tui_widget_t *widget, mp_tui_rect_t *rect) {
	int title = _mp_tui_window_title_height((mp_tui_window_t *)widget);

	rect->x = 1;
	rect->y = title + 1;
	rect->w = widget->rect.w - 2;
	rect->h = widget->rect.h - title - 2;
	if(rect->w < 0) rect->w = 0;
	if(rect->h < 0) rect->h = 0;
}

const mp_tui_class_t mp_tui_window_class = { "window", _mp_tui_window_draw, _mp_tui_window_client };

void mp_tui_window(mp_tui_window_t *window, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *title) {
	window->font = NULL;
	snprintf(window->title, sizeof(window->title), "%s", title ? title : "");
	mp_tui_widget_init(&window->widget, &mp_tui_window_class, parent, x, y, w, h);
}

void mp_tui_window_title(mp_tui_window_t *window, const char *format, ...) {
	char title[MP_TUI_TEXT_MAX];
	va_list args;

	va_start(args, format);
	vsnprintf(title, sizeof(title), format, args);
	va_end(args);
	if(strcmp(title, window->title) == 0)
		return;
	memcpy(window->title, title, sizeof(title));
	mp_tui_invalidate(&window->widget);
}

/**@}*/

#endif
