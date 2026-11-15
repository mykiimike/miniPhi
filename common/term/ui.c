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

/**
 * @defgroup mpCommonTerm Terminal codec
 * @ingroup mpCommon
 *
 * The shell. One line of menus, dropdowns over the page, a status line.
 * The page is the full width. A menu is a layer on the back buffer: the
 * diff sends only the cells that changed, and closing it redraws what was
 * underneath. A one-line body shift is insert or delete line inside the
 * body region. Minicom applies those and ignores CSI S / CSI T, which is
 * how Messages keeps up with printk.
 * @{
 */

#include <mp.h>

#ifdef SUPPORT_COMMON_TERM

#include <stdio.h>
#include <string.h>
#include <term/term.h>
#include "pages/pages.h"

static unsigned short _ui_cell(int ch, int fg, int bg, int flags) {
	unsigned short cell = (unsigned short)(ch & 0x7f);

	cell |= (unsigned short)((fg & 7) << 8);
	cell |= (unsigned short)((bg & 7) << 11);
	if(flags & MP_TERM_UI_DEC)
		cell |= 0x80;
	if(flags & MP_TERM_UI_BOLD)
		cell |= 0x4000;
	if(flags & MP_TERM_UI_UNDER)
		cell |= 0x8000;
	return(cell);
}

static int _ui_ch(unsigned short cell) { return(cell & 0x7f); }
static int _ui_fg(unsigned short cell) { return((cell >> 8) & 7); }
static int _ui_bg(unsigned short cell) { return((cell >> 11) & 7); }

static int _ui_fl(unsigned short cell) {
	int flags = 0;

	if(cell & 0x80)
		flags |= MP_TERM_UI_DEC;
	if(cell & 0x4000)
		flags |= MP_TERM_UI_BOLD;
	if(cell & 0x8000)
		flags |= MP_TERM_UI_UNDER;
	return(flags);
}

static unsigned long _ui_ms(unsigned long ms) {
	unsigned long ticks = (mp_clock_tick_hz() * ms) / 1000UL;

	return(ticks ? ticks : 1);
}

void _ui_copy(char *dst, int cap, const char *src) {
	int i = 0;

	if(!src)
		src = "";
	if(cap < 1)
		return;
	while(src[i] && i + 1 < cap) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
}

static int _ui_lower(int c) {
	if(c >= 'A' && c <= 'Z')
		return(c - 'A' + 'a');
	return(c);
}

static int _ui_count(const mp_term_ui_t *ui, int parent) {
	int c = parent < 0 ? ui->root : ui->node[parent].child;
	int n = 0;

	while(c >= 0) {
		n++;
		c = ui->node[c].next;
	}
	return(n);
}

/* the root panel ends with Exit: it is not a page, it returns to the splash */
static int _ui_menu_count(const mp_term_ui_t *ui, int parent) {
	return(_ui_count(ui, parent) + (parent < 0 ? 1 : 0));
}

static int _ui_nth(const mp_term_ui_t *ui, int parent, int index) {
	int c = parent < 0 ? ui->root : ui->node[parent].child;

	while(c >= 0 && index > 0) {
		c = ui->node[c].next;
		index--;
	}
	return(c);
}


static int _ui_alloc(mp_term_ui_t *ui) {
	int i;

	for(i = 0; i < MP_TERM_UI_NODES; i++) {
		if(ui->node[i].used == YES)
			continue;
		memset(&ui->node[i], 0, sizeof(ui->node[i]));
		ui->node[i].used = YES;
		ui->node[i].parent = -1;
		ui->node[i].child = -1;
		ui->node[i].next = -1;
		return(i);
	}
	return(-1);
}

static void _ui_link(mp_term_ui_t *ui, int parent, int node) {
	int *head;
	int c;

	ui->node[node].parent = parent;
	ui->node[node].next = -1;
	head = parent < 0 ? &ui->root : &ui->node[parent].child;
	if(*head < 0) {
		*head = node;
		return;
	}
	c = *head;
	while(ui->node[c].next >= 0)
		c = ui->node[c].next;
	ui->node[c].next = node;
}

static void _ui_unlink(mp_term_ui_t *ui, int node) {
	int parent = ui->node[node].parent;
	int *head = parent < 0 ? &ui->root : &ui->node[parent].child;
	int prev = -1;
	int c = *head;

	while(c >= 0 && c != node) {
		prev = c;
		c = ui->node[c].next;
	}
	if(c != node)
		return;
	if(prev < 0)
		*head = ui->node[node].next;
	else
		ui->node[prev].next = ui->node[node].next;
	ui->node[node].next = -1;
	ui->node[node].parent = -1;
}

static void _ui_free_tree(mp_term_ui_t *ui, int node) {
	int c, n;

	if(node < 0)
		return;
	c = ui->node[node].child;
	ui->node[node].child = -1;
	while(c >= 0) {
		n = ui->node[c].next;
		_ui_free_tree(ui, c);
		c = n;
	}
	if(ui->open == node)
		ui->open = -1;
	if(ui->conNode == node)
		ui->conNode = -1;
	if(ui->logNode == node)
		ui->logNode = -1;
	ui->node[node].used = NO;
	ui->node[node].page = NULL;
	ui->node[node].next = -1;
}

static void _ui_prune(mp_term_ui_t *ui, int parent) {
	while(parent >= 0 && ui->node[parent].page == NULL &&
			ui->node[parent].child < 0 && ui->node[parent].builtin != YES) {
		int above = ui->node[parent].parent;

		_ui_unlink(ui, parent);
		_ui_free_tree(ui, parent);
		parent = above;
	}
}

static int _ui_find_child(const mp_term_ui_t *ui, int parent, const char *name) {
	int c = parent < 0 ? ui->root : ui->node[parent].child;

	while(c >= 0) {
		if(strcmp(ui->node[c].name, name) == 0)
			return(c);
		c = ui->node[c].next;
	}
	return(-1);
}

static int _ui_component(const char *path, int *at, char *out, int cap) {
	int i = *at;
	int n = 0;

	while(path[i] == '/')
		i++;
	if(path[i] == 0) {
		*at = i;
		return(0);
	}
	while(path[i] && path[i] != '/') {
		if(n + 1 < cap)
			out[n++] = path[i];
		i++;
	}
	out[n] = 0;
	*at = i;
	return(1);
}

static int _ui_find_path(const mp_term_ui_t *ui, const char *path) {
	char part[16];
	int at = 0;
	int parent = -1;
	int node = -1;

	while(_ui_component(path, &at, part, (int)sizeof(part))) {
		node = _ui_find_child(ui, parent, part);
		if(node < 0)
			return(-1);
		parent = node;
	}
	return(node);
}

static void _ui_raw(mp_term_ui_t *ui, int row, int col, const char *s, int n,
		int fg, int bg, int flags) {
	int i;

	if(row < 0 || row >= ui->rows)
		return;
	for(i = 0; i < n && s[i]; i++) {
		if(col + i < 0 || col + i >= ui->cols)
			continue;
		ui->page[row][col + i] = _ui_cell((unsigned char)s[i], fg, bg, flags);
	}
}

static void _ui_text(mp_term_ui_t *ui, int row, int col, const char *s,
		int fg, int bg, int flags, int accelAt) {
	int i;

	if(!s || row < 0 || row >= ui->rows)
		return;
	for(i = 0; s[i] && col + i < ui->cols; i++) {
		int f = flags;
		int c = col + i;

		if(c < 0)
			continue;
		if(i == accelAt)
			f |= MP_TERM_UI_UNDER;
		ui->page[row][c] = _ui_cell((unsigned char)s[i], fg, bg, f);
	}
}

void mp_term_ui_clear(mp_term_ui_t *ui) {
	int r, c, bot;

	if(!ui)
		return;
	bot = ui->rows - 1;
	for(r = 1; r < bot; r++)
		for(c = 0; c < ui->cols; c++)
			ui->page[r][c] = _ui_cell(' ', 7, 0, 0);
}

/* one visual row: the bytes to draw, and how many source bytes follow it
 * (the breaking space, or the newline). A leading newline is a blank row. */
static int _ui_piece(const char *p, int width, int *skip) {
	int n = 0;

	*skip = 0;
	if(!p || !*p || width < 1)
		return(0);
	if(*p == '\n') {
		*skip = 1;
		return(0);
	}
	while(p[n] && p[n] != '\n' && n < width)
		n++;
	if(p[n] == '\n') {
		*skip = 1;
		return(n);
	}
	if(!p[n])
		return(n);
	{
		int cut = n;

		while(cut > width / 2 && p[cut] != ' ')
			cut--;
		if(p[cut] == ' ') {
			*skip = 1;
			return(cut);
		}
	}
	return(n);
}

static int _ui_span(const char *text, int width) {
	const char *p = text;
	int rows = 0;

	if(!text || !*text || width < 1)
		return(0);
	while(*p) {
		int skip = 0;
		int n = _ui_piece(p, width, &skip);

		rows++;
		p += n + skip;
		if(n == 0 && skip == 0)
			break;
	}
	return(rows);
}

int mp_term_ui_lines(const mp_term_ui_t *ui, int col, const char *text) {
	if(!ui || col >= ui->cols)
		return(0);
	return(_ui_span(text, ui->cols - col));
}

int mp_term_ui_put(mp_term_ui_t *ui, int row, int col, const char *text,
		int fg, int bg, int flags) {
	const char *p = text;
	int width, rows, r;

	if(!ui || !text)
		return(0);
	width = ui->cols - col;
	if(width < 1 || !*text)
		return(0);
	rows = 0;
	r = row + 1;
	while(*p && r > 0 && r < ui->rows - 1) {
		int skip = 0;
		int n = _ui_piece(p, width, &skip);
		int i;

		for(i = 0; i < n; i++) {
			if(col + i < 0 || col + i >= ui->cols)
				continue;
			ui->page[r][col + i] = _ui_cell((unsigned char)p[i], fg, bg, flags);
		}
		rows++;
		r++;
		p += n + skip;
		if(n == 0 && skip == 0)
			break;
	}
	return(rows);
}

void mp_term_ui_fill(mp_term_ui_t *ui, int row, int col, int n, int ch,
		int fg, int bg, int flags) {
	int r, i;

	if(!ui)
		return;
	r = row + 1;
	if(r <= 0 || r >= ui->rows - 1)
		return;
	for(i = 0; i < n; i++) {
		int c = col + i;

		if(c < 0 || c >= ui->cols)
			continue;
		ui->page[r][c] = _ui_cell(ch, fg, bg, flags);
	}
}

void mp_term_ui_hint(mp_term_ui_t *ui, const char *text) {
	if(!ui)
		return;
	_ui_copy(ui->hint, (int)sizeof(ui->hint), text ? text : "");
}

void mp_term_ui_cursor_at(mp_term_ui_t *ui, int row, int col) {
	int r;

	if(!ui)
		return;
	if(row < 0 || col < 0) {
		ui->curRow = -1;
		ui->curCol = -1;
		return;
	}
	r = row + 1;
	if(r >= ui->rows - 1)
		r = ui->rows - 2;
	if(r < 1)
		r = 1;
	if(col >= ui->cols)
		col = ui->cols - 1;
	if(col < 0)
		col = 0;
	ui->curRow = r;
	ui->curCol = col;
}

int mp_term_ui_rows(const mp_term_ui_t *ui) {
	int n;

	if(!ui)
		return(0);
	n = ui->rows - 2;
	return(n > 0 ? n : 0);
}

int mp_term_ui_cols(const mp_term_ui_t *ui) {
	if(!ui || ui->cols < 0)
		return(0);
	return(ui->cols);
}

static char _ui_accel_of(const char *name) {
	int i;

	for(i = 0; name[i]; i++) {
		int c = _ui_lower((unsigned char)name[i]);

		if(c >= 'a' && c <= 'z')
			return((char)c);
		if(c >= '0' && c <= '9')
			return((char)c);
	}
	return(0);
}

static mp_ret_t _ui_add(mp_term_ui_t *ui, const char *path, const mp_term_page_t *page, int builtin) {
	char part[16];
	int at, depth, parts, parent, node;
	mp_task_lock_t key;

	if(!ui || !path || !path[0] || !page)
		return(FALSE);
	parts = 0;
	at = 0;
	while(path[at]) {
		while(path[at] == '/')
			at++;
		if(!path[at])
			break;
		parts++;
		while(path[at] && path[at] != '/')
			at++;
	}
	if(parts < 1 || parts > MP_TERM_UI_DEPTH)
		return(FALSE);

	key = mp_task_lock(MP_TASK_CEILING_KERNEL);
	at = 0;
	depth = 0;
	parent = -1;
	node = -1;
	while(_ui_component(path, &at, part, (int)sizeof(part))) {
		depth++;
		node = _ui_find_child(ui, parent, part);
		if(node < 0) {
			node = _ui_alloc(ui);
			if(node < 0) {
				mp_task_unlock(key);
				return(FALSE);
			}
			_ui_copy(ui->node[node].name, (int)sizeof(ui->node[node].name), part);
			ui->node[node].accel = _ui_accel_of(part);
			_ui_link(ui, parent, node);
		}
		if(depth == parts) {
			ui->node[node].page = page;
			if(builtin)
				ui->node[node].builtin = YES;
			if(page == &mp_term_page_console)
				ui->conNode = node;
			if(page == &mp_term_page_messages)
				ui->logNode = node;
		}
		parent = node;
	}
	ui->bodyDirty = YES;
	mp_task_unlock(key);
	return(TRUE);
}

static void _ui_clock(char *dst, int n) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long sec = hz ? mp_clock_ticks() / hz : 0;

	snprintf(dst, (size_t)n, "%02lu:%02lu:%02lu",
		(sec / 3600UL) % 100UL, (sec / 60UL) % 60UL, sec % 60UL);
}

static void _ui_crumb(const mp_term_ui_t *ui, int node, char *out, int cap) {
	int chain[MP_TERM_UI_DEPTH];
	int n = 0, i, at = 0;

	out[0] = 0;
	while(node >= 0 && n < MP_TERM_UI_DEPTH) {
		chain[n++] = node;
		node = ui->node[node].parent;
	}
	for(i = n - 1; i >= 0; i--) {
		const char *name = ui->node[chain[i]].name;
		int k;

		if(at > 0 && at + 3 < cap) {
			out[at++] = ' ';
			out[at++] = '>';
			out[at++] = ' ';
		}
		for(k = 0; name[k] && at + 1 < cap; k++)
			out[at++] = name[k];
	}
	out[at] = 0;
}

/* the header is not a menu any more: just the page in use, and the clock */
static void _ui_header(mp_term_ui_t *ui) {
	char clock[16];
	char label[64];
	int c, right, max;

	if(ui->rows < 1 || ui->cols < 1)
		return;
	for(c = 0; c < ui->cols; c++)
		ui->page[0][c] = _ui_cell(' ', 7, 4, 0);
	_ui_clock(clock, (int)sizeof(clock));
	right = ui->cols - (int)strlen(clock) - 1;
	if(right < 0)
		right = ui->cols;
	else
		_ui_text(ui, 0, right, clock, 7, 4, 0, -1);
	label[0] = 0;
	if(ui->open >= 0 && ui->node[ui->open].used == YES)
		_ui_crumb(ui, ui->open, label, (int)sizeof(label));
	if(!label[0])
		_ui_copy(label, (int)sizeof(label), "miniPhi");
	if(ui->cols > 4) {
		ui->page[0][1] = _ui_cell('`', 6, 4, MP_TERM_UI_DEC | MP_TERM_UI_BOLD);
		max = right - 4;
		if(max > 0)
			_ui_raw(ui, 0, 3, label, max, 7, 4, MP_TERM_UI_BOLD);
	}
}

/* "Enter send  Up history": the first word of each pair is a key */
/* "Key what  Key what": each key as a cyan pill, its words after it */
static void _ui_hints(mp_term_ui_t *ui, int row, int limit) {
	const char *h = ui->hint;
	int x = 1;

	while(*h && x < limit) {
		int k = 0, d = 0;

		while(h[k] && h[k] != ' ')
			k++;
		while(h[k + 1 + d] && !(h[k + 1 + d] == ' ' && h[k + 2 + d] == ' '))
			d++;
		if(!h[k])
			d = -1;
		if(x + k + 2 + (d > 0 ? d + 1 : 0) > limit)
			break;
		ui->page[row][x++] = _ui_cell(' ', 0, 6, 0);
		_ui_raw(ui, row, x, h, k, 0, 6, 0);
		x += k;
		ui->page[row][x++] = _ui_cell(' ', 0, 6, 0);
		if(d > 0) {
			x++;
			_ui_raw(ui, row, x, h + k + 1, d, 7, 4, 0);
			x += d;
		}
		h += k + (d >= 0 ? 1 + d : 0);
		while(*h == ' ')
			h++;
		x += 2;
	}
}

static void _ui_status(mp_term_ui_t *ui) {
	char mhz[24];
	const char *level = NULL;
	unsigned long hz;
	int row, c, x;

	row = ui->rows - 1;
	if(row < 1)
		return;
	for(c = 0; c < ui->cols; c++)
		ui->page[row][c] = _ui_cell(' ', 7, 4, 0);
	hz = mp_clock_get_speed();
	snprintf(mhz, sizeof(mhz), "%lu MHz", (hz + 500000UL) / 1000000UL);
#ifdef _HAVE_MP_POWER_H
	if(ui->term && ui->term->kernel)
		level = mp_power_name(mp_power_level(ui->term->kernel));
#endif
	/* right to left: level, clock, board */
	x = ui->cols - 1;
	if(level) {
		x -= (int)strlen(level);
		_ui_text(ui, row, x, level, 3, 4, MP_TERM_UI_BOLD, -1);
		x -= 2;
		if(x >= 0)
			ui->page[row][x] = _ui_cell('x', 6, 4, MP_TERM_UI_DEC);
		x -= 1;
	}
	x -= (int)strlen(mhz);
	_ui_text(ui, row, x, mhz, 7, 4, MP_TERM_UI_BOLD, -1);
	if(ui->place[0]) {
		x -= 2;
		if(x >= 0)
			ui->page[row][x] = _ui_cell('x', 6, 4, MP_TERM_UI_DEC);
		x -= 1 + (int)strlen(ui->place);
		_ui_text(ui, row, x, ui->place, 7, 4, 0, -1);
	}
	if(x < 0)
		x = 0;
	_ui_hints(ui, row, x - 2);
}

/* the menu is one flat list: every page, in tree order */
static void _ui_menu_collect_from(const mp_term_ui_t *ui, int parent,
		int *nodes, int *count) {
	int c;

	for(c = parent < 0 ? ui->root : ui->node[parent].child;
			c >= 0 && *count < MP_TERM_UI_NODES; c = ui->node[c].next) {
		if(ui->node[c].page)
			nodes[(*count)++] = c;
		else if(ui->node[c].child >= 0)
			_ui_menu_collect_from(ui, c, nodes, count);
	}
}

static int _ui_menu_collect(const mp_term_ui_t *ui, int *nodes) {
	int count = 0;

	_ui_menu_collect_from(ui, -1, nodes, &count);
	return(count);
}

static int _ui_first_leaf(const mp_term_ui_t *ui, int node) {
	int c;

	if(node < 0 || ui->node[node].used != YES)
		return(-1);
	if(ui->node[node].page)
		return(node);
	for(c = ui->node[node].child; c >= 0; c = ui->node[c].next) {
		int leaf = _ui_first_leaf(ui, c);

		if(leaf >= 0)
			return(leaf);
	}
	return(-1);
}

/* index of node among the children of parent, 0 when absent */
static int _ui_child_index(const mp_term_ui_t *ui, int parent, int node) {
	int n = _ui_count(ui, parent);
	int i;

	for(i = 0; i < n; i++) {
		if(_ui_nth(ui, parent, i) == node)
			return(i);
	}
	return(0);
}

static int _ui_menu_parent(const mp_term_ui_t *ui) {
	if(ui->menuDepth < 1)
		return(-1);
	return(ui->menuParent[ui->menuDepth - 1]);
}

/* open the panel on a group: the stack remembers how we got there */
static void _ui_open_group(mp_term_ui_t *ui, int node) {
	int chain[MP_TERM_UI_DEPTH];
	int n = 0, i, temp = node;

	while(temp >= 0 && n < MP_TERM_UI_DEPTH) {
		chain[n++] = temp;
		temp = ui->node[temp].parent;
	}
	ui->menuDepth = 1;
	ui->menuParent[0] = -1;
	ui->menuCursor[0] = n > 0 ? _ui_child_index(ui, -1, chain[n - 1]) : 0;
	for(i = n - 1; i >= 0; i--) {
		if(ui->menuDepth >= MP_TERM_UI_DEPTH)
			break;
		ui->menuParent[ui->menuDepth] = chain[i];
		ui->menuCursor[ui->menuDepth] = 0;
		ui->menuDepth++;
	}
	ui->mode = MP_TERM_UI_MODE_MENU;
}

static void _ui_layout(mp_term_ui_t *ui) {
	mp_term_ui_box_t *box = &ui->box[0];
	int parent, count, width, height, cursor, vis, scroll, i;
	char title[64];

	ui->depth = 0;
	if(ui->mode != MP_TERM_UI_MODE_MENU)
		return;
	parent = _ui_menu_parent(ui);
	count = _ui_menu_count(ui, parent);
	if(count < 1)
		return;
	if(parent < 0)
		_ui_copy(title, (int)sizeof(title), "Menu");
	else
		_ui_crumb(ui, parent, title, (int)sizeof(title));
	width = (int)strlen(title) + 6;
	if(width < 28)
		width = 28;
	for(i = 0; i < _ui_count(ui, parent); i++) {
		int child = _ui_nth(ui, parent, i);
		int w = (int)strlen(ui->node[child].name) + 7;

		if(w > width)
			width = w;
	}
	if(width > ui->cols)
		width = ui->cols;
	height = count + 4;
	if(height > ui->rows - 2)
		height = ui->rows - 2;
	if(height < 5)
		height = 5;
	cursor = ui->menuCursor[ui->menuDepth - 1];
	if(cursor < 0)
		cursor = 0;
	if(cursor >= count)
		cursor = count - 1;
	ui->menuCursor[ui->menuDepth - 1] = cursor;
	vis = height - 4;
	if(vis < 1)
		vis = 1;
	scroll = 0;
	if(cursor >= vis)
		scroll = cursor - vis + 1;
	box->node = parent;
	box->cursor = cursor;
	box->scroll = scroll;
	box->top = (ui->rows - height) / 2;
	if(box->top < 1)
		box->top = 1;
	box->left = (ui->cols - width) / 2;
	if(box->left < 0)
		box->left = 0;
	box->width = width;
	box->height = height;
	box->count = count;
	ui->depth = 1;
}

static unsigned short _ui_box_cell(const mp_term_ui_t *ui, const mp_term_ui_box_t *box, int row, int col) {
	int relR = row - box->top;
	int relC = col - box->left;
	char title[64];
	char pill[68];
	int titleAt;

	if(box->node < 0)
		_ui_copy(title, (int)sizeof(title), "Menu");
	else
		_ui_crumb(ui, box->node, title, (int)sizeof(title));
	snprintf(pill, sizeof(pill), " %s ", title);
	titleAt = (box->width - (int)strlen(pill)) / 2;
	if(titleAt < 1)
		titleAt = 1;
	if(relR == 0 && relC >= titleAt && relC < titleAt + (int)strlen(pill))
		return(_ui_cell((unsigned char)pill[relC - titleAt], 0, 6, 0));
	if(relR == 0 || relR == box->height - 1 || relC == 0 || relC == box->width - 1) {
		int ch = 'x';

		if(relR == 0 && relC == 0)
			ch = 'l';
		else if(relR == 0 && relC == box->width - 1)
			ch = 'k';
		else if(relR == box->height - 1 && relC == 0)
			ch = 'm';
		else if(relR == box->height - 1 && relC == box->width - 1)
			ch = 'j';
		else if(relR == 0 || relR == box->height - 1)
			ch = 'q';
		return(_ui_cell(ch, 4, 0, MP_TERM_UI_DEC | MP_TERM_UI_BOLD));
	}
	if(relR == box->height - 2) {
		static const char foot[] = "Enter open   Esc back";
		int at = (box->width - (int)sizeof(foot) + 1) / 2;
		int i = relC - at;

		if(i >= 0 && i < (int)sizeof(foot) - 1)
			return(_ui_cell((unsigned char)foot[i], 4, 0, MP_TERM_UI_BOLD));
		return(_ui_cell(' ', 7, 0, 0));
	}
	if(relR < 2 || relR >= box->height - 2)
		return(_ui_cell(' ', 7, 0, 0));
	{
		int index = box->scroll + relR - 2;
		int count = _ui_menu_count(ui, box->node);
		int selected, child, at;

		if(index < 0 || index >= count)
			return(_ui_cell(' ', 7, 0, 0));
		if(box->node < 0 && index + 1 == count) {
			static const char exitName[] = "Exit";

			selected = index == box->cursor;
			if(relC == 1)
				return(_ui_cell(selected ? '>' : ' ', selected ? 0 : 7,
					selected ? 6 : 0, selected ? MP_TERM_UI_BOLD : 0));
			at = relC - 3;
			if(at < 0 || at >= (int)sizeof(exitName) - 1)
				return(_ui_cell(' ', selected ? 0 : 7, selected ? 6 : 0, 0));
			return(_ui_cell((unsigned char)exitName[at],
				selected ? 0 : 7, selected ? 6 : 0, 0));
		}
		child = _ui_nth(ui, box->node, index);
		selected = index == box->cursor;
		if(ui->node[child].child >= 0 && relC == box->width - 2)
			return(_ui_cell('>', selected ? 0 : 6, selected ? 6 : 0,
				selected ? MP_TERM_UI_BOLD : 0));
		if(ui->node[child].child < 0 && child == ui->open && relC == box->width - 2)
			return(_ui_cell('`', selected ? 0 : 3, selected ? 6 : 0,
				MP_TERM_UI_DEC | MP_TERM_UI_BOLD));
		if(relC == 1)
			return(_ui_cell(selected ? '>' : ' ', selected ? 0 : 7,
				selected ? 6 : 0, selected ? MP_TERM_UI_BOLD : 0));
		at = relC - 3;
		if(at < 0 || at >= (int)strlen(ui->node[child].name))
			return(_ui_cell(' ', selected ? 0 : 7, selected ? 6 : 0, 0));
		return(_ui_cell((unsigned char)ui->node[child].name[at],
			selected ? 0 : 7, selected ? 6 : 0,
			ui->node[child].child >= 0 ? MP_TERM_UI_BOLD : 0));
	}
}

static int _ui_cover(const mp_term_ui_t *ui, int row, int col, unsigned short *cell) {
	int d;

	for(d = ui->depth - 1; d >= 0; d--) {
		const mp_term_ui_box_t *box = &ui->box[d];

		if(row < box->top || row >= box->top + box->height)
			continue;
		if(col < box->left || col >= box->left + box->width)
			continue;
		*cell = _ui_box_cell(ui, box, row, col);
		return(1);
	}
	return(0);
}

static int _ui_first_page(const mp_term_ui_t *ui) {
	int i;

	if(ui->conNode >= 0 && ui->node[ui->conNode].used == YES && ui->node[ui->conNode].page)
		return(ui->conNode);
	for(i = 0; i < MP_TERM_UI_NODES; i++) {
		if(ui->node[i].used == YES && ui->node[i].page)
			return(i);
	}
	return(-1);
}

static void _ui_validate(mp_term_ui_t *ui) {
	int i;

	if(ui->open < 0 || ui->node[ui->open].used != YES || ui->node[ui->open].page == NULL)
		ui->open = _ui_first_page(ui);
	while(ui->menuDepth > 0) {
		int parent = ui->menuParent[ui->menuDepth - 1];

		if(parent < 0 || (ui->node[parent].used == YES && ui->node[parent].child >= 0))
			break;
		ui->menuDepth--;
	}
	if(ui->mode == MP_TERM_UI_MODE_MENU && ui->menuDepth < 1)
		ui->mode = MP_TERM_UI_MODE_PAGE;
	for(i = 0; i < ui->menuDepth; i++) {
		int count = _ui_menu_count(ui, ui->menuParent[i]);

		if(ui->menuCursor[i] < 0)
			ui->menuCursor[i] = 0;
		if(count > 0 && ui->menuCursor[i] >= count)
			ui->menuCursor[i] = count - 1;
	}
}

static void _ui_render(mp_term_ui_t *ui) {
	const mp_term_page_t *page = NULL;
	unsigned long now;

	_ui_validate(ui);
	if(ui->open >= 0 && ui->node[ui->open].used == YES)
		page = ui->node[ui->open].page;
	if(ui->open == ui->logNode && ui->logSeq != ui->paintedLog)
		ui->bodyDirty = YES;
	now = mp_clock_ticks();
	if(page && page->refresh > 0 && ui->bodyDirty != YES) {
		unsigned long need = _ui_ms(page->refresh);

		if(now - ui->paintAt >= need)
			ui->bodyDirty = YES;
	}
	if(ui->bodyDirty == YES) {
		ui->curRow = -1;
		ui->curCol = -1;
		_ui_copy(ui->hint, (int)sizeof(ui->hint), "Esc menu");
		mp_term_ui_clear(ui);
		if(page && page->paint)
			page->paint(ui, page->user);
		ui->paintAt = now;
		ui->bodyDirty = NO;
	}
	if(ui->mode == MP_TERM_UI_MODE_MENU)
		_ui_copy(ui->hint, (int)sizeof(ui->hint), "Up/Down choose  Enter open  Esc close");
	_ui_layout(ui);
	_ui_header(ui);
	_ui_status(ui);
}

static int _ui_row_same(const unsigned short *a, const unsigned short *b, int n) {
	int c;

	for(c = 0; c < n; c++)
		if(a[c] != b[c])
			return(0);
	return(1);
}

/* 1 when the body moved up one row, -1 when it moved down, 0 otherwise.
 * The row that leaves has to change, or a uniform page editing its last
 * line looks like a scroll. */
static int _ui_body_scrolled(const mp_term_ui_t *ui) {
	int top = 1;
	int bot = ui->rows - 2;
	int r, up, down;

	if(ui->mode == MP_TERM_UI_MODE_MENU || ui->mode == MP_TERM_UI_MODE_SPLASH)
		return(0);
	if(ui->rows < 4 || bot <= top)
		return(0);
	if(!ui->term || (ui->term->caps & MP_TERM_CAP_VT102) == 0)
		return(0);
	up = !_ui_row_same(ui->page[top], ui->sent[top], ui->cols);
	down = !_ui_row_same(ui->page[bot], ui->sent[bot], ui->cols);
	if(up) {
		for(r = top; r < bot; r++)
			if(!_ui_row_same(ui->page[r], ui->sent[r + 1], ui->cols)) {
				up = 0;
				break;
			}
	}
	if(down) {
		for(r = top; r < bot; r++)
			if(!_ui_row_same(ui->page[r + 1], ui->sent[r], ui->cols)) {
				down = 0;
				break;
			}
	}
	if(up)
		return(1);
	if(down)
		return(-1);
	return(0);
}

static void _ui_sgr(mp_term_ui_t *ui, int fg, int bg, int flags) {
	unsigned int style = 0;
	int look = flags & (MP_TERM_UI_BOLD | MP_TERM_UI_UNDER);

	if(fg == ui->emitFg && bg == ui->emitBg && look == ui->emitStyle)
		return;
	mp_term_style(ui->term, MP_TERM_STYLE_RESET);
	mp_term_fg(ui->term, fg & 7);
	mp_term_bg(ui->term, bg & 7);
	if(flags & MP_TERM_UI_BOLD)
		style |= MP_TERM_STYLE_BOLD;
	if(flags & MP_TERM_UI_UNDER)
		style |= MP_TERM_STYLE_UNDERLINE;
	if(style)
		mp_term_style(ui->term, style);
	ui->emitFg = fg & 7;
	ui->emitBg = bg & 7;
	ui->emitStyle = look;
}

/*
 * Line cells are stored as DEC graphics letters. Minicom does not switch
 * to DEC graphics, so they go out as UTF-8 box drawing (rounded corners)
 * when the glass decodes it, else as plain ASCII.
 */
static void _ui_line_glyph(mp_term_ui_t *ui, int ch) {
	static const struct {
		char dec;
		char ascii;
		const char *utf8;
	} map[] = {
		{ 'j', '+', "\xe2\x95\xaf" },
		{ 'k', '+', "\xe2\x95\xae" },
		{ 'l', '+', "\xe2\x95\xad" },
		{ 'm', '+', "\xe2\x95\xb0" },
		{ 'q', '-', "\xe2\x94\x80" },
		{ 'x', '|', "\xe2\x94\x82" },
		{ 'n', '+', "\xe2\x94\xbc" },
		{ 't', '+', "\xe2\x94\x9c" },
		{ 'u', '+', "\xe2\x94\xa4" },
		{ 'v', '+', "\xe2\x94\xb4" },
		{ 'w', '+', "\xe2\x94\xac" },
		{ '`', '*', "\xe2\x97\x86" },
		{ 'a', '.', "\xe2\x96\x91" },
		{ '~', '.', "\xc2\xb7" },
		{ 'f', 'o', "\xc2\xb0" },
	};
	int i;

	for(i = 0; i < (int)(sizeof(map) / sizeof(map[0])); i++) {
		if(map[i].dec != ch)
			continue;
		if(ui->utf8 == YES)
			mp_term_puts(ui->term, (const unsigned char *)map[i].utf8,
				(int)strlen(map[i].utf8));
		else
			mp_term_putc(ui->term, (unsigned char)map[i].ascii);
		return;
	}
	mp_term_putc(ui->term, (unsigned char)ch);
}

static void _ui_diff(mp_term_ui_t *ui) {
	size_t span = (size_t)ui->cols * sizeof(ui->page[0][0]);
	int wide = ui->term->cols > ui->cols;
	int r, c;

	for(r = 0; r < ui->rows; r++) {
		int bar = wide && ui->mode != MP_TERM_UI_MODE_SPLASH && (r == 0 || r == ui->rows - 1);

		if(ui->depth == 0 && memcmp(ui->page[r], ui->sent[r], span) == 0)
			continue;
		for(c = 0; c < ui->cols; c++) {
			unsigned short want;
			int ch, fg, bg, flags;

			if(!_ui_cover(ui, r, c, &want))
				want = ui->page[r][c];
			if(want == ui->sent[r][c])
				continue;
			if(ui->emitRow != r || ui->emitCol != c) {
				mp_term_move(ui->term, r + 1, c + 1);
				ui->emitRow = r;
				ui->emitCol = c;
			}
			ch = _ui_ch(want);
			fg = _ui_fg(want);
			bg = _ui_bg(want);
			flags = _ui_fl(want);
			_ui_sgr(ui, fg, bg, flags);
			if(flags & MP_TERM_UI_DEC)
				_ui_line_glyph(ui, ch);
			else
				mp_term_putc(ui->term, (unsigned char)ch);
			ui->sent[r][c] = want;
			ui->emitCol++;
			if(ui->emitCol >= ui->cols) {
				/* past the buffer the bar goes on to the edge of the glass */
				if(bar)
					mp_term_puts(ui->term, (const unsigned char *)"\033[K", 3);
				ui->emitCol = -1;
				ui->emitRow = -1;
			}
		}
	}
	if(ui->curRow >= 0 && ui->curCol >= 0 && ui->mode == MP_TERM_UI_MODE_PAGE) {
		if(ui->emitRow != ui->curRow || ui->emitCol != ui->curCol) {
			mp_term_move(ui->term, ui->curRow + 1, ui->curCol + 1);
			ui->emitRow = ui->curRow;
			ui->emitCol = ui->curCol;
		}
		if(ui->cursorShown != YES) {
			mp_term_cursor(ui->term, 1);
			ui->cursorShown = YES;
		}
	}
	else if(ui->cursorShown == YES) {
		mp_term_cursor(ui->term, 0);
		ui->cursorShown = NO;
	}
}

static void _ui_invalidate(mp_term_ui_t *ui) {
	memset(ui->sent, 0, sizeof(ui->sent));
	ui->emitCol = -1;
	ui->emitRow = -1;
	ui->emitFg = -1;
	ui->emitDec = 0;
}

static void _ui_clamp_size(mp_term_ui_t *ui, int rows, int cols) {
	if(rows > MP_TERM_UI_ROWS)
		rows = MP_TERM_UI_ROWS;
	if(cols > MP_TERM_UI_COLS)
		cols = MP_TERM_UI_COLS;
	if(rows < 1)
		rows = 1;
	if(cols < 1)
		cols = 1;
	ui->rows = rows;
	ui->cols = cols;
}

static void _ui_open_page(mp_term_ui_t *ui, int node) {
	ui->open = node;
	ui->mode = MP_TERM_UI_MODE_PAGE;
	ui->menuDepth = 0;
	ui->bodyDirty = YES;
}

static void _ui_activate(mp_term_ui_t *ui, int node) {
	int leaf = _ui_first_leaf(ui, node);

	if(leaf >= 0)
		_ui_open_page(ui, leaf);
}

/* Esc opens the main panel, on the group the page in use belongs to */
static void _ui_open_menus(mp_term_ui_t *ui) {
	int top = ui->open;
	int guard = 0;

	if(_ui_count(ui, -1) < 1)
		return;
	while(top >= 0 && ui->node[top].parent >= 0 && guard++ < MP_TERM_UI_DEPTH)
		top = ui->node[top].parent;
	ui->menuDepth = 1;
	ui->menuParent[0] = -1;
	ui->menuCursor[0] = top >= 0 ? _ui_child_index(ui, -1, top) : 0;
	ui->mode = MP_TERM_UI_MODE_MENU;
}

static void _ui_close_menu(mp_term_ui_t *ui) {
	ui->menuDepth = 0;
	ui->mode = MP_TERM_UI_MODE_PAGE;
	ui->bodyDirty = YES;
}

/* Esc and Left go back one level, and close the main panel */
static void _ui_menu_back(mp_term_ui_t *ui) {
	ui->menuDepth--;
	if(ui->menuDepth < 1)
		_ui_close_menu(ui);
}

static void _ui_reset_glass(mp_term_ui_t *ui);

/* the lines welcome() stored, played again */
static void _ui_to_splash(mp_term_ui_t *ui) {
	ui->menuDepth = 0;
	ui->depth = 0;
	ui->mode = MP_TERM_UI_MODE_SPLASH;
	ui->splashAt = mp_clock_ticks();
	ui->splashFrame = 0;
	ui->splashBeat = ui->splashAt;
	ui->glass = NO;
	_ui_reset_glass(ui);
}

static void _ui_menu_key(mp_term_ui_t *ui, int key) {
	int parent, count, cursor, level, child;

	if(ui->menuDepth < 1) {
		_ui_close_menu(ui);
		return;
	}
	level = ui->menuDepth - 1;
	parent = ui->menuParent[level];
	count = _ui_menu_count(ui, parent);
	if(count < 1) {
		_ui_menu_back(ui);
		return;
	}
	cursor = ui->menuCursor[level];
	if(cursor < 0)
		cursor = 0;
	if(cursor >= count)
		cursor = count - 1;
	if(key == MP_TERM_KEY_UP)
		cursor = (cursor + count - 1) % count;
	else if(key == MP_TERM_KEY_DOWN)
		cursor = (cursor + 1) % count;
	else if(key == MP_TERM_KEY_HOME)
		cursor = 0;
	else if(key == MP_TERM_KEY_END)
		cursor = count - 1;
	else if(key == MP_TERM_KEY_ENTER || key == MP_TERM_KEY_RIGHT) {
		ui->menuCursor[level] = cursor;
		if(parent < 0 && cursor == _ui_count(ui, parent)) {
			_ui_to_splash(ui);
			return;
		}
		child = _ui_nth(ui, parent, cursor);
		if(child >= 0 && ui->node[child].child >= 0) {
			if(key == MP_TERM_KEY_RIGHT || ui->node[child].page == NULL) {
				if(ui->menuDepth < MP_TERM_UI_DEPTH) {
					ui->menuParent[ui->menuDepth] = child;
					ui->menuCursor[ui->menuDepth] = 0;
					ui->menuDepth++;
				}
				return;
			}
		}
		if(child >= 0)
			_ui_activate(ui, child);
		return;
	}
	else if(key == MP_TERM_KEY_ESCAPE || key == MP_TERM_KEY_LEFT) {
		ui->menuCursor[level] = cursor;
		_ui_menu_back(ui);
		return;
	}
	ui->menuCursor[level] = cursor;
}

static int _ui_accel_at(const mp_term_ui_t *ui, int parent, int letter) {
	int n = _ui_count(ui, parent);
	int i, node;

	for(i = 0; i < n; i++) {
		node = _ui_nth(ui, parent, i);
		if(node >= 0 && ui->node[node].accel == (char)letter)
			return(node);
	}
	return(-1);
}

static void _ui_alt(mp_term_ui_t *ui, int ch) {
	int nodes[MP_TERM_UI_NODES];
	int count = _ui_menu_collect(ui, nodes);
	int letter, node, i;

	if(ch >= '1' && ch <= '9') {
		i = ch - '1';
		if(i < count)
			_ui_activate(ui, nodes[i]);
		return;
	}
	letter = _ui_lower(ch);
	for(i = 0; i < count; i++) {
		if(ui->node[nodes[i]].accel == (char)letter) {
			_ui_activate(ui, nodes[i]);
			return;
		}
	}
	/* a group letter (Alt+S for System) opens that group */
	node = _ui_accel_at(ui, -1, letter);
	if(node < 0 || ui->node[node].child < 0)
		return;
	_ui_open_group(ui, node);
}

#define _UI_FRAME_MS 40
#define _UI_WAIT_MS  1500
#define _UI_SPLASH_W 120
#define _UI_LABEL_W  11
#define _UI_OPEN     6


/* break a value on spaces, at most three rows of width w */
static int _ui_wrap(const char *s, int w, int max, int *from, int *len) {
	int n = 0;
	int at = 0;
	int total = (int)strlen(s);

	if(w < 4)
		w = 4;
	while(at < total && n < max) {
		int cut = total - at;

		if(cut > w) {
			cut = w;
			while(cut > w / 2 && s[at + cut] != ' ')
				cut--;
			if(s[at + cut] != ' ')
				cut = w;
		}
		if(n == max - 1 && total - at > w)
			cut = w;
		from[n] = at;
		len[n] = cut;
		n++;
		at += cut;
		while(s[at] == ' ')
			at++;
	}
	if(n == 0) {
		from[0] = 0;
		len[0] = 0;
		n = 1;
	}
	return(n);
}

static int _ui_logo_fg(int x, int y, int width) {
	static const int ramp[5] = { 5, 4, 6, 2, 3 };
	int span = width + 10;

	if(span < 1)
		span = 1;
	return(ramp[((x + 2 * y) * 5 / span) % 5]);
}

/* a DEC line frame, blue: corners l k m j, edges q x */
static void _ui_box(mp_term_ui_t *ui, int top, int left, int h, int w) {
	int flags = MP_TERM_UI_DEC | MP_TERM_UI_BOLD;
	int r, c, bot = top + h - 1, right = left + w - 1;

	if(h < 2 || w < 2 || top < 0 || left < 0 || right >= ui->cols)
		return;
	if(bot >= ui->rows)
		bot = ui->rows - 1;
	for(c = left + 1; c < right; c++) {
		ui->page[top][c] = _ui_cell('q', 4, 0, flags);
		ui->page[bot][c] = _ui_cell('q', 4, 0, flags);
	}
	for(r = top + 1; r < bot; r++) {
		ui->page[r][left] = _ui_cell('x', 4, 0, flags);
		ui->page[r][right] = _ui_cell('x', 4, 0, flags);
	}
	ui->page[top][left] = _ui_cell('l', 4, 0, flags);
	ui->page[top][right] = _ui_cell('k', 4, 0, flags);
	ui->page[bot][left] = _ui_cell('m', 4, 0, flags);
	ui->page[bot][right] = _ui_cell('j', 4, 0, flags);
}

static int _ui_frame(const mp_term_ui_t *ui) {
	unsigned long step = _ui_ms(_UI_FRAME_MS);

	return((int)((mp_clock_ticks() - ui->splashAt) / step));
}

/*
 * The welcome card, centered: a frame with the board on top and the glass
 * size under it, the logo swept in column by column, then the identity
 * lines one by one, then a shine. The last printk line ticks underneath.
 */
static int _ui_splash_render(mp_term_ui_t *ui) {
	int art[MP_TERM_UI_SPLASH];
	int info[MP_TERM_UI_SPLASH];
	int wrapFrom[MP_TERM_UI_SPLASH][3];
	int wrapLen[MP_TERM_UI_SPLASH][3];
	int wraps[MP_TERM_UI_SPLASH];
	int nart = 0, ninfo = 0, artW = 0, valW = 0;
	int i, r, c, k, w, h, top, left, inner, rows, wrapMax, showArt, showInfo;
	int t, reveal, infoAt, shineAt, shine, end;
	char text[32];

	for(r = 0; r < ui->rows; r++)
		for(c = 0; c < ui->cols; c++)
			ui->page[r][c] = _ui_cell(' ', 7, 0, 0);
	for(i = 0; i < ui->splashN; i++) {
		if(ui->splashLabel[i][0] == 0 && ui->splashValue[i][0] != 0) {
			art[nart++] = i;
			if((int)strlen(ui->splashValue[i]) > artW)
				artW = (int)strlen(ui->splashValue[i]);
		}
		else if(ui->splashLabel[i][0] != 0) {
			info[ninfo++] = i;
			if((int)strlen(ui->splashValue[i]) > valW)
				valW = (int)strlen(ui->splashValue[i]);
		}
	}

	w = ui->cols - 4;
	if(w > _UI_SPLASH_W)
		w = _UI_SPLASH_W;
	inner = w - 6;
	if(inner < 10)
		inner = 10;

	/* shrink until it fits: wrap less, then drop the logo, then lines */
	showArt = (nart > 0 && artW <= inner);
	showInfo = ninfo;
	for(wrapMax = 3; ; ) {
		rows = 0;
		for(k = 0; k < ninfo; k++) {
			wraps[k] = _ui_wrap(ui->splashValue[info[k]], inner - _UI_LABEL_W, wrapMax,
				wrapFrom[k], wrapLen[k]);
			if(k < showInfo)
				rows += wraps[k];
		}
		/* borders, a blank row inside each, the logo and its gap, the rule and the lines */
		h = 4 + (showArt ? nart + 1 : 0) + (showInfo ? rows + 1 : 0);
		if(showArt && !showInfo)
			h--;
		if(h + 3 <= ui->rows || (wrapMax == 1 && !showArt && showInfo == 0))
			break;
		if(wrapMax > 1)
			wrapMax--;
		else if(showArt)
			showArt = 0;
		else
			showInfo--;
	}
	if(h > ui->rows)
		h = ui->rows;
	top = (ui->rows - (h + 3)) / 2;
	if(top < 0)
		top = 0;
	left = (ui->cols - w) / 2;
	if(left < 0)
		left = 0;

	/* the card opens from its middle, then fills */
	t = _ui_frame(ui);
	ui->splashFrame = t;
	if(t < _UI_OPEN) {
		int hh = 2 + ((h - 2) * (t + 1)) / _UI_OPEN;
		int ww = 4 + ((w - 4) * (t + 1)) / _UI_OPEN;

		_ui_box(ui, top + (h - hh) / 2, left + (w - ww) / 2, hh, ww);
		ui->curRow = -1;
		ui->curCol = -1;
		return(TRUE);
	}
	t -= _UI_OPEN;
	reveal = t * 3;
	infoAt = showArt ? (artW + 2) / 3 : 0;
	shineAt = infoAt + showInfo + 4;
	shine = (t - shineAt) * 3;
	end = shineAt + (artW + 8) / 3;

	_ui_box(ui, top, left, h, w);
	if(ui->place[0]) {
		int n = (int)strlen(ui->place) + 2;

		_ui_raw(ui, top, left + 3, " ", 1, 0, 6, 0);
		_ui_raw(ui, top, left + 4, ui->place, n - 2, 0, 6, 0);
		_ui_raw(ui, top, left + 2 + n, " ", 1, 0, 6, 0);
	}
	if(ui->term && ui->term->rows > 0) {
		snprintf(text, sizeof(text), " %dx%d ", ui->term->rows, ui->term->cols);
		_ui_raw(ui, top + h - 1, left + w - 3 - (int)strlen(text), text, 32, 4, 0, MP_TERM_UI_BOLD);
	}

	r = top + 2;
	if(showArt) {
		int x0 = left + (w - artW) / 2;

		for(k = 0; k < nart; k++, r++) {
			const char *s = ui->splashValue[art[k]];

			for(i = 0; s[i]; i++) {
				int fg, flags = MP_TERM_UI_BOLD;

				if(s[i] == ' ' || i > reveal)
					continue;
				fg = _ui_logo_fg(i, k, artW);
				if(i >= reveal - 2 || (shine >= 0 && i >= shine - 2 && i <= shine))
					fg = 7;
				ui->page[r][x0 + i] = _ui_cell((unsigned char)s[i], fg, 0, flags);
			}
		}
		r++;
	}
	if(showInfo) {
		int x0 = left + 3;

		for(c = 0; c < w - 6 && t >= infoAt; c++)
			ui->page[r][x0 + c] = _ui_cell('q', 4, 0, MP_TERM_UI_DEC);
		r++;
		for(k = 0; k < showInfo && r < top + h - 1; k++) {
			int age = t - infoAt - k;
			const char *v = ui->splashValue[info[k]];
			int fresh = age >= 0 && age < 2;

			if(age < 0) {
				r += wraps[k];
				continue;
			}
			_ui_raw(ui, r, x0, ui->splashLabel[info[k]], _UI_LABEL_W, fresh ? 7 : 6, 0, MP_TERM_UI_BOLD);
			for(i = 0; i < wraps[k] && r < top + h - 1; i++, r++)
				_ui_raw(ui, r, x0 + _UI_LABEL_W, v + wrapFrom[k][i], wrapLen[k][i],
					7, 0, fresh ? MP_TERM_UI_BOLD : 0);
		}
	}

	/* under the card: the call to action, then the boot ticker */
	r = top + h + 1;
	if(t >= shineAt && r < ui->rows) {
		static const char *go = "press any key";
		unsigned long hz = mp_clock_tick_hz();
		int on = hz ? (int)((mp_clock_ticks() / hz) & 1) : 0;
		int n = (int)strlen(go);

		_ui_raw(ui, r, (ui->cols - n) / 2, go, n, on ? 7 : 6, 0, on ? MP_TERM_UI_BOLD : 0);
	}
	r++;
	if(ui->logCount > 0 && r < ui->rows) {
		char stamp[16];
		char last[MP_TERM_UI_LOG_LEN];
		unsigned long at = 0;
		int n, x;

		last[0] = 0;
		MP_INTERRUPT_SAFE_BEGIN
		if(ui->logCount > 0) {
			int slot = _ui_log_slot(ui, ui->logCount - 1);

			at = ui->log[slot].ticks;
			memcpy(last, ui->log[slot].text, sizeof(last));
		}
		MP_INTERRUPT_SAFE_END
		last[sizeof(last) - 1] = 0;
		n = (int)strlen(last);
		_ui_stamp(at, stamp, (int)sizeof(stamp));
		if(n > w - 12)
			n = w - 12;
		if(n < 0)
			n = 0;
		x = (ui->cols - (n + 11)) / 2;
		if(x < 0)
			x = 0;
		_ui_raw(ui, r, x, stamp, 9, 4, 0, MP_TERM_UI_BOLD);
		_ui_raw(ui, r, x + 11, last, n, 4, 0, MP_TERM_UI_BOLD);
	}
	ui->curRow = -1;
	ui->curCol = -1;
	return(t < end);
}

static void _ui_reset_glass(mp_term_ui_t *ui) {
	_ui_invalidate(ui);
	/* the first time the host answers, take the screen. A reset drops the
	 * opening sequence, and this one goes out once the line is alive. */
	if(ui->glass != YES) {
		ui->glass = YES;
		mp_term_alt(ui->term, 0);
		mp_term_cursor(ui->term, 0);
		mp_term_alt(ui->term, 1);
	}
	mp_term_style(ui->term, MP_TERM_STYLE_RESET);
	mp_term_graphic(ui->term, 0);
	mp_term_region(ui->term, 0, 0);
	mp_term_clear(ui->term);
	ui->cursorShown = NO;
	if(ui->mode != MP_TERM_UI_MODE_SPLASH && ui->rows > 3)
		mp_term_region(ui->term, 2, ui->rows - 1);
	ui->emitRow = -1;
	ui->emitCol = -1;
	ui->bodyDirty = YES;
}

static void _ui_resize(mp_term_ui_t *ui, int rows, int cols) {
	int clampedRows, clampedCols;

	if(rows < 1 || cols < 1 || !ui->term)
		return;
	ui->term->rows = rows;
	ui->term->cols = cols;
	clampedRows = rows > MP_TERM_UI_ROWS ? MP_TERM_UI_ROWS : rows;
	clampedCols = cols > MP_TERM_UI_COLS ? MP_TERM_UI_COLS : cols;
	if(ui->sized != YES || clampedRows != ui->rows || clampedCols != ui->cols) {
		_ui_clamp_size(ui, rows, cols);
		/* a new size replays the splash, so the logo is framed for this glass */
		if(ui->mode == MP_TERM_UI_MODE_SPLASH) {
			ui->splashAt = mp_clock_ticks();
			ui->splashFrame = 0;
			ui->splashBeat = ui->splashAt;
		}
		ui->sized = YES;
		_ui_reset_glass(ui);
	}
	mp_term_ui_flush(ui);
}

void mp_term_ui_redraw(mp_term_ui_t *ui) {
	if(!ui || !ui->term || ui->rows < 1)
		return;
	ui->term->utf8Known = NO;
	ui->glass = NO;
	_ui_reset_glass(ui);
	mp_term_ui_flush(ui);
}

void mp_term_ui_unsent(mp_term_ui_t *ui) {
	if(!ui)
		return;
	_ui_invalidate(ui);
}

unsigned long mp_term_ui_waiting(const mp_term_ui_t *ui) {
	unsigned long now, step;

	if(!ui || ui->mode != MP_TERM_UI_MODE_SPLASH)
		return(0);
	now = mp_clock_ticks();
	if(ui->sized != YES) {
		unsigned long wait = _ui_ms(_UI_WAIT_MS);
		unsigned long gone = now - ui->startAt;

		return(gone >= wait ? 1 : wait - gone);
	}
	if(ui->splashFrame < 0)
		return(0);
	step = _ui_ms(_UI_FRAME_MS);
	return(step);
}

void mp_term_ui_flush(mp_term_ui_t *ui) {
	mp_task_lock_t key;
	int dir;

	if(!ui || !ui->term || !ui->term->write)
		return;
	ui->flushedLog = ui->logHead;
	/* the splash waits for the glass: either the host answers the probe
	 * (a resize lands) or the driver signals the host is taking packets
	 * (mp_term_ui_host_ready). Painting before the host listens wedges the
	 * IN fifo with a burst no one drains, so the card never arrives. */
	if(ui->mode == MP_TERM_UI_MODE_SPLASH && ui->sized != YES)
		return;
	if(ui->rows < 1 || ui->cols < 1)
		return;
	if(ui->term->utf8Known == YES && ui->term->utf8 != ui->utf8) {
		ui->utf8 = ui->term->utf8;
		_ui_invalidate(ui);
		if(ui->mode == MP_TERM_UI_MODE_SPLASH) {
			ui->splashAt = mp_clock_ticks();
			ui->splashFrame = 0;
		}
	}
	key = mp_task_lock(MP_TASK_CEILING_KERNEL);
	if(ui->mode == MP_TERM_UI_MODE_SPLASH) {
		if(!_ui_splash_render(ui))
			ui->splashFrame = -1;
	}
	else
		_ui_render(ui);
	mp_task_unlock(key);
	dir = _ui_body_scrolled(ui);
	if(dir != 0) {
		int top = 1;
		int bot = ui->rows - 2;
		int count = bot - top;
		int c;
		size_t bytes = (size_t)count * (size_t)ui->cols * sizeof(ui->sent[0][0]);

		/* cursor on the first body row: minicom inserts and deletes from there,
		 * inside the region, and does not implement CSI S / CSI T */
		mp_term_region(ui->term, 2, ui->rows - 1);
		mp_term_move(ui->term, 2, 1);
		if(dir > 0) {
			mp_term_delete_line(ui->term, 1);
			if(count > 0)
				memmove(&ui->sent[top][0], &ui->sent[top + 1][0], bytes);
			for(c = 0; c < ui->cols; c++)
				ui->sent[bot][c] = 0;
		}
		else {
			mp_term_insert_line(ui->term, 1);
			if(count > 0)
				memmove(&ui->sent[top + 1][0], &ui->sent[top][0], bytes);
			for(c = 0; c < ui->cols; c++)
				ui->sent[top][c] = 0;
		}
		ui->emitCol = -1;
		ui->emitRow = -1;
		ui->emitFg = -1;
	}
	_ui_diff(ui);
	ui->drew = YES;
}

void mp_term_ui_event(mp_term_ui_t *ui, const mp_term_event_t *event) {
	const mp_term_page_t *page;

	if(!ui || !event)
		return;
	if(event->type == MP_TERM_EVENT_RESIZE) {
		_ui_resize(ui, event->x, event->y);
		return;
	}
	if(event->type == MP_TERM_EVENT_KEY && event->key == MP_TERM_KEY_CTRL_L) {
		mp_term_ui_redraw(ui);
		return;
	}
	if(ui->mode == MP_TERM_UI_MODE_SPLASH) {
		/* a host that reconnects (minicom) sends noise as it opens the line;
		 * a stray printable byte would dismiss the card before anyone sees it.
		 * Wait for the glass to have been up for a beat, and only a real key
		 * (not text) leaves the splash. */
		if(event->type == MP_TERM_EVENT_KEY &&
				(unsigned long)(mp_clock_ticks() - ui->splashBeat) >= _ui_ms(300))
			mp_term_ui_enter(ui);
		return;
	}
	if(event->type == MP_TERM_EVENT_KEY && event->key == MP_TERM_KEY_ESCAPE) {
		if(ui->mode == MP_TERM_UI_MODE_MENU)
			_ui_menu_key(ui, MP_TERM_KEY_ESCAPE);
		else
			_ui_open_menus(ui);
		mp_term_ui_flush(ui);
		return;
	}
	if(event->type == MP_TERM_EVENT_KEY && MP_TERM_KEY_IS_ALT(event->key)) {
		_ui_alt(ui, MP_TERM_KEY_ALT_CHAR(event->key));
		mp_term_ui_flush(ui);
		return;
	}
	if(ui->mode == MP_TERM_UI_MODE_MENU) {
		if(event->type == MP_TERM_EVENT_KEY)
			_ui_menu_key(ui, event->key);
		mp_term_ui_flush(ui);
		return;
	}
	page = NULL;
	if(ui->open >= 0 && ui->node[ui->open].used == YES)
		page = ui->node[ui->open].page;
	if(page && page->key && page->key(ui, event, page->user))
		ui->bodyDirty = YES;
	mp_term_ui_flush(ui);
}

void mp_term_ui_enter(mp_term_ui_t *ui) {
	if(!ui || !ui->term)
		return;
	if(ui->sized != YES || ui->rows < 4 || ui->cols < 20) {
		int rows = ui->term->rows > 0 ? ui->term->rows : 24;
		int cols = ui->term->cols > 0 ? ui->term->cols : 80;

		_ui_clamp_size(ui, rows, cols);
		ui->sized = YES;
	}
	if(ui->mode == MP_TERM_UI_MODE_SPLASH) {
		ui->mode = MP_TERM_UI_MODE_PAGE;
		_ui_reset_glass(ui);
	}
	ui->bodyDirty = YES;
	mp_term_ui_flush(ui);
}

void mp_term_ui_welcome(mp_term_ui_t *ui, const mp_term_banner_line_t *lines, int n) {
	int i;

	if(!ui)
		return;
	if(!lines || n < 0)
		n = 0;
	if(n > MP_TERM_UI_SPLASH)
		n = MP_TERM_UI_SPLASH;
	ui->splashN = n;
	for(i = 0; i < n; i++) {
		_ui_copy(ui->splashLabel[i], (int)sizeof(ui->splashLabel[i]),
			lines[i].label ? lines[i].label : "");
		_ui_copy(ui->splashValue[i], MP_TERM_UI_COLS,
			lines[i].value ? lines[i].value : "");
	}
	ui->mode = MP_TERM_UI_MODE_SPLASH;
	ui->startAt = mp_clock_ticks();
	ui->splashAt = ui->startAt;
	ui->splashFrame = 0;
	ui->sized = NO;
	ui->glass = NO;
	_ui_invalidate(ui);
	ui->cursorShown = NO;
}

void mp_term_ui_replay(mp_term_ui_t *ui) {
	if(!ui || !ui->term)
		return;
	/* the lines welcome() stored. The new glass has none of the old screen. */
	ui->mode = MP_TERM_UI_MODE_SPLASH;
	ui->startAt = mp_clock_ticks();
	ui->splashAt = ui->startAt;
	ui->splashFrame = 0;
	ui->splashBeat = ui->startAt;
	ui->term->state = _MP_TERM_GROUND;
	ui->term->escape = 0;
	ui->term->paramCount = 0;
	ui->term->paramOpen = NO;
	ui->term->probe = NO;
	ui->term->probeUtf8 = NO;
	ui->glass = NO;
	ui->drew = NO;
	_ui_reset_glass(ui);
}

/* the driver felt the host take a packet: the line is alive, so the card can
 * go out without wedging the IN fifo. Used when the host never answers the
 * probe (minicom) but does read. */
void mp_term_ui_host_ready(mp_term_ui_t *ui) {
	if(!ui || !ui->term)
		return;
	if(ui->mode != MP_TERM_UI_MODE_SPLASH || ui->sized == YES)
		return;
	_ui_clamp_size(ui, ui->term->rows > 0 ? ui->term->rows : 24,
		ui->term->cols > 0 ? ui->term->cols : 80);
	ui->sized = YES;
	ui->splashAt = mp_clock_ticks();
	ui->splashBeat = ui->splashAt;
	_ui_reset_glass(ui);
}

void mp_term_ui_note_probe(mp_term_ui_t *ui) {
	if(!ui)
		return;
	if(ui->term && ui->term->probeUtf8 == YES && ui->rows > 0) {
		int c;

		for(c = 0; c < 3 && c < ui->cols; c++)
			ui->sent[0][c] = 0;
	}
	ui->drew = NO;
	ui->cursorShown = NO;
	ui->emitCol = -1;
	ui->emitRow = -1;
	ui->emitFg = -1;
}

void mp_term_ui_place(mp_term_ui_t *ui, const char *where) {
	if(!ui)
		return;
	_ui_copy(ui->place, (int)sizeof(ui->place), where ? where : "");
}

int mp_term_ui_at(mp_term_ui_t *ui, int row, int col, int *ch, int *fg, int *bg) {
	unsigned short cell;

	if(!ui || row < 0 || col < 0 || row >= ui->rows || col >= ui->cols)
		return(0);
	_ui_layout(ui);
	if(!_ui_cover(ui, row, col, &cell))
		cell = ui->page[row][col];
	if(ch)
		*ch = _ui_ch(cell);
	if(fg)
		*fg = _ui_fg(cell);
	if(bg)
		*bg = _ui_bg(cell);
	return(1);
}

void mp_term_ui_log(mp_term_ui_t *ui, const char *text) {
	char clean[MP_TERM_UI_LOG_LEN];
	int i, n, slot;
	unsigned long ticks;

	if(!ui || !text)
		return;
	n = 0;
	for(i = 0; text[i] && n + 1 < MP_TERM_UI_LOG_LEN; i++) {
		unsigned char c = (unsigned char)text[i];

		if(c == '\n' || c == '\r' || c == '\t')
			c = ' ';
		if(c < 0x20 || c > 0x7e)
			continue;
		clean[n++] = (char)c;
	}
	clean[n] = 0;
	ticks = mp_clock_ticks();
	MP_INTERRUPT_SAFE_BEGIN
	slot = ui->logHead;
	ui->log[slot].ticks = ticks;
	memcpy(ui->log[slot].text, clean, (size_t)n + 1);
	ui->logHead = (slot + 1) % MP_TERM_UI_LOG;
	ui->logSeq++;
	if(ui->logCount < MP_TERM_UI_LOG)
		ui->logCount++;
	if(ui->mode == MP_TERM_UI_MODE_PAGE && ui->open == ui->logNode)
		ui->logUnread = 0;
	else if(ui->logUnread < 999)
		ui->logUnread++;
	if(ui->logNode >= 0 && ui->node[ui->logNode].used == YES)
		ui->node[ui->logNode].badge = ui->logUnread;
	MP_INTERRUPT_SAFE_END
	if(ui->term && ui->term->task)
		mp_task_wake(ui->term->task);
}

mp_ret_t mp_term_ui_init(mp_term_ui_t *ui, mp_term_t *term) {
	if(!ui || !term)
		return(FALSE);
	memset(ui, 0, sizeof(*ui));
	ui->term = term;
	ui->root = -1;
	ui->open = -1;
	ui->conNode = -1;
	ui->logNode = -1;
	ui->conHistAt = -1;
	ui->curRow = -1;
	ui->curCol = -1;
	ui->emitRow = -1;
	ui->emitCol = -1;
	ui->emitFg = -1;
	ui->logFollow = YES;
	ui->mode = MP_TERM_UI_MODE_SPLASH;
	ui->bodyDirty = YES;
	if(_ui_add(ui, "Console", &mp_term_page_console, YES) != TRUE)
		return(FALSE);
	if(_ui_add(ui, "Messages", &mp_term_page_messages, YES) != TRUE)
		return(FALSE);
	ui->open = ui->conNode;
	return(TRUE);
}

mp_ret_t mp_term_ui_add(mp_term_ui_t *ui, const char *path, const mp_term_page_t *page) {
	return(_ui_add(ui, path, page, NO));
}

void mp_term_ui_remove(mp_term_ui_t *ui, const char *path) {
	int node, parent;
	mp_task_lock_t key;

	if(!ui || !path)
		return;
	key = mp_task_lock(MP_TASK_CEILING_KERNEL);
	node = _ui_find_path(ui, path);
	if(node < 0) {
		mp_task_unlock(key);
		return;
	}
	parent = ui->node[node].parent;
	_ui_unlink(ui, node);
	_ui_free_tree(ui, node);
	_ui_prune(ui, parent);
	ui->bodyDirty = YES;
	mp_task_unlock(key);
}

/**@}*/

#endif
