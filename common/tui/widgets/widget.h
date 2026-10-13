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

#ifndef _MP_TUI_WIDGET_H
#define _MP_TUI_WIDGET_H

#include <mp.h>
#include <stdarg.h>

/**
@defgroup mpCommonTUIWidgets TUI things

@ingroup mpCommonTUI
*/

static inline const mp_tui_font_t *_mp_tui_font(mp_tui_widget_t *widget, const mp_tui_font_t *font) {
	return(font ? font : widget->tui->theme.font);
}

void _mp_tui_label_vset(mp_tui_label_t *label, const char *format, va_list args);

#endif
