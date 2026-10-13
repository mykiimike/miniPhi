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

/**
@defgroup mpCommonTUIWidgets TUI things

@ingroup mpCommonTUI

@{
*/

static const mp_tui_font_t *_mp_tui_font(mp_tui_widget_t *widget, const mp_tui_font_t *font) {
	return(font ? font : widget->tui->theme.font);
}

/* box */

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

/* label */

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

static void _mp_tui_label_vset(mp_tui_label_t *label, const char *format, va_list args) {
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

/* bar */

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

/* table */

#define _MP_TUI_TABLE_COLUMNS 8

static int _mp_tui_table_row_height(mp_tui_table_t *table) {
	return(table->widget.tui->theme.font->lineHeight + 6);
}

static int _mp_tui_table_height(mp_tui_table_t *table) {
	return(_mp_tui_table_row_height(table) * (table->rows + 1));
}

/* x and w are relative to the widget; a width of 0 shares the leftover */
static void _mp_tui_table_columns(mp_tui_table_t *table, int width, int *x, int *w) {
	int count = table->columnCount;
	int fixed = 0, flex = 0, left, share, used, i;

	if(count > _MP_TUI_TABLE_COLUMNS)
		count = _MP_TUI_TABLE_COLUMNS;
	for(i = 0; i < count; i++) {
		if(table->columns[i].width > 0)
			fixed += table->columns[i].width;
		else
			flex++;
	}
	left = width - fixed;
	if(left < 0)
		left = 0;
	share = flex ? left / flex : 0;
	left -= share * flex;
	used = 0;
	for(i = 0; i < count; i++) {
		x[i] = used;
		if(table->columns[i].width > 0)
			w[i] = table->columns[i].width;
		else {
			w[i] = share + left;
			left = 0;
		}
		if(w[i] < 0)
			w[i] = 0;
		used += w[i];
	}
}

static void _mp_tui_table_text(mp_tui_draw_t *draw, const mp_tui_font_t *font, int x, int y, int w, int h, mp_tui_align_t align, const char *text, mp_tui_color_t color) {
	mp_tui_rect_t saved = draw->clip;
	int x0 = x > saved.x ? x : saved.x;
	int y0 = y > saved.y ? y : saved.y;
	int x1 = x + w < saved.x + saved.w ? x + w : saved.x + saved.w;
	int y1 = y + h < saved.y + saved.h ? y + h : saved.y + saved.h;
	int pad = 6;
	int width, tx, ty;

	if(x1 <= x0 || y1 <= y0 || !text)
		return;
	draw->clip.x = x0;
	draw->clip.y = y0;
	draw->clip.w = x1 - x0;
	draw->clip.h = y1 - y0;
	width = mp_tui_text_width(font, text);
	ty = y + (h - font->lineHeight) / 2;
	if(align == MP_TUI_ALIGN_RIGHT)
		tx = x + w - pad - width;
	else if(align == MP_TUI_ALIGN_CENTER)
		tx = x + (w - width) / 2;
	else
		tx = x + pad;
	mp_tui_draw_text(draw, font, tx, ty, text, color);
	draw->clip = saved;
}

static void _mp_tui_table_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_table_t *table = (mp_tui_table_t *)widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	mp_tui_rect_t *r = &widget->screen;
	const mp_tui_font_t *font = theme->font;
	int rowHeight = _mp_tui_table_row_height(table);
	int count = table->columnCount;
	int x[8], w[8];
	int row, column, top;
	mp_tui_color_t background;
	const char *text;

	if(count > _MP_TUI_TABLE_COLUMNS)
		count = _MP_TUI_TABLE_COLUMNS;
	if(count < 1 || !table->columns || rowHeight < 1)
		return;

	_mp_tui_table_columns(table, r->w, x, w);
	mp_tui_draw_frame(draw, r->x, r->y, r->w, r->h, theme->border);

	for(row = 0; row <= table->rows; row++) {
		top = r->y + row * rowHeight;
		if(top + rowHeight <= draw->clip.y || top >= draw->clip.y + draw->clip.h)
			continue;
		if(row == 0)
			background = theme->titleBackground;
		else if(row & 1)
			background = theme->background;
		else
			background = theme->surface;
		{
			int fy = top;
			int fh = rowHeight;

			if(row == 0) {
				fy = top + 1;
				fh = rowHeight - 2;
			}
			else if(row == table->rows)
				fh = rowHeight - 1;
			mp_tui_draw_fill(draw, r->x + 1, fy, r->w - 2, fh, background);
		}
		for(column = 0; column < count; column++) {
			if(column)
				mp_tui_draw_vline(draw, r->x + x[column], top, row == table->rows ? rowHeight - 1 : rowHeight, theme->border);
			text = row == 0 ? table->columns[column].title : NULL;
			if(row > 0 && table->cells && row - 1 < table->capacity)
				text = table->cells[(row - 1) * table->columnCount + column].text;
			_mp_tui_table_text(draw, font, r->x + x[column], top, w[column], rowHeight,
				row == 0 ? MP_TUI_ALIGN_LEFT : table->columns[column].align,
				text ? text : "",
				row == 0 ? theme->titleForeground : theme->foreground);
		}
		if(row == 0)
			mp_tui_draw_hline(draw, r->x, top + rowHeight - 1, r->w, theme->border);
	}
}

const mp_tui_class_t mp_tui_table_class = { "table", _mp_tui_table_draw, NULL };

void mp_tui_table(mp_tui_table_t *table, mp_tui_widget_t *parent, int x, int y, int w, const mp_tui_column_t *columns, int columnCount, mp_tui_cell_t *cells, int capacity) {
	int n;

	table->columns = columns;
	table->cells = cells;
	table->columnCount = columnCount > 0 ? columnCount : 0;
	table->capacity = capacity > 0 ? capacity : 0;
	table->rows = 0;
	if(cells && table->columnCount && table->capacity) {
		for(n = 0; n < table->capacity * table->columnCount; n++)
			cells[n].text[0] = '\0';
	}
	mp_tui_widget_init(&table->widget, &mp_tui_table_class, parent, x, y, w,
		(parent->tui->theme.font->lineHeight + 6) * (table->rows + 1));
}

void mp_tui_table_rows(mp_tui_table_t *table, int rows) {
	if(rows < 0)
		rows = 0;
	if(rows > table->capacity)
		rows = table->capacity;
	if(rows == table->rows)
		return;
	table->rows = rows;
	mp_tui_resize(&table->widget, table->widget.rect.w, _mp_tui_table_height(table));
}

void mp_tui_table_set(mp_tui_table_t *table, int row, int column, const char *format, ...) {
	mp_tui_cell_t *cell;
	char text[MP_TUI_TABLE_CELL];
	va_list args;

	if(!table->cells || !format || row < 0 || column < 0 || row >= table->rows || column >= table->columnCount)
		return;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	cell = &table->cells[row * table->columnCount + column];
	if(strcmp(text, cell->text) == 0)
		return;
	memcpy(cell->text, text, sizeof(text));
	mp_tui_invalidate(&table->widget);
}

/* window */

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

/* button */

static void _mp_tui_button_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_button_t *button = (mp_tui_button_t *)widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	const mp_tui_font_t *font = theme->font;
	mp_tui_rect_t *r = &widget->screen;
	mp_tui_color_t color = button->selected ? theme->accent : theme->foreground;
	mp_tui_color_t border = button->selected ? theme->accent : theme->border;
	int y = r->y + (r->h - font->lineHeight) / 2;

	if(button->plain) {
		mp_tui_draw_text(draw, font, r->x, y, button->text, button->pressed ? theme->accent : theme->titleForeground);
		return;
	}

	mp_tui_draw_fill(draw, r->x, r->y, r->w, r->h, button->pressed ? theme->titleBackground : theme->surface);
	mp_tui_draw_frame(draw, r->x, r->y, r->w, r->h, border);
	mp_tui_draw_text(draw, font, r->x + 10, y, button->text, color);
	if(button->chevron)
		mp_tui_draw_text(draw, font, r->x + r->w - 18, y, ">", button->selected ? theme->accent : theme->muted);
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
	snprintf(button->text, sizeof(button->text), "%s", text ? text : "");
	mp_tui_widget_init(&button->widget, &mp_tui_button_class, parent, x, y, w, h);
}

/* view */

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

/* page */

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

/* by name */

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

	if(!widget || widget->type != &mp_tui_bar_class)
		return(0);
	mp_tui_bar_set((mp_tui_bar_t *)widget, value);
	return(1);
}

/**@}*/

#endif
