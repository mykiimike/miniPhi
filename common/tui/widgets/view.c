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

#include "widget.h"

/**
@defgroup mpCommonTUIView View

@ingroup mpCommonTUIWidgets

@{
*/

static int _mp_tui_view_event(mp_tui_widget_t *widget, const mp_tui_event_t *event) {
	if(event->type != MP_TUI_EVENT_WHEEL)
		return(0);
	mp_tui_view_scroll((mp_tui_view_t *)widget, -event->value * 3 * _mp_tui_font(widget, NULL)->lineHeight);
	return(1);
}

const mp_tui_class_t mp_tui_view_class = { "view", NULL, NULL, _mp_tui_view_event };

void mp_tui_view(mp_tui_view_t *view, mp_tui_widget_t *parent, int x, int y, int w, int h) {
	view->scroll = 0;
	view->pending = 0;
	mp_tui_widget_init(&view->widget, &mp_tui_view_class, parent, x, y, w, h);
}

/**@}*/

#endif
