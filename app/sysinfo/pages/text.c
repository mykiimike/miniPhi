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

#include "index.h"

#ifdef SYSINFO_SCREEN

#include <string.h>

static int _sysinfo_fits(const mp_tui_font_t *font, const char *text, int count, int width) {
	char probe[MP_TUI_TEXT_MAX];

	if(count <= 0)
		return(1);
	if(count >= MP_TUI_TEXT_MAX)
		count = MP_TUI_TEXT_MAX - 1;
	memcpy(probe, text, (size_t)count);
	probe[count] = '\0';
	return(mp_tui_text_width(font, probe) <= width);
}

/* a line that does not fit the panel continues underneath, so nothing is cut off */
static int _sysinfo_wrap(char *text, const mp_tui_font_t *font, int width) {
	char src[MP_TUI_TEXT_MAX];
	int from, lines, out;

	if(!text || !text[0] || width < 1)
		return(1);
	if(_sysinfo_fits(font, text, (int)strlen(text), width))
		return(1);

	snprintf(src, sizeof(src), "%s", text);
	text[0] = '\0';
	from = 0;
	out = 0;
	lines = 0;
	while(src[from] && out < MP_TUI_TEXT_MAX - 1) {
		int at = from;
		int space = -1;
		int end, next, n;

		while(src[at] && src[at] != '\n' && _sysinfo_fits(font, src + from, at - from + 1, width)) {
			if(src[at] == ' ')
				space = at;
			at++;
		}
		if(!src[at] || src[at] == '\n') {
			n = at - from;
			if(out + n >= MP_TUI_TEXT_MAX)
				n = MP_TUI_TEXT_MAX - out - 1;
			memcpy(text + out, src + from, (size_t)n);
			out += n;
			text[out] = '\0';
			lines++;
			if(src[at] != '\n')
				break;
			if(out < MP_TUI_TEXT_MAX - 1) {
				text[out++] = '\n';
				text[out] = '\0';
			}
			from = at + 1;
			continue;
		}
		if(space > from) {
			end = space;
			next = space + 1;
		}
		else if(at > from) {
			end = at;
			next = at;
		}
		else {
			end = from + 1;
			next = from + 1;
		}
		n = end - from;
		if(out + n + 1 >= MP_TUI_TEXT_MAX)
			break;
		memcpy(text + out, src + from, (size_t)n);
		out += n;
		text[out++] = '\n';
		text[out] = '\0';
		lines++;
		from = next;
	}
	return(lines > 0 ? lines : 1);
}

void _sysinfo_text(mp_tui_label_t *label, mp_tui_widget_t *parent, int x, int *y, int w, int line, const mp_tui_font_t *font, const char *text) {
	char wrapped[MP_TUI_TEXT_MAX];
	int rows;

	snprintf(wrapped, sizeof(wrapped), "%s", text);
	rows = _sysinfo_wrap(wrapped, font, w);
	mp_tui_label(label, parent, x, *y, w, rows * line, wrapped);
	*y += rows * line;
}

#endif
