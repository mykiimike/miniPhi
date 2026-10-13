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
@defgroup mpCommonTUITable Table

@ingroup mpCommonTUIWidgets

@{
*/

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

/**@}*/

#endif
