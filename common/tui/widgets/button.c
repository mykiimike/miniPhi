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
@defgroup mpCommonTUIButton Button

@ingroup mpCommonTUIWidgets

@{
*/

static void _mp_tui_button_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_button_t *button = (mp_tui_button_t *)widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	const mp_tui_font_t *font = theme->font;
	mp_tui_rect_t *r = &widget->screen;
	mp_tui_color_t color = button->selected ? theme->accent : theme->foreground;
	mp_tui_color_t border = button->selected ? theme->accent : theme->border;
	int y = r->y + (r->h - font->lineHeight) / 2;
	int pad = r->h <= font->lineHeight + 2 ? 2 : 10;
	int mark = pad == 2 ? 10 : 18;

	if(button->plain) {
		mp_tui_draw_text(draw, font, r->x, y, button->text, button->pressed ? theme->accent : theme->titleForeground);
		return;
	}

	mp_tui_draw_fill(draw, r->x, r->y, r->w, r->h, button->pressed ? theme->titleBackground : theme->surface);
	mp_tui_draw_frame(draw, r->x, r->y, r->w, r->h, border);
	mp_tui_draw_text(draw, font, r->x + pad, y, button->text, color);
	if(button->chevron)
		mp_tui_draw_text(draw, font, r->x + r->w - mark, y, ">", button->selected ? theme->accent : theme->muted);
}

static int _mp_tui_button_event(mp_tui_widget_t *widget, const mp_tui_event_t *event) {
	mp_tui_button_t *button = (mp_tui_button_t *)widget;

	if(event->type != MP_TUI_EVENT_CLICK)
		return(0);
	if(button->onPress)
		button->onPress(button);
	return(1);
}

const mp_tui_class_t mp_tui_button_class = { "button", _mp_tui_button_draw, NULL, _mp_tui_button_event };

void mp_tui_button(mp_tui_button_t *button, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text) {
	button->onPress = NULL;
	button->user = NULL;
	button->id = 0;
	button->selected = 0;
	button->chevron = 0;
	button->plain = 0;
	button->pressed = 0;
	button->back = 0;
	snprintf(button->text, sizeof(button->text), "%s", text ? text : "");
	mp_tui_widget_init(&button->widget, &mp_tui_button_class, parent, x, y, w, h);
}

void mp_tui_button_back(mp_tui_button_t *button) {
	button->back = 1;
}

/**@}*/

#endif
