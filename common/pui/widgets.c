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

#ifdef SUPPORT_COMMON_PUI

static int _mp_pui_line(void) {
	int line = mp_pui_font_default.lineHeight;

	return(line > 0 ? line : 1);
}

static void _mp_pui_copy(char *dst, int cap, const char *src) {
	int index;

	if(!src)
		src = "";
	for(index = 0; index < cap - 1 && src[index]; index++)
		dst[index] = src[index];
	dst[index] = 0;
}

static void _mp_pui_changed(mp_pui_widget_t *widget) {
	if(widget->page && widget->page->ui && widget->page->ui->current == widget->page)
		mp_pui_refresh_rows(widget->page->ui, widget->y, widget->y + widget->h - 1);
}

void mp_pui_page_add(mp_pui_page_t *page, mp_pui_widget_t *widget) {
	widget->page = page;
	widget->item.next = NULL;
	widget->item.prev = NULL;
	mp_list_add_last(&page->widgets, &widget->item, widget);
}

void mp_pui_paint_text(unsigned char *buffer, int bufW, int bpp, int x, int right, int destY, int lineRow, const mp_pui_font_t *font, const char *text, mp_pui_color_t color) {
	const char *cursor;
	int pen;

	if(!buffer || !font || !text || destY < 0)
		return;
	cursor = text;
	pen = x;
	while(*cursor && *cursor != '\n' && pen < right) {
		uint32_t code = mp_pui_utf8_next(&cursor);
		const mp_pui_glyph_t *glyph = mp_pui_font_glyph(font, code);
		int advance = glyph ? glyph->advance : font->size / 2 + 1;
		int grow, column;

		if(glyph && glyph->w) {
			grow = lineRow - (int)font->baseline - (int)glyph->dy;
			if(grow >= 0 && grow < glyph->h) {
				const uint8_t *bits = font->bitmap + glyph->offset;

				for(column = 0; column < glyph->w; column++) {
					int px = pen + glyph->dx + column;
					unsigned int index;

					if(px < 0 || px >= right || px >= bufW)
						continue;
					index = (unsigned int)(grow * glyph->w + column);
					if(bits[index >> 3] & (uint8_t)(0x80 >> (index & 7)))
						mp_pui_put(buffer, bufW, bpp, px, destY, color);
				}
			}
		}
		pen += advance;
	}
}

void mp_pui_draw_text(unsigned char *buffer, int bufW, int bpp, int x, int y, const mp_pui_font_t *font, const char *text, mp_pui_color_t color) {
	int row;

	if(!font)
		return;
	for(row = 0; row < font->lineHeight; row++)
		mp_pui_paint_text(buffer, bufW, bpp, x, bufW, y + row, row, font, text, color);
}

static int _mp_pui_text_x(mp_pui_widget_t *widget, const mp_pui_font_t *font, const char *text, int align) {
	int width = mp_pui_text_width(font, text);
	int x = widget->x;

	if(align == MP_PUI_ALIGN_CENTER)
		x += (widget->w - width) / 2;
	else if(align == MP_PUI_ALIGN_RIGHT)
		x += widget->w - width;
	if(x < widget->x)
		x = widget->x;
	return(x);
}

static int _mp_pui_line_row(mp_pui_widget_t *widget, const mp_pui_font_t *font, int local) {
	int line = font->lineHeight > 0 ? font->lineHeight : 1;
	int offset = widget->h > line ? (widget->h - line) / 2 : 0;
	int row = local - offset;

	if(row < 0 || row >= line)
		return(-1);
	return(row);
}

static void _mp_pui_label_draw(mp_pui_label_t *label, unsigned char *buffer, int bufW, int bpp, int local) {
	int row = _mp_pui_line_row(&label->widget, &mp_pui_font_default, local);

	if(row < 0)
		return;
	mp_pui_paint_text(buffer, bufW, bpp,
		_mp_pui_text_x(&label->widget, &mp_pui_font_default, label->text, label->align),
		label->widget.x + label->widget.w, 0, row, &mp_pui_font_default, label->text, label->fg);
}

static const mp_pui_font_t *_mp_pui_big_font(mp_pui_big_t *big) {
	return(big->font ? big->font : &mp_pui_font_bold);
}

static void _mp_pui_big_draw(mp_pui_big_t *big, unsigned char *buffer, int bufW, int bpp, int local) {
	const mp_pui_font_t *font = _mp_pui_big_font(big);
	int row = _mp_pui_line_row(&big->widget, font, local);

	if(row < 0)
		return;
	mp_pui_paint_text(buffer, bufW, bpp,
		_mp_pui_text_x(&big->widget, font, big->text, MP_PUI_ALIGN_CENTER),
		big->widget.x + big->widget.w, 0, row, font, big->text, big->fg);
}

static void _mp_pui_value_draw(mp_pui_value_t *value, unsigned char *buffer, int bufW, int bpp, int local) {
	const mp_pui_font_t *font = &mp_pui_font_default;
	int row = _mp_pui_line_row(&value->widget, font, local);
	int capW, valW, left, right, dot;

	if(row < 0)
		return;
	capW = mp_pui_text_width(font, value->caption);
	valW = mp_pui_text_width(font, value->value);
	left = value->widget.x;
	right = value->widget.x + value->widget.w - valW;
	if(right < left + capW)
		right = left + capW;
	mp_pui_paint_text(buffer, bufW, bpp, left, value->widget.x + value->widget.w, 0, row, font, value->caption, value->fg);
	mp_pui_paint_text(buffer, bufW, bpp, right, value->widget.x + value->widget.w, 0, row, font, value->value, value->fg);
	if(row == font->lineHeight / 2) {
		for(dot = left + capW + 2; dot < right - 2; dot += 2)
			mp_pui_put(buffer, bufW, bpp, dot, 0, value->fg);
	}
}

static void _mp_pui_bar_draw(mp_pui_bar_t *bar, unsigned char *buffer, int bufW, int bpp, int local) {
	int width = bar->widget.w;
	int height = bar->widget.h;
	int max = bar->max > 0 ? bar->max : 1;
	int value = bar->value;
	int fill, column, edge;

	if(value < 0)
		value = 0;
	if(value > max)
		value = max;
	fill = width > 2 ? (width - 2) * value / max : 0;
	edge = (local == 0 || local == height - 1);
	for(column = 0; column < width; column++) {
		int on = edge || column == 0 || column == width - 1 || column - 1 < fill;

		mp_pui_put(buffer, bufW, bpp, bar->widget.x + column, 0, on ? bar->fg : bar->bg);
	}
}

static void _mp_pui_invert(unsigned char *buffer, int bufW, int bpp, int x, int w) {
	int column;

	for(column = 0; column < w; column++) {
		int px = x + column;
		mp_pui_color_t color;

		if(px < 0 || px >= bufW)
			continue;
		color = mp_pui_get(buffer, bufW, bpp, px, 0);
		mp_pui_put(buffer, bufW, bpp, px, 0, mp_pui_color_lit(color) ? MP_PUI_COLOR_OFF : MP_PUI_COLOR_ON);
	}
}

static void _mp_pui_list_draw(mp_pui_list_t *list, unsigned char *buffer, int bufW, int bpp, int local) {
	int line = _mp_pui_line();
	int row = local / line;
	int sub = local % line;

	if(row < 0 || row >= list->count)
		return;
	mp_pui_paint_text(buffer, bufW, bpp, list->widget.x + 2, list->widget.x + list->widget.w, 0, sub,
		&mp_pui_font_default, list->text[row], MP_PUI_COLOR_ON);
	if(row == list->focus)
		_mp_pui_invert(buffer, bufW, bpp, list->widget.x, list->widget.w);
}

static void _mp_pui_icon_draw(mp_pui_icon_t *icon, unsigned char *buffer, int bufW, int bpp, int local) {
	int zoom = icon->zoom > 0 ? icon->zoom : 1;
	int stride, column, row, dot;

	row = local / zoom;
	if(!icon->bits || local < 0 || row >= icon->bh || icon->bw <= 0)
		return;
	stride = (icon->bw + 7) / 8;
	for(column = 0; column < icon->bw; column++) {
		int on = icon->bits[row * stride + (column >> 3)] & (0x80 >> (column & 7));

		if(!on)
			continue;
		for(dot = 0; dot < zoom; dot++)
			mp_pui_put(buffer, bufW, bpp, icon->widget.x + column * zoom + dot, 0, icon->fg);
	}
}

static void _mp_pui_badge_draw(mp_pui_badge_t *badge, unsigned char *buffer, int bufW, int bpp, int local) {
	const mp_pui_font_t *font = &mp_pui_font_default;
	int row = _mp_pui_line_row(&badge->widget, font, local);
	int column;

	for(column = 0; column < badge->widget.w; column++)
		mp_pui_put(buffer, bufW, bpp, badge->widget.x + column, 0, badge->fg);
	if(row < 0)
		return;
	mp_pui_paint_text(buffer, bufW, bpp,
		_mp_pui_text_x(&badge->widget, font, badge->text, MP_PUI_ALIGN_CENTER),
		badge->widget.x + badge->widget.w, 0, row, font, badge->text, badge->bg);
}

void mp_pui_widget_draw(mp_pui_widget_t *widget, unsigned char *buffer, int bufW, int bpp, int local) {
	if(!widget || local < 0 || local >= widget->h)
		return;
	switch(widget->kind) {
		case MP_PUI_KIND_LABEL:
			_mp_pui_label_draw((mp_pui_label_t *)widget, buffer, bufW, bpp, local);
			break;
		case MP_PUI_KIND_VALUE:
			_mp_pui_value_draw((mp_pui_value_t *)widget, buffer, bufW, bpp, local);
			break;
		case MP_PUI_KIND_BAR:
			_mp_pui_bar_draw((mp_pui_bar_t *)widget, buffer, bufW, bpp, local);
			break;
		case MP_PUI_KIND_LIST:
			_mp_pui_list_draw((mp_pui_list_t *)widget, buffer, bufW, bpp, local);
			break;
		case MP_PUI_KIND_ICON:
			_mp_pui_icon_draw((mp_pui_icon_t *)widget, buffer, bufW, bpp, local);
			break;
		case MP_PUI_KIND_BADGE:
			_mp_pui_badge_draw((mp_pui_badge_t *)widget, buffer, bufW, bpp, local);
			break;
		case MP_PUI_KIND_BIG:
			_mp_pui_big_draw((mp_pui_big_t *)widget, buffer, bufW, bpp, local);
			break;
	}
}

int mp_pui_widget_rows(mp_pui_widget_t *widget) {
	if(!widget || widget->kind != MP_PUI_KIND_LIST)
		return(0);
	return(((mp_pui_list_t *)widget)->count);
}

void mp_pui_widget_focus(mp_pui_widget_t *widget, int row) {
	if(!widget || widget->kind != MP_PUI_KIND_LIST)
		return;
	((mp_pui_list_t *)widget)->focus = row;
}

const char *mp_pui_widget_row(mp_pui_widget_t *widget, int row) {
	mp_pui_list_t *list;

	if(!widget || widget->kind != MP_PUI_KIND_LIST)
		return(NULL);
	list = (mp_pui_list_t *)widget;
	if(row < 0 || row >= list->count)
		return(NULL);
	return(list->text[row]);
}

void mp_pui_label(mp_pui_label_t *label, mp_pui_page_t *page, int x, int y, int w, int h, const char *text) {
	memset(label, 0, sizeof(*label));
	label->widget.kind = MP_PUI_KIND_LABEL;
	label->widget.x = x;
	label->widget.y = y;
	label->widget.w = w;
	label->widget.h = h;
	label->align = MP_PUI_ALIGN_LEFT;
	label->fg = MP_PUI_COLOR_ON;
	_mp_pui_copy(label->text, MP_PUI_TEXT, text);
	mp_pui_page_add(page, &label->widget);
}

void mp_pui_label_set(mp_pui_label_t *label, const char *text) {
	char next[MP_PUI_TEXT];

	_mp_pui_copy(next, MP_PUI_TEXT, text);
	if(strcmp(label->text, next) == 0)
		return;
	_mp_pui_copy(label->text, MP_PUI_TEXT, next);
	_mp_pui_changed(&label->widget);
}

void mp_pui_label_align(mp_pui_label_t *label, int align) {
	if(label->align == align)
		return;
	label->align = align;
	_mp_pui_changed(&label->widget);
}

void mp_pui_value(mp_pui_value_t *value, mp_pui_page_t *page, int x, int y, int w, int h, const char *caption, const char *text) {
	memset(value, 0, sizeof(*value));
	value->widget.kind = MP_PUI_KIND_VALUE;
	value->widget.x = x;
	value->widget.y = y;
	value->widget.w = w;
	value->widget.h = h;
	value->fg = MP_PUI_COLOR_ON;
	_mp_pui_copy(value->caption, MP_PUI_TEXT, caption);
	_mp_pui_copy(value->value, MP_PUI_TEXT, text);
	mp_pui_page_add(page, &value->widget);
}

void mp_pui_value_set(mp_pui_value_t *value, const char *text) {
	char next[MP_PUI_TEXT];

	_mp_pui_copy(next, MP_PUI_TEXT, text);
	if(strcmp(value->value, next) == 0)
		return;
	_mp_pui_copy(value->value, MP_PUI_TEXT, next);
	_mp_pui_changed(&value->widget);
}

void mp_pui_bar(mp_pui_bar_t *bar, mp_pui_page_t *page, int x, int y, int w, int h, int max) {
	memset(bar, 0, sizeof(*bar));
	bar->widget.kind = MP_PUI_KIND_BAR;
	bar->widget.x = x;
	bar->widget.y = y;
	bar->widget.w = w;
	bar->widget.h = h;
	bar->max = max > 0 ? max : 1;
	bar->fg = MP_PUI_COLOR_ON;
	bar->bg = MP_PUI_COLOR_OFF;
	mp_pui_page_add(page, &bar->widget);
}

void mp_pui_bar_set(mp_pui_bar_t *bar, int value) {
	if(value < 0)
		value = 0;
	if(value > bar->max)
		value = bar->max;
	if(bar->value == value)
		return;
	bar->value = value;
	_mp_pui_changed(&bar->widget);
}

void mp_pui_list(mp_pui_list_t *list, mp_pui_page_t *page, int x, int y, int w, int h) {
	memset(list, 0, sizeof(*list));
	list->widget.kind = MP_PUI_KIND_LIST;
	list->widget.x = x;
	list->widget.y = y;
	list->widget.w = w;
	list->widget.h = h > 0 ? h : _mp_pui_line();
	list->focus = -1;
	mp_pui_page_add(page, &list->widget);
}

void mp_pui_list_add(mp_pui_list_t *list, const char *text) {
	if(list->count >= MP_PUI_LIST_MAX)
		return;
	_mp_pui_copy(list->text[list->count], MP_PUI_TEXT, text);
	list->count++;
	list->widget.h = list->count * _mp_pui_line();
	_mp_pui_changed(&list->widget);
}

void mp_pui_icon(mp_pui_icon_t *icon, mp_pui_page_t *page, int x, int y, int w, int h) {
	memset(icon, 0, sizeof(*icon));
	icon->widget.kind = MP_PUI_KIND_ICON;
	icon->widget.x = x;
	icon->widget.y = y;
	icon->widget.w = w;
	icon->widget.h = h;
	icon->zoom = 1;
	icon->fg = MP_PUI_COLOR_ON;
	mp_pui_page_add(page, &icon->widget);
}

static void _mp_pui_icon_fit(mp_pui_icon_t *icon) {
	if(icon->bw * icon->zoom > icon->widget.w)
		icon->widget.w = icon->bw * icon->zoom;
	if(icon->bh * icon->zoom > icon->widget.h)
		icon->widget.h = icon->bh * icon->zoom;
}

void mp_pui_icon_set(mp_pui_icon_t *icon, const unsigned char *bits, int bw, int bh) {
	icon->bits = bits;
	icon->bw = bw;
	icon->bh = bh;
	_mp_pui_icon_fit(icon);
	_mp_pui_changed(&icon->widget);
}

void mp_pui_icon_zoom(mp_pui_icon_t *icon, int zoom) {
	if(zoom < 1)
		zoom = 1;
	if(icon->zoom == zoom)
		return;
	icon->zoom = zoom;
	_mp_pui_icon_fit(icon);
	_mp_pui_changed(&icon->widget);
}

void mp_pui_badge(mp_pui_badge_t *badge, mp_pui_page_t *page, int x, int y, int w, int h, const char *text) {
	memset(badge, 0, sizeof(*badge));
	badge->widget.kind = MP_PUI_KIND_BADGE;
	badge->widget.x = x;
	badge->widget.y = y;
	badge->widget.w = w;
	badge->widget.h = h;
	badge->fg = MP_PUI_COLOR_ON;
	badge->bg = MP_PUI_COLOR_OFF;
	_mp_pui_copy(badge->text, MP_PUI_TEXT, text);
	mp_pui_page_add(page, &badge->widget);
}

void mp_pui_badge_set(mp_pui_badge_t *badge, const char *text) {
	char next[MP_PUI_TEXT];

	_mp_pui_copy(next, MP_PUI_TEXT, text);
	if(strcmp(badge->text, next) == 0)
		return;
	_mp_pui_copy(badge->text, MP_PUI_TEXT, next);
	_mp_pui_changed(&badge->widget);
}

void mp_pui_big(mp_pui_big_t *big, mp_pui_page_t *page, int x, int y, int w, int h, const char *text) {
	memset(big, 0, sizeof(*big));
	big->widget.kind = MP_PUI_KIND_BIG;
	big->widget.x = x;
	big->widget.y = y;
	big->widget.w = w;
	big->widget.h = h;
	big->fg = MP_PUI_COLOR_ON;
	_mp_pui_copy(big->text, MP_PUI_TEXT, text);
	mp_pui_page_add(page, &big->widget);
}

void mp_pui_big_font(mp_pui_big_t *big, const mp_pui_font_t *font) {
	if(big->font == font)
		return;
	big->font = font;
	_mp_pui_changed(&big->widget);
}

void mp_pui_big_set(mp_pui_big_t *big, const char *text) {
	char next[MP_PUI_TEXT];

	_mp_pui_copy(next, MP_PUI_TEXT, text);
	if(strcmp(big->text, next) == 0)
		return;
	_mp_pui_copy(big->text, MP_PUI_TEXT, next);
	_mp_pui_changed(&big->widget);
}

void mp_pui_label_color(mp_pui_label_t *label, mp_pui_color_t fg) {
	if(label->fg == fg)
		return;
	label->fg = fg;
	_mp_pui_changed(&label->widget);
}

void mp_pui_value_color(mp_pui_value_t *value, mp_pui_color_t fg) {
	if(value->fg == fg)
		return;
	value->fg = fg;
	_mp_pui_changed(&value->widget);
}

void mp_pui_bar_color(mp_pui_bar_t *bar, mp_pui_color_t fg, mp_pui_color_t bg) {
	if(bar->fg == fg && bar->bg == bg)
		return;
	bar->fg = fg;
	bar->bg = bg;
	_mp_pui_changed(&bar->widget);
}

void mp_pui_icon_color(mp_pui_icon_t *icon, mp_pui_color_t fg) {
	if(icon->fg == fg)
		return;
	icon->fg = fg;
	_mp_pui_changed(&icon->widget);
}

void mp_pui_badge_color(mp_pui_badge_t *badge, mp_pui_color_t fg, mp_pui_color_t bg) {
	if(badge->fg == fg && badge->bg == bg)
		return;
	badge->fg = fg;
	badge->bg = bg;
	_mp_pui_changed(&badge->widget);
}

void mp_pui_big_color(mp_pui_big_t *big, mp_pui_color_t fg) {
	if(big->fg == fg)
		return;
	big->fg = fg;
	_mp_pui_changed(&big->widget);
}

#endif
