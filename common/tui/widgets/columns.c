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
@defgroup mpCommonTUIColumns Columns

@ingroup mpCommonTUIWidgets

@{
*/

static const mp_tui_class_t _mp_tui_column_class = { "column", NULL, NULL, NULL };
const mp_tui_class_t mp_tui_columns_class = { "columns", NULL, NULL, NULL };

/* widths side by side, or 0 when some column would get less than its minWidth */
static int _mp_tui_columns_widths(mp_tui_columns_t *columns, int width, int *w) {
	const mp_tui_column_spec_t *spec = columns->specs;
	int count = columns->count;
	int gaps = columns->gap * (count - 1);
	int need = 0, sum = 0, flex = 0, left, share, cut, first = 1, i;

	for(i = 0; i < count; i++) {
		int min = spec[i].minWidth > 0 ? spec[i].minWidth : 0;

		w[i] = spec[i].width > min ? spec[i].width : min;
		if(spec[i].width <= 0)
			flex++;
		need += min;
		sum += w[i];
	}
	if(need + gaps > width)
		return(0);

	left = width - gaps - sum;
	/* fixed columns give back down to their minimum */
	for(i = 0; i < count && left < 0; i++) {
		int min = spec[i].minWidth > 0 ? spec[i].minWidth : 0;

		cut = w[i] - min;
		if(cut > -left)
			cut = -left;
		w[i] -= cut;
		left += cut;
	}
	if(left > 0 && flex) {
		share = left / flex;
		for(i = 0; i < count; i++) {
			if(spec[i].width > 0)
				continue;
			w[i] += share + (first ? left - share * flex : 0);
			first = 0;
		}
	}
	return(1);
}

void mp_tui_columns(mp_tui_columns_t *columns, mp_tui_widget_t *parent, int x, int y, int w, const mp_tui_column_spec_t *specs, int count, int gap) {
	int widths[MP_TUI_COLUMNS_MAX];
	int at = 0, i;

	if(!specs || count < 0)
		count = 0;
	if(count > MP_TUI_COLUMNS_MAX)
		count = MP_TUI_COLUMNS_MAX;
	columns->specs = specs;
	columns->count = count;
	columns->gap = gap > 0 ? gap : 0;
	mp_tui_widget_init(&columns->widget, &mp_tui_columns_class, parent, x, y, w, 0);

	columns->stacked = count > 0 && !_mp_tui_columns_widths(columns, w, widths);
	for(i = 0; i < count; i++) {
		int cw = columns->stacked ? w : widths[i];

		mp_tui_widget_init(&columns->column[i], &_mp_tui_column_class, &columns->widget, columns->stacked ? 0 : at, 0, cw, 0);
		at += cw + columns->gap;
	}
}

mp_tui_widget_t *mp_tui_columns_at(mp_tui_columns_t *columns, int index) {
	if(index < 0 || index >= columns->count)
		return(&columns->widget);
	return(&columns->column[index]);
}

int mp_tui_columns_fit(mp_tui_columns_t *columns) {
	mp_tui_widget_t *column, *child;
	int y = 0, h = 0, bottom, i;

	for(i = 0; i < columns->count; i++) {
		column = &columns->column[i];
		bottom = 0;
		for(child = column->first; child; child = child->next) {
			if(!child->hidden && child->rect.y + child->rect.h > bottom)
				bottom = child->rect.y + child->rect.h;
		}
		if(columns->stacked) {
			if(y > 0 && bottom > 0)
				y += columns->gap;
			mp_tui_move(column, 0, y);
			y += bottom;
		}
		else if(bottom > h)
			h = bottom;
		mp_tui_resize(column, column->rect.w, bottom);
	}
	if(columns->stacked)
		h = y;
	mp_tui_resize(&columns->widget, columns->widget.rect.w, h);
	return(h);
}

/**@}*/

#endif
