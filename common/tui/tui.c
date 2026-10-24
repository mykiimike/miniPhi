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
@defgroup mpCommonTUICore TUI tree and band renderer

@ingroup mpCommonTUI

@{
*/

static const mp_tui_class_t _mp_tui_root_class = { "root", NULL, NULL, NULL };

static void _mp_tui_cursor_draw(mp_tui_t *ui, mp_tui_draw_t *draw);

static int _mp_tui_intersect(const mp_tui_rect_t *a, const mp_tui_rect_t *b, mp_tui_rect_t *out) {
	int x0 = a->x > b->x ? a->x : b->x;
	int y0 = a->y > b->y ? a->y : b->y;
	int x1 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
	int y1 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;

	if(x1 <= x0 || y1 <= y0) {
		out->x = out->y = out->w = out->h = 0;
		return(0);
	}
	out->x = x0;
	out->y = y0;
	out->w = x1 - x0;
	out->h = y1 - y0;
	return(1);
}

static void _mp_tui_union(const mp_tui_rect_t *a, const mp_tui_rect_t *b, mp_tui_rect_t *out) {
	int x0 = a->x < b->x ? a->x : b->x;
	int y0 = a->y < b->y ? a->y : b->y;
	int x1 = a->x + a->w > b->x + b->w ? a->x + a->w : b->x + b->w;
	int y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;

	out->x = x0;
	out->y = y0;
	out->w = x1 - x0;
	out->h = y1 - y0;
}

static long _mp_tui_area(const mp_tui_rect_t *rect) {
	return((long)rect->w * rect->h);
}

static void _mp_tui_client(mp_tui_widget_t *widget, mp_tui_rect_t *rect) {
	if(widget->type && widget->type->client) {
		widget->type->client(widget, rect);
		return;
	}
	rect->x = 0;
	rect->y = 0;
	rect->w = widget->rect.w;
	rect->h = widget->rect.h;
}

/* screen rectangles of a widget and everything under it */
static void _mp_tui_place(mp_tui_widget_t *widget) {
	mp_tui_widget_t *child;
	mp_tui_rect_t client;

	if(widget->parent) {
		int shift = 0;

		_mp_tui_client(widget->parent, &client);
		if(widget->parent->type == &mp_tui_view_class)
			shift = ((mp_tui_view_t *)widget->parent)->scroll;
		widget->screen.x = widget->parent->screen.x + client.x + widget->rect.x;
		widget->screen.y = widget->parent->screen.y + client.y + widget->rect.y - shift;
	}
	else {
		widget->screen.x = widget->rect.x;
		widget->screen.y = widget->rect.y;
	}
	widget->screen.w = widget->rect.w;
	widget->screen.h = widget->rect.h;

	for(child = widget->first; child; child = child->next)
		_mp_tui_place(child);
}

/* what is actually visible of a widget: its rect cut by every client area above it */
static int _mp_tui_visible(mp_tui_widget_t *widget, mp_tui_rect_t *out) {
	mp_tui_widget_t *parent;
	mp_tui_rect_t client;

	*out = widget->screen;
	for(parent = widget->parent; parent; parent = parent->parent) {
		if(parent->hidden)
			return(0);
		_mp_tui_client(parent, &client);
		client.x += parent->screen.x;
		client.y += parent->screen.y;
		if(!_mp_tui_intersect(out, &client, out))
			return(0);
	}
	return(1);
}

int mp_tui_init(mp_tui_t *ui, mp_tui_display_t *display, mp_tui_color_t *buffer, unsigned int bufferSize) {
	memset(ui, 0, sizeof(*ui));
	if(!display || !display->write || !buffer || display->width <= 0 || display->height <= 0)
		return(0);
	if(bufferSize < (unsigned int)display->width)
		return(0);

	ui->display = display;
	ui->buffer = buffer;
	ui->bufferSize = bufferSize;

	ui->theme.background = MP_TUI_RGB(0x10, 0x14, 0x1C);
	ui->theme.surface = MP_TUI_RGB(0x1E, 0x24, 0x30);
	ui->theme.foreground = MP_TUI_RGB(0xE8, 0xEA, 0xEE);
	ui->theme.muted = MP_TUI_RGB(0x8A, 0x92, 0xA0);
	ui->theme.accent = MP_TUI_RGB(0x2E, 0xB8, 0x8A);
	ui->theme.border = MP_TUI_RGB(0x3A, 0x42, 0x50);
	ui->theme.titleBackground = MP_TUI_RGB(0x2A, 0x32, 0x40);
	ui->theme.titleForeground = MP_TUI_RGB(0xFF, 0xFF, 0xFF);
	ui->theme.font = &mp_tui_font_default;
	ui->saver = MP_TUI_AWAKE;
	ui->activeAt = mp_clock_ticks();

	ui->root.type = &_mp_tui_root_class;
	ui->root.tui = ui;
	ui->root.rect.w = ui->root.screen.w = display->width;
	ui->root.rect.h = ui->root.screen.h = display->height;

	mp_tui_refresh(ui);
	return(1);
}

void mp_tui_fini(mp_tui_t *ui) {
	mp_tui_detach(ui);
	while(ui->root.first)
		mp_tui_widget_remove(ui->root.first);
	ui->display = NULL;
}

mp_tui_widget_t *mp_tui_root(mp_tui_t *ui) {
	return(&ui->root);
}

void mp_tui_refresh(mp_tui_t *ui) {
	ui->dirtyCount = 0;
	ui->drawing = 0;
	mp_tui_invalidate_rect(ui, &ui->root.screen);
}

static void _mp_tui_dirty(mp_tui_t *ui, const mp_tui_rect_t *area);

void mp_tui_invalidate_rect(mp_tui_t *ui, const mp_tui_rect_t *rect) {
	mp_tui_rect_t area;

	if(!ui->display || !_mp_tui_intersect(rect, &ui->root.screen, &area))
		return;
	_mp_tui_dirty(ui, &area);

	/* off, it waits for the screen to wake up */
	if(ui->task && ui->saver != MP_TUI_OFF)
		mp_task_wake(ui->task);
}

static void _mp_tui_dirty(mp_tui_t *ui, const mp_tui_rect_t *rect) {
	mp_tui_rect_t area = *rect, merged;
	long best, cost;
	int a, pick;

	for(a=0; a<ui->dirtyCount; a++) {
		_mp_tui_union(&ui->dirty[a], &area, &merged);
		/* contained, or overlapping enough that one rectangle is cheaper */
		if(_mp_tui_area(&merged) <= _mp_tui_area(&ui->dirty[a]) + _mp_tui_area(&area)) {
			ui->dirty[a] = merged;
			return;
		}
	}

	if(ui->dirtyCount < MP_TUI_DIRTY_MAX) {
		ui->dirty[ui->dirtyCount++] = area;
		return;
	}

	/* full: grow the rectangle that grows the least */
	pick = 0;
	best = -1;
	for(a=0; a<ui->dirtyCount; a++) {
		_mp_tui_union(&ui->dirty[a], &area, &merged);
		cost = _mp_tui_area(&merged) - _mp_tui_area(&ui->dirty[a]);
		if(best < 0 || cost < best) {
			best = cost;
			pick = a;
		}
	}
	_mp_tui_union(&ui->dirty[pick], &area, &ui->dirty[pick]);
}

int mp_tui_pending(mp_tui_t *ui) {
	return(ui->splash < 3 || ui->drawing || ui->dirtyCount > 0);
}

/* how long the splash stays once it is fully on screen, in ms */
#define _MP_TUI_SPLASH_MS 1500

static unsigned long _mp_tui_ms(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz + 999) / 1000 : ms;
	return(ticks ? ticks : 1);
}

static void _mp_tui_splash_line(mp_tui_t *ui, mp_tui_draw_t *draw, const mp_tui_font_t *font, int y, const char *text, mp_tui_color_t color) {
	int x = (ui->display->width - mp_tui_text_width(font, text)) / 2;
	mp_tui_draw_text(draw, font, x, y, text, color);
}

static void _mp_tui_splash_paint(mp_tui_t *ui, mp_tui_draw_t *draw) {
	const mp_tui_font_t *font = ui->theme.font;
	int gap = font->lineHeight / 2 + 2;
	int logo = mp_tui_logo.w;
	int foot = font->lineHeight;
	int limit = ui->display->height - font->lineHeight - foot - gap * 4;
	int block, top, y;

	if(ui->display->width - 16 < limit)
		limit = ui->display->width - 16;
	if(limit < 8)
		limit = 8;
	if(logo > limit)
		logo = limit;

	block = font->lineHeight + gap + logo + gap + foot;
	top = (ui->display->height - block) / 2;
	if(top < 0)
		top = 0;

	mp_tui_draw_fill(draw, 0, 0, ui->display->width, ui->display->height, ui->theme.background);

	_mp_tui_splash_line(ui, draw, font, top, "miniPhi", ui->theme.foreground);
	mp_tui_draw_bitmap(draw, &mp_tui_logo, (ui->display->width - logo) / 2, top + font->lineHeight + gap, logo, logo);

	y = top + font->lineHeight + gap + logo + gap;
	_mp_tui_splash_line(ui, draw, font, y, MP_KERNEL_VERSION, ui->theme.muted);
}

/* draws one band of the splash; returns non-zero while the splash is up */
static int _mp_tui_splash(mp_tui_t *ui) {
	mp_tui_draw_t draw;
	int rows;

	if(ui->splashHold) {
		unsigned long now = mp_clock_ticks();
		if(now < ui->splashUntil) {
			if(ui->task) {
				ui->task->delay = ui->splashUntil - now;
				return(1);
			}
			mp_clock_delay((int)(ui->splashUntil - now));
		}
		ui->splashHold = 0;
		ui->splash = 3;
		if(ui->task)
			ui->task->delay = 1;
		mp_tui_refresh(ui);
		return(0);
	}

	/* back to one band per tick; the hold above is the only long wait */
	if(ui->task)
		ui->task->delay = 1;

	rows = (int)(ui->bufferSize / (unsigned int)ui->display->width);
	if(rows > ui->display->height - ui->splashRow)
		rows = ui->display->height - ui->splashRow;

	draw.tui = ui;
	draw.pixels = ui->buffer;
	draw.band.x = 0;
	draw.band.y = ui->splashRow;
	draw.band.w = ui->display->width;
	draw.band.h = rows;
	draw.clip = draw.band;

	_mp_tui_splash_paint(ui, &draw);
	ui->display->write(ui->display, 0, ui->splashRow, ui->display->width, rows, ui->buffer);

	ui->splashRow += rows;
	if(ui->splashRow >= ui->display->height) {
		ui->splashRow = 0;
		ui->splashHold = 1;
		ui->splashUntil = mp_clock_ticks() + _mp_tui_ms(_MP_TUI_SPLASH_MS);
		if(ui->task)
			ui->task->delay = _mp_tui_ms(_MP_TUI_SPLASH_MS);
		if(ui->display->flush)
			ui->display->flush(ui->display);
	}
	return(1);
}

static void _mp_tui_draw_tree(mp_tui_widget_t *widget, mp_tui_draw_t *draw, const mp_tui_rect_t *clip) {
	mp_tui_widget_t *child;
	mp_tui_rect_t area, client;

	if(widget->hidden || !_mp_tui_intersect(clip, &widget->screen, &area))
		return;

	if(widget->type && widget->type->draw) {
		draw->clip = area;
		widget->type->draw(widget, draw);
	}

	if(!widget->first)
		return;
	_mp_tui_client(widget, &client);
	client.x += widget->screen.x;
	client.y += widget->screen.y;
	if(!_mp_tui_intersect(&area, &client, &area))
		return;
	for(child = widget->first; child; child = child->next)
		_mp_tui_draw_tree(child, draw, &area);
}

int mp_tui_step(mp_tui_t *ui) {
	mp_tui_draw_t draw;
	int rows, a, pending;

	if(!ui->display)
		return(0);

	if(ui->splash < 3 && _mp_tui_splash(ui))
		return(1);

	/* the dirty rectangles wait for the screen to wake up */
	if(ui->saver == MP_TUI_OFF)
		return(0);

	if(!ui->drawing) {
		if(ui->dirtyCount == 0)
			return(0);
		ui->current = ui->dirty[0];
		for(a=1; a<ui->dirtyCount; a++)
			ui->dirty[a - 1] = ui->dirty[a];
		ui->dirtyCount--;
		ui->currentRow = ui->current.y;
		ui->drawing = 1;
	}

	rows = (int)(ui->bufferSize / (unsigned int)ui->current.w);
	if(rows > ui->current.y + ui->current.h - ui->currentRow)
		rows = ui->current.y + ui->current.h - ui->currentRow;

	draw.tui = ui;
	draw.pixels = ui->buffer;
	draw.band.x = ui->current.x;
	draw.band.y = ui->currentRow;
	draw.band.w = ui->current.w;
	draw.band.h = rows;
	draw.clip = draw.band;

	mp_tui_draw_fill(&draw, draw.band.x, draw.band.y, draw.band.w, draw.band.h, ui->theme.background);
	_mp_tui_draw_tree(&ui->root, &draw, &draw.band);
	_mp_tui_cursor_draw(ui, &draw);

	ui->display->write(ui->display, draw.band.x, draw.band.y, draw.band.w, draw.band.h, ui->buffer);

	ui->currentRow += rows;
	if(ui->currentRow >= ui->current.y + ui->current.h) {
		ui->drawing = 0;
		/* one scroll position for the whole rectangle, so a glyph is not split across two */
		if(ui->shell && ui->view.pending) {
			pending = ui->view.pending;
			ui->view.pending = 0;
			mp_tui_view_scroll(&ui->view, pending);
		}
	}

	if(mp_tui_pending(ui))
		return(1);
	if(ui->display->flush)
		ui->display->flush(ui->display);
	return(0);
}

void mp_tui_render(mp_tui_t *ui) {
	while(mp_tui_step(ui));
}

void mp_tui_widget_init(mp_tui_widget_t *widget, const mp_tui_class_t *type, mp_tui_widget_t *parent, int x, int y, int w, int h) {
	widget->type = type;
	widget->tui = parent ? parent->tui : NULL;
	widget->parent = parent;
	widget->first = widget->last = NULL;
	widget->prev = widget->next = NULL;
	widget->name = NULL;
	widget->rect.x = x;
	widget->rect.y = y;
	widget->rect.w = w < 0 ? 0 : w;
	widget->rect.h = h < 0 ? 0 : h;
	widget->hidden = 0;
	widget->flags = 0;

	if(parent) {
		widget->prev = parent->last;
		if(parent->last)
			parent->last->next = widget;
		else
			parent->first = widget;
		parent->last = widget;
	}

	_mp_tui_place(widget);
	mp_tui_invalidate(widget);
}

static void _mp_tui_unlink(mp_tui_widget_t *widget) {
	mp_tui_widget_t *parent = widget->parent;

	if(!parent)
		return;
	if(widget->prev)
		widget->prev->next = widget->next;
	else
		parent->first = widget->next;
	if(widget->next)
		widget->next->prev = widget->prev;
	else
		parent->last = widget->prev;
	widget->prev = widget->next = NULL;
}

void mp_tui_widget_remove(mp_tui_widget_t *widget) {
	mp_tui_invalidate(widget);
	while(widget->first)
		mp_tui_widget_remove(widget->first);
	_mp_tui_unlink(widget);
	widget->parent = NULL;
	widget->tui = NULL;
}

void mp_tui_widget_name(mp_tui_widget_t *widget, const char *name) {
	widget->name = name;
}

void mp_tui_invalidate(mp_tui_widget_t *widget) {
	mp_tui_rect_t area;

	if(!widget->tui || widget->hidden)
		return;
	if(_mp_tui_visible(widget, &area))
		mp_tui_invalidate_rect(widget->tui, &area);
}

void mp_tui_move(mp_tui_widget_t *widget, int x, int y) {
	if(widget->rect.x == x && widget->rect.y == y)
		return;
	mp_tui_invalidate(widget);
	widget->rect.x = x;
	widget->rect.y = y;
	_mp_tui_place(widget);
	mp_tui_invalidate(widget);
}

void mp_tui_resize(mp_tui_widget_t *widget, int w, int h) {
	if(widget->rect.w == w && widget->rect.h == h)
		return;
	mp_tui_invalidate(widget);
	widget->rect.w = w < 0 ? 0 : w;
	widget->rect.h = h < 0 ? 0 : h;
	_mp_tui_place(widget);
	mp_tui_invalidate(widget);
}

void mp_tui_show(mp_tui_widget_t *widget) {
	if(!widget->hidden)
		return;
	widget->hidden = 0;
	mp_tui_invalidate(widget);
}

void mp_tui_hide(mp_tui_widget_t *widget) {
	if(widget->hidden)
		return;
	mp_tui_invalidate(widget);
	widget->hidden = 1;
}

void mp_tui_raise(mp_tui_widget_t *widget) {
	mp_tui_widget_t *parent = widget->parent;

	if(!parent || parent->last == widget)
		return;
	_mp_tui_unlink(widget);
	widget->prev = parent->last;
	parent->last->next = widget;
	parent->last = widget;
	mp_tui_invalidate(widget);
}

static mp_tui_widget_t *_mp_tui_find(mp_tui_widget_t *widget, const char *name) {
	mp_tui_widget_t *child, *found;

	if(widget->name && strcmp(widget->name, name) == 0)
		return(widget);
	for(child = widget->first; child; child = child->next) {
		found = _mp_tui_find(child, name);
		if(found)
			return(found);
	}
	return(NULL);
}

mp_tui_widget_t *mp_tui_find(mp_tui_t *ui, const char *name) {
	return(name ? _mp_tui_find(&ui->root, name) : NULL);
}

/* home */

static void _mp_tui_release(mp_tui_t *ui);

static int _mp_tui_line(mp_tui_t *ui) {
	int line = ui->theme.font ? ui->theme.font->lineHeight : 12;

	return(line > 0 ? line : 12);
}

static int _mp_tui_button_top(mp_tui_t *ui) {
	int line = _mp_tui_line(ui);

	return(8 + line + 14);
}

static void _mp_tui_show(mp_tui_t *ui, int index) {
	_mp_tui_release(ui);
	while(ui->view.widget.first)
		mp_tui_widget_remove(ui->view.widget.first);
	ui->view.scroll = 0;
	ui->view.pending = 0;
	ui->app[index].build(&ui->view.widget, ui->app[index].user);
	mp_tui_label_set(&ui->pageTitle, "%s", ui->app[index].name);
	ui->opened = index;
	_mp_tui_place(&ui->view.widget);
	mp_tui_show(&ui->page.widget);
	mp_tui_raise(&ui->page.widget);
}

static void _mp_tui_on_back(mp_tui_button_t *button) {
	mp_tui_t *ui = button->user;

	if(!ui || ui->opened < 0)
		return;
	/* a sub-page returns to the page that opened it */
	if(ui->depth > 1) {
		ui->depth--;
		_mp_tui_show(ui, ui->stack[ui->depth - 1]);
		return;
	}
	_mp_tui_release(ui);
	ui->depth = 0;
	ui->opened = -1;
	mp_tui_hide(&ui->page.widget);
}

static void _mp_tui_open(mp_tui_t *ui, int index) {
	if(index < 0 || index >= ui->appCount || !ui->app[index].build)
		return;

	ui->depth = 0;
	ui->stack[ui->depth++] = index;
	_mp_tui_show(ui, index);
}

static void _mp_tui_on_app(mp_tui_button_t *button) {
	_mp_tui_open(button->widget.tui, button->id);
}

static void _mp_tui_shell(mp_tui_t *ui) {
	int line = _mp_tui_line(ui);
	int width = ui->display->width;
	int height = ui->display->height;
	int header = line + 20;

	/* a finger needs a target taller than a text line */
	if(header < 40)
		header = 40;

	ui->opened = -1;
	ui->focus = 0;
	ui->shell = 1;

	mp_tui_label(&ui->homeTitle, &ui->root, 12, 8, width - 24, line + 2, "miniPhi");
	mp_tui_label_align(&ui->homeTitle, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&ui->homeTitle, ui->theme.accent);

	mp_tui_page(&ui->page, &ui->root, 0, 0, width, height, header);
	mp_tui_label(&ui->pageTitle, &ui->page.widget, 0, (header - line) / 2, width, line, "");
	mp_tui_label_align(&ui->pageTitle, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&ui->pageTitle, ui->theme.titleForeground);

	/* the words only; the whole header stays the hit target */
	mp_tui_button(&ui->back, &ui->page.widget, 10, (header - line) / 2,
		mp_tui_text_width(ui->theme.font, "< Back") + 4, line, "< Back");
	ui->back.plain = 1;
	ui->back.onPress = _mp_tui_on_back;
	ui->back.user = ui;

	mp_tui_view(&ui->view, &ui->page.widget, 0, header, width, height - header);
	/* the title spans the header; the button stays above it so a tap still hits it */
	mp_tui_raise(&ui->back.widget);
	mp_tui_hide(&ui->page.widget);
}

static void _mp_tui_focus(mp_tui_t *ui, int index) {
	if(ui->homeCount <= 0)
		return;
	if(index < 0)
		index = 0;
	if(index >= ui->homeCount)
		index = ui->homeCount - 1;
	if(index == ui->focus)
		return;
	ui->homeButton[ui->focus].selected = 0;
	mp_tui_invalidate(&ui->homeButton[ui->focus].widget);
	ui->focus = index;
	ui->homeButton[index].selected = 1;
	mp_tui_invalidate(&ui->homeButton[index].widget);
}

static int _mp_tui_view_max(mp_tui_view_t *view) {
	mp_tui_widget_t *child;
	int bottom = 0, edge;

	for(child = view->widget.first; child; child = child->next) {
		edge = child->rect.y + child->rect.h;
		if(edge > bottom)
			bottom = edge;
	}
	bottom += 8;
	edge = bottom - view->widget.rect.h;
	return(edge > 0 ? edge : 0);
}

/* the deepest visible widget under the point, skipping one subtree, or NULL */
static mp_tui_widget_t *_mp_tui_pick(mp_tui_widget_t *widget, int x, int y, mp_tui_widget_t *skip) {
	mp_tui_widget_t *child, *found;
	mp_tui_rect_t area;

	if(widget == skip || widget->hidden || !_mp_tui_visible(widget, &area))
		return(NULL);
	if(x < area.x || y < area.y || x >= area.x + area.w || y >= area.y + area.h)
		return(NULL);

	for(child = widget->last; child; child = child->prev) {
		found = _mp_tui_pick(child, x, y, skip);
		if(found)
			return(found);
	}
	return(widget);
}

static int _mp_tui_deliver(mp_tui_widget_t *widget, const mp_tui_event_t *event) {
	for(; widget; widget = widget->parent) {
		if(widget->type && widget->type->event && widget->type->event(widget, event))
			return(1);
	}
	return(0);
}

static void _mp_tui_unpress(mp_tui_t *ui) {
	if(!ui->pressed)
		return;
	ui->pressed->pressed = 0;
	mp_tui_invalidate(&ui->pressed->widget);
	ui->pressed = NULL;
}

/* forgets what the pointer holds, before the widgets it points at go away */
static void _mp_tui_release(mp_tui_t *ui) {
	_mp_tui_unpress(ui);
	ui->tapped = NULL;
	ui->dragged = NULL;
	ui->dragView = NULL;
}

int mp_tui_app(mp_tui_t *ui, const char *name, mp_tui_app_build_t build, void *user) {
	int index, home, line, h, y;
	mp_tui_button_t *button;

	if(!ui->display || !name || !name[0] || !build || ui->appCount >= MP_TUI_APP_MAX || ui->homeCount >= MP_TUI_APP_MAX)
		return(0);
	if(!ui->shell)
		_mp_tui_shell(ui);

	/* a child registered between two home entries must not open a gap on the home */
	index = ui->appCount;
	home = ui->homeCount;
	line = _mp_tui_line(ui);
	h = line + 18;
	y = _mp_tui_button_top(ui) + home * (h + 8);

	ui->app[index].name = name;
	ui->app[index].build = build;
	ui->app[index].user = user;

	button = &ui->homeButton[home];
	mp_tui_button(button, &ui->root, 12, y, ui->display->width - 24, h, name);
	button->onPress = _mp_tui_on_app;
	button->id = index;
	button->chevron = 1;
	button->selected = home == ui->focus;

	ui->appCount++;
	ui->homeCount++;
	mp_tui_raise(&ui->page.widget);
	return(1);
}

int mp_tui_child(mp_tui_t *ui, const char *name, mp_tui_app_build_t build, void *user) {
	int index;

	if(!ui->display || !name || !name[0] || !build || ui->appCount >= MP_TUI_APP_MAX)
		return(0);
	if(!ui->shell)
		_mp_tui_shell(ui);

	index = ui->appCount;
	ui->app[index].name = name;
	ui->app[index].build = build;
	ui->app[index].user = user;
	ui->appCount++;
	return(1);
}

void mp_tui_enter(mp_tui_t *ui, int index) {
	if(!ui || index < 0 || index >= ui->appCount || !ui->app[index].build)
		return;
	if(ui->depth >= MP_TUI_APP_MAX)
		return;
	ui->stack[ui->depth++] = index;
	_mp_tui_show(ui, index);
}

int mp_tui_dispatch(mp_tui_t *ui, const mp_tui_event_t *event) {
	mp_tui_widget_t *target = _mp_tui_pick(&ui->root, event->x, event->y, event->source);

	return(target ? _mp_tui_deliver(target, event) : 0);
}

int mp_tui_press(mp_tui_t *ui, int x, int y) {
	mp_tui_event_t event;

	memset(&event, 0, sizeof(event));
	event.type = MP_TUI_EVENT_CLICK;
	event.x = x;
	event.y = y;
	return(mp_tui_dispatch(ui, &event));
}

void mp_tui_view_scroll(mp_tui_view_t *view, int dy) {
	int next, max;

	if(dy == 0)
		return;
	/* moving children while a band is being sent shears the text already on screen */
	if(view->widget.tui && view->widget.tui->drawing) {
		view->pending += dy;
		return;
	}
	max = _mp_tui_view_max(view);
	next = view->scroll + dy;
	if(next < 0)
		next = 0;
	if(next > max)
		next = max;
	if(next == view->scroll)
		return;
	view->scroll = next;
	_mp_tui_place(&view->widget);
	mp_tui_invalidate(&view->widget);
}

void mp_tui_scroll(mp_tui_t *ui, int dy) {
	if(!ui->shell || ui->opened < 0)
		return;
	mp_tui_view_scroll(&ui->view, dy);
}

void mp_tui_key(mp_tui_t *ui, mp_tui_key_t key) {
	int step;

	if(!ui->shell)
		return;
	if(ui->opened >= 0) {
		if(key == MP_TUI_KEY_BACK) {
			_mp_tui_on_back(&ui->back);
			return;
		}
		step = _mp_tui_line(ui);
		if(key == MP_TUI_KEY_PAGE_UP || key == MP_TUI_KEY_PAGE_DOWN)
			step = ui->view.widget.rect.h - step;
		if(key == MP_TUI_KEY_UP || key == MP_TUI_KEY_PAGE_UP)
			mp_tui_scroll(ui, -step);
		else if(key == MP_TUI_KEY_DOWN || key == MP_TUI_KEY_PAGE_DOWN)
			mp_tui_scroll(ui, step);
		return;
	}
	if(key == MP_TUI_KEY_UP)
		_mp_tui_focus(ui, ui->focus - 1);
	else if(key == MP_TUI_KEY_DOWN)
		_mp_tui_focus(ui, ui->focus + 1);
	else if(key == MP_TUI_KEY_ENTER && ui->homeCount > 0)
		_mp_tui_open(ui, ui->homeButton[ui->focus].id);
}

/* mouse cursor: 'X' outline, '.' fill */

static const char *const _mp_tui_arrow[] = {
	"X",
	"XX",
	"X.X",
	"X..X",
	"X...X",
	"X....X",
	"X.....X",
	"X......X",
	"X.......X",
	"X........X",
	"X.....XXXXX",
	"X..X..X",
	"X.X X..X",
	"XX  X..X",
	"X    X..X",
	"     XXXX",
};

#define _MP_TUI_ARROW_W 11
#define _MP_TUI_ARROW_H ((int)(sizeof(_mp_tui_arrow) / sizeof(_mp_tui_arrow[0])))

static void _mp_tui_cursor_rect(mp_tui_t *ui, mp_tui_rect_t *rect) {
	rect->x = ui->cursorX;
	rect->y = ui->cursorY;
	rect->w = _MP_TUI_ARROW_W;
	rect->h = _MP_TUI_ARROW_H;
}

static void _mp_tui_cursor_draw(mp_tui_t *ui, mp_tui_draw_t *draw) {
	const char *row;
	int line, start, x;
	char c;

	if(!ui->cursorShown)
		return;
	draw->clip = draw->band;
	for(line = 0; line < _MP_TUI_ARROW_H; line++) {
		if(ui->cursorY + line < draw->band.y || ui->cursorY + line >= draw->band.y + draw->band.h)
			continue;
		row = _mp_tui_arrow[line];
		for(x = 0; row[x]; ) {
			c = row[x];
			start = x;
			while(row[x] == c)
				x++;
			if(c == 'X')
				mp_tui_draw_hline(draw, ui->cursorX + start, ui->cursorY + line, x - start, MP_TUI_RGB(0, 0, 0));
			else if(c == '.')
				mp_tui_draw_hline(draw, ui->cursorX + start, ui->cursorY + line, x - start, MP_TUI_RGB(0xFF, 0xFF, 0xFF));
		}
	}
}

static void _mp_tui_cursor_move(mp_tui_t *ui, int x, int y) {
	mp_tui_rect_t rect;

	if(ui->cursorShown && ui->cursorX == x && ui->cursorY == y)
		return;
	if(ui->cursorShown) {
		_mp_tui_cursor_rect(ui, &rect);
		mp_tui_invalidate_rect(ui, &rect);
	}
	ui->cursorShown = 1;
	ui->cursorX = x;
	ui->cursorY = y;
	_mp_tui_cursor_rect(ui, &rect);
	mp_tui_invalidate_rect(ui, &rect);
}

/* inputs */

#ifdef SUPPORT_COMMON_INPUT
static mp_tui_view_t *_mp_tui_view_of(mp_tui_widget_t *widget) {
	for(; widget; widget = widget->parent) {
		if(widget->type == &mp_tui_view_class)
			return((mp_tui_view_t *)widget);
	}
	return(NULL);
}

static void _mp_tui_input_key(mp_tui_t *ui, int key) {
	switch(key) {
		case MP_INPUT_KEY_UP: mp_tui_key(ui, MP_TUI_KEY_UP); break;
		case MP_INPUT_KEY_DOWN:
		case MP_INPUT_KEY_TAB: mp_tui_key(ui, MP_TUI_KEY_DOWN); break;
		case MP_INPUT_KEY_RIGHT:
		case MP_INPUT_KEY_ENTER:
		case ' ': mp_tui_key(ui, MP_TUI_KEY_ENTER); break;
		case MP_INPUT_KEY_LEFT:
		case MP_INPUT_KEY_ESCAPE:
		case MP_INPUT_KEY_BACKSPACE: mp_tui_key(ui, MP_TUI_KEY_BACK); break;
		case MP_INPUT_KEY_PAGE_UP: mp_tui_key(ui, MP_TUI_KEY_PAGE_UP); break;
		case MP_INPUT_KEY_PAGE_DOWN: mp_tui_key(ui, MP_TUI_KEY_PAGE_DOWN); break;
	}
}

/* the words and a margin around them, from the corner: a finger on the bezel reads as 0 */
static int _mp_tui_back_zone(mp_tui_t *ui, int x, int y) {
	mp_tui_rect_t *words = &ui->back.widget.screen;

	return(ui->opened >= 0 && x >= 0 && y >= 0 && x < words->x + words->w + 16 && y < ui->page.header);
}

static void _mp_tui_arm_back(mp_tui_t *ui) {
	if(ui->pressed == &ui->back)
		return;
	_mp_tui_unpress(ui);
	ui->pressed = &ui->back;
	ui->back.pressed = 1;
	mp_tui_invalidate(&ui->back.widget);
}

/* with a little slack for a noisy panel */
static int _mp_tui_within(mp_tui_rect_t *area, int x, int y) {
	return(x >= area->x - 12 && y >= area->y - 12 && x < area->x + area->w + 12 && y < area->y + area->h + 12);
}

/* the finger is still on the button it went down on */
static int _mp_tui_held(mp_tui_t *ui, int x, int y) {
	if(!ui->pressed)
		return(0);
	if(ui->pressed == &ui->back)
		return(_mp_tui_back_zone(ui, x, y));
	return(_mp_tui_within(&ui->pressed->widget.screen, x, y));
}

/* the nearest widget under the finger that takes events itself, outside a view or a draggable */
static mp_tui_widget_t *_mp_tui_tappable(mp_tui_widget_t *target) {
	for(; target; target = target->parent) {
		if(target->flags & MP_TUI_DRAGGABLE)
			return(NULL);
		if(target->type == &mp_tui_view_class)
			return(NULL);
		if(target->type && target->type->event)
			return(target);
	}
	return(NULL);
}

static void _mp_tui_input(mp_tui_t *ui, mp_input_t *input, const mp_input_event_t *in) {
	mp_tui_widget_t *target;
	mp_tui_event_t event;
	int held;

	if(in->type != MP_INPUT_KEY && (input->caps & MP_INPUT_CAP_MOUSE))
		_mp_tui_cursor_move(ui, in->x, in->y);

	memset(&event, 0, sizeof(event));
	event.x = in->x;
	event.y = in->y;

	switch(in->type) {
		case MP_INPUT_DOWN:
			if(_mp_tui_back_zone(ui, in->x, in->y)) {
				_mp_tui_arm_back(ui);
				break;
			}
			_mp_tui_unpress(ui);
			ui->tapped = NULL;
			target = _mp_tui_pick(&ui->root, in->x, in->y, NULL);
			for(; target; target = target->parent) {
				if(target->type == &mp_tui_button_class) {
					ui->pressed = (mp_tui_button_t *)target;
					ui->pressed->pressed = 1;
					mp_tui_invalidate(target);
					break;
				}
			}
			if(!ui->pressed)
				ui->tapped = _mp_tui_tappable(_mp_tui_pick(&ui->root, in->x, in->y, NULL));
			break;

		case MP_INPUT_CLICK:
			if(ui->pressed == &ui->back || _mp_tui_back_zone(ui, in->x, in->y)) {
				_mp_tui_unpress(ui);
				_mp_tui_on_back(&ui->back);
				break;
			}
			_mp_tui_unpress(ui);
			ui->tapped = NULL;
			event.type = MP_TUI_EVENT_CLICK;
			mp_tui_dispatch(ui, &event);
			break;

		case MP_INPUT_DRAG_START:
			/* a finger always moves a little; that stays a press while it is on a button */
			if(ui->pressed || ui->tapped)
				break;
			_mp_tui_release(ui);
			target = _mp_tui_pick(&ui->root, in->x, in->y, NULL);
			for(; target; target = target->parent) {
				if(target->flags & MP_TUI_DRAGGABLE) {
					ui->dragged = target;
					mp_tui_raise(target);
					break;
				}
			}
			if(!ui->dragged)
				ui->dragView = _mp_tui_view_of(_mp_tui_pick(&ui->root, in->x, in->y, NULL));
			break;

		case MP_INPUT_DRAG:
			if(ui->pressed) {
				int dy = in->y - in->startY;
				int travel = dy < 0 ? -dy : dy;
				mp_tui_view_t *view = ui->pressed == &ui->back ? &ui->view : _mp_tui_view_of(&ui->pressed->widget);

				/* a pull on a button is a scroll, not a tap, when the page has more below */
				if(view && travel >= 20 && _mp_tui_view_max(view) > 0) {
					_mp_tui_unpress(ui);
					ui->dragView = view;
					mp_tui_view_scroll(view, -dy);
					break;
				}
			}
			if(ui->tapped) {
				int dy = in->y - in->startY;
				int travel = dy < 0 ? -dy : dy;
				mp_tui_view_t *view = _mp_tui_view_of(ui->tapped);

				if(view && travel >= 20 && _mp_tui_view_max(view) > 0) {
					ui->tapped = NULL;
					ui->dragView = view;
					mp_tui_view_scroll(view, -dy);
				}
				break;
			}
			if(ui->pressed) {
				held = _mp_tui_held(ui, in->x, in->y);
				if(ui->pressed->pressed != held) {
					ui->pressed->pressed = held;
					mp_tui_invalidate(&ui->pressed->widget);
				}
				break;
			}
			if(ui->dragged)
				mp_tui_move(ui->dragged, ui->dragged->rect.x + in->dx, ui->dragged->rect.y + in->dy);
			else if(ui->dragView)
				mp_tui_view_scroll(ui->dragView, -in->dy);
			break;

		case MP_INPUT_DROP:
			/* lifted on Back again: a press that slid away from it is not a tap */
			if(ui->pressed == &ui->back) {
				held = _mp_tui_back_zone(ui, in->x, in->y);
				_mp_tui_unpress(ui);
				if(held)
					_mp_tui_on_back(&ui->back);
				break;
			}
			if(ui->pressed) {
				held = _mp_tui_held(ui, in->x, in->y);
				_mp_tui_unpress(ui);
				if(held) {
					event.type = MP_TUI_EVENT_CLICK;
					mp_tui_dispatch(ui, &event);
				}
				break;
			}
			if(ui->tapped) {
				target = ui->tapped;
				ui->tapped = NULL;
				if(_mp_tui_within(&target->screen, in->x, in->y)) {
					event.type = MP_TUI_EVENT_CLICK;
					_mp_tui_deliver(target, &event);
				}
				break;
			}
			if(ui->dragged) {
				event.type = MP_TUI_EVENT_DROP;
				event.source = ui->dragged;
				mp_tui_dispatch(ui, &event);
			}
			_mp_tui_release(ui);
			break;

		case MP_INPUT_WHEEL:
			event.type = MP_TUI_EVENT_WHEEL;
			event.value = in->value;
			if(!mp_tui_dispatch(ui, &event))
				mp_tui_scroll(ui, -in->value * 3 * _mp_tui_line(ui));
			break;

		case MP_INPUT_KEY:
			_mp_tui_input_key(ui, in->value);
			break;

		default:
			break;
	}
}
#endif

static void _mp_tui_backlight(mp_tui_t *ui, int percent) {
	if(ui->display->backlight)
		ui->display->backlight(ui->display, percent);
}

static void _mp_tui_rest(int on) {
#ifdef SUPPORT_COMMON_DEVICE
	mp_device_t *device;
	int rank;

	for(rank = 0; (device = mp_device_find(MP_DEVICE_ACCELEROMETER, rank)) != NULL; rank++)
		mp_device_set(device, on);
#else
	(void)on;
#endif
}

static void _mp_tui_doze(mp_tui_t *ui, int dozing) {
#ifdef SUPPORT_COMMON_INPUT
	mp_input_t *input = NULL;

	if(!ui->kernel)
		return;
	while((input = mp_input_next(ui->kernel, input, ui)) != NULL)
		mp_input_doze(input, dozing);
#else
	(void)ui;
	(void)dozing;
#endif
}

void mp_tui_wake(mp_tui_t *ui) {
	ui->activeAt = mp_clock_ticks();
	if(ui->saver == MP_TUI_AWAKE || !ui->display)
		return;
	if(ui->saver == MP_TUI_OFF) {
		if(ui->display->power)
			ui->display->power(ui->display, 1);
		_mp_tui_doze(ui, 0);
		_mp_tui_rest(1);
		ui->saver = MP_TUI_AWAKE;
		mp_tui_refresh(ui);
	}
	ui->saver = MP_TUI_AWAKE;
	_mp_tui_backlight(ui, 100);
}

int mp_tui_awake(mp_tui_t *ui) {
	return(ui->saver != MP_TUI_OFF);
}

static unsigned long _mp_tui_saver(mp_tui_t *ui) {
	unsigned long dim = MP_TUI_DIM_AFTER ? _mp_tui_ms((unsigned long)MP_TUI_DIM_AFTER * 1000UL) : 0;
	unsigned long off = MP_TUI_OFF_AFTER ? _mp_tui_ms((unsigned long)MP_TUI_OFF_AFTER * 1000UL) : 0;
	unsigned long idle;

	if(!ui->display || ui->splash < 3 || (!ui->display->backlight && !ui->display->power))
		return(0);
	if(off && dim >= off)
		dim = 0;

	idle = mp_clock_ticks() - ui->activeAt;
	if(ui->saver == MP_TUI_AWAKE && dim && idle >= dim) {
		_mp_tui_backlight(ui, MP_TUI_DIM_LEVEL);
		ui->saver = MP_TUI_DIMMED;
	}
	/* a dark panel can drop the draw enough that a USB port cuts the 5 V */
	if(ui->kernel && mp_power_hibernating(ui->kernel) == YES) {
		if(ui->saver != MP_TUI_OFF && off && idle >= off) {
			_mp_tui_backlight(ui, MP_TUI_OFF_LEVEL);
			if(ui->display->power)
				ui->display->power(ui->display, 0);
			_mp_tui_doze(ui, 1);
			_mp_tui_rest(0);
			ui->saver = MP_TUI_OFF;
		}
	} else if(ui->saver == MP_TUI_OFF) {
		if(ui->display->power)
			ui->display->power(ui->display, 1);
		_mp_tui_doze(ui, 0);
		_mp_tui_rest(1);
		ui->saver = MP_TUI_DIMMED;
		mp_tui_refresh(ui);
		_mp_tui_backlight(ui, MP_TUI_DIM_LEVEL);
	}

	if(ui->saver == MP_TUI_AWAKE && dim)
		return(dim - idle);
	if(ui->saver != MP_TUI_OFF && off && ui->kernel && mp_power_hibernating(ui->kernel) == YES)
		return(off - idle);
	return(0);
}

unsigned long mp_tui_idle(mp_tui_t *ui) {
	unsigned long wait = _mp_tui_saver(ui);
#ifdef SUPPORT_COMMON_INPUT
	mp_input_t *input = NULL;
	unsigned long next;

	if(ui->kernel) {
		while((input = mp_input_next(ui->kernel, input, ui)) != NULL) {
			next = mp_input_waiting(input);
			if(next && (!wait || next < wait))
				wait = next;
		}
	}
#endif
	return(wait);
}

void mp_tui_poll(mp_tui_t *ui) {
#ifdef SUPPORT_COMMON_INPUT
	mp_input_t *input = NULL;
	mp_input_event_t event;

	if(!ui->kernel || !ui->display)
		return;
	while((input = mp_input_next(ui->kernel, input, ui)) != NULL) {
		if(input->width <= 0 || input->height <= 0)
			mp_input_bounds(input, ui->display->width, ui->display->height);
		if(!input->reader)
			input->reader = ui->task;
		while(mp_input_read(input, &event)) {
			/* nothing to act on while the splash is up */
			if(ui->splash < 3)
				continue;

			mp_power_hold_for(ui->kernel, &ui->boost, MP_POWER_HIGH, MP_TUI_BOOST, "TUI");
			/* a dimmed backlight chops, and the panel reads that as the finger moving */
			if(ui->saver == MP_TUI_DIMMED && event.type == MP_INPUT_MOVE)
				continue;
			if(ui->saver == MP_TUI_OFF)
				ui->swallow = 1;
			mp_tui_wake(ui);

			/* the press that woke the screen ends with its release */
			if(ui->swallow) {
				if(event.type != MP_INPUT_DOWN && event.type != MP_INPUT_DRAG_START && event.type != MP_INPUT_DRAG)
					ui->swallow = 0;
				continue;
			}
			_mp_tui_input(ui, input, &event);
		}
	}
#else
	(void)ui;
#endif
}

/**@}*/

#endif
