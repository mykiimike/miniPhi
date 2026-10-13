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
@defgroup mpCommonTUILabel Label

@ingroup mpCommonTUIWidgets

@{
*/

static void _mp_tui_label_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_label_t *label = (mp_tui_label_t *)widget;
	const mp_tui_font_t *font = _mp_tui_font(widget, label->font);
	mp_tui_rect_t *r = &widget->screen;
	const char *line = label->text, *p;
	int lines = 1, x, y, width;

	for(p=label->text; *p; p++)
		if(*p == '\n')
			lines++;

	/* the block of lines is centered vertically */
	y = r->y + (r->h - lines * font->lineHeight) / 2;
	while(1) {
		width = mp_tui_text_width(font, line);
		if(label->align == MP_TUI_ALIGN_CENTER)
			x = r->x + (r->w - width) / 2;
		else if(label->align == MP_TUI_ALIGN_RIGHT)
			x = r->x + r->w - width;
		else
			x = r->x;
		mp_tui_draw_text(draw, font, x, y, line, label->color);

		p = strchr(line, '\n');
		if(!p)
			break;
		line = p + 1;
		y += font->lineHeight;
	}
}

const mp_tui_class_t mp_tui_label_class = { "label", _mp_tui_label_draw, NULL };

void mp_tui_label(mp_tui_label_t *label, mp_tui_widget_t *parent, int x, int y, int w, int h, const char *text) {
	label->font = NULL;
	label->color = parent->tui->theme.foreground;
	label->align = MP_TUI_ALIGN_LEFT;
	snprintf(label->text, sizeof(label->text), "%s", text ? text : "");
	mp_tui_widget_init(&label->widget, &mp_tui_label_class, parent, x, y, w, h);
}

void _mp_tui_label_vset(mp_tui_label_t *label, const char *format, va_list args) {
	char text[MP_TUI_TEXT_MAX];

	vsnprintf(text, sizeof(text), format, args);
	if(strcmp(text, label->text) == 0)
		return;
	memcpy(label->text, text, sizeof(text));
	mp_tui_invalidate(&label->widget);
}

void mp_tui_label_set(mp_tui_label_t *label, const char *format, ...) {
	va_list args;

	va_start(args, format);
	_mp_tui_label_vset(label, format, args);
	va_end(args);
}

void mp_tui_label_font(mp_tui_label_t *label, const mp_tui_font_t *font) {
	if(label->font == font)
		return;
	label->font = font;
	mp_tui_invalidate(&label->widget);
}

void mp_tui_label_color(mp_tui_label_t *label, mp_tui_color_t color) {
	if(label->color == color)
		return;
	label->color = color;
	mp_tui_invalidate(&label->widget);
}

void mp_tui_label_align(mp_tui_label_t *label, mp_tui_align_t align) {
	if(label->align == align)
		return;
	label->align = align;
	mp_tui_invalidate(&label->widget);
}

/**@}*/

#endif
