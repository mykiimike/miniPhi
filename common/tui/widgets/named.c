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

#ifdef SUPPORT_COMMON_TUI

#include "widget.h"

/**
@defgroup mpCommonTUINamed Widgets by name

@ingroup mpCommonTUIWidgets

@{
*/

int mp_tui_set_text(mp_tui_t *ui, const char *name, const char *format, ...) {
	mp_tui_widget_t *widget = mp_tui_find(ui, name);
	va_list args;

	if(!widget || widget->type != &mp_tui_label_class)
		return(0);
	va_start(args, format);
	_mp_tui_label_vset((mp_tui_label_t *)widget, format, args);
	va_end(args);
	return(1);
}

int mp_tui_set_value(mp_tui_t *ui, const char *name, int value) {
	mp_tui_widget_t *widget = mp_tui_find(ui, name);

	if(widget && widget->type == &mp_tui_switch_class) {
		mp_tui_switch_set((mp_tui_switch_t *)widget, value);
		return(1);
	}
	if(!widget || widget->type != &mp_tui_bar_class)
		return(0);
	mp_tui_bar_set((mp_tui_bar_t *)widget, value);
	return(1);
}

/**@}*/

#endif
