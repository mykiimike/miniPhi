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

static const char *_mp_pui_leaf(const char *path) {
	const char *slash = strrchr(path, '/');

	return(slash ? slash + 1 : path);
}

static mp_pui_page_t *_mp_pui_by_path(mp_pui_t *ui, const char *path) {
	mp_list_item_t *item;

	for(item = ui->pages.first; item; item = item->next) {
		mp_pui_page_t *page = item->user;

		if(page->path && strcmp(page->path, path) == 0)
			return(page);
	}
	return(NULL);
}

static int _mp_pui_linked(mp_pui_t *ui, mp_pui_page_t *page) {
	mp_list_item_t *item;

	for(item = ui->pages.first; item; item = item->next)
		if(item == &page->item)
			return(1);
	return(0);
}

static int _mp_pui_path_ok(const char *path) {
	const char *cursor;
	int length = 0;

	if(!path || !path[0])
		return(0);
	if(path[0] == '/')
		return(0);
	for(cursor = path; *cursor; cursor++) {
		length++;
		if(*cursor == '/' && (cursor[1] == '/' || cursor[1] == 0))
			return(0);
	}
	return(length > 0 && length < MP_PUI_PATH * 4);
}

static void _mp_pui_open(mp_pui_t *ui, mp_pui_page_t *page);

static mp_pui_page_t *_mp_pui_new_folder(mp_pui_t *ui, const char *path, mp_pui_page_t *parent) {
	mp_pui_page_t *page;
	int index;

	for(index = 0; index < MP_PUI_FOLDER_MAX; index++)
		if(!ui->folderUsed[index])
			break;
	if(index == MP_PUI_FOLDER_MAX)
		return(NULL);
	page = &ui->folder[index];
	memset(page, 0, sizeof(*page));
	strncpy(page->storage, path, sizeof(page->storage) - 1);
	page->storage[sizeof(page->storage) - 1] = 0;
	page->path = page->storage;
	page->name = _mp_pui_leaf(page->storage);
	page->parent = parent;
	page->ui = ui;
	page->synthetic = 1;
	mp_list_init(&page->widgets);
	mp_list_add_last(&ui->pages, &page->item, page);
	ui->folderUsed[index] = 1;
	return(page);
}

static void _mp_pui_release_folder(mp_pui_t *ui, mp_pui_page_t *page) {
	int index;

	if(!page->synthetic)
		return;
	for(index = 0; index < MP_PUI_FOLDER_MAX; index++) {
		if(&ui->folder[index] == page)
			ui->folderUsed[index] = 0;
	}
}

/* parents of path, created when a component has no page yet */
static mp_pui_page_t *_mp_pui_ensure_parents(mp_pui_t *ui, const char *path) {
	char scratch[MP_PUI_PATH];
	mp_pui_page_t *parent = NULL;
	int index, start;

	start = 0;
	for(index = 0; path[index]; index++) {
		mp_pui_page_t *found;

		if(path[index] != '/')
			continue;
		if(index - start <= 0 || index >= MP_PUI_PATH)
			return(NULL);
		memcpy(scratch, path, (size_t)index);
		scratch[index] = 0;
		found = _mp_pui_by_path(ui, scratch);
		if(!found)
			found = _mp_pui_new_folder(ui, scratch, parent);
		if(!found)
			return(NULL);
		parent = found;
		start = index + 1;
	}
	if(start == 0)
		return(NULL);
	if(path[start] == 0)
		return(NULL);
	return(parent);
}

static int _mp_pui_child_count(mp_pui_page_t *parent) {
	mp_list_item_t *item;
	int count = 0;

	if(!parent || !parent->ui)
		return(0);
	for(item = parent->ui->pages.first; item; item = item->next) {
		mp_pui_page_t *page = item->user;

		if(!page->hidden && page->parent == parent)
			count++;
	}
	return(count);
}

static mp_pui_page_t *_mp_pui_child_at(mp_pui_page_t *parent, int index) {
	mp_list_item_t *item;
	int count = 0;

	if(!parent || !parent->ui || index < 0)
		return(NULL);
	for(item = parent->ui->pages.first; item; item = item->next) {
		mp_pui_page_t *page = item->user;

		if(page->hidden || page->parent != parent)
			continue;
		if(count == index)
			return(page);
		count++;
	}
	return(NULL);
}

static mp_pui_page_t *_mp_pui_home(mp_pui_t *ui) {
	mp_list_item_t *item;

	for(item = ui->pages.first; item; item = item->next) {
		mp_pui_page_t *page = item->user;

		if(!page->hidden && !page->parent)
			return(page);
	}
	return(NULL);
}

static int _mp_pui_widgets_bottom(mp_pui_page_t *page) {
	mp_list_item_t *item;
	int bottom = 0;

	for(item = page->widgets.first; item; item = item->next) {
		mp_pui_widget_t *widget = item->user;

		if(widget->y + widget->h > bottom)
			bottom = widget->y + widget->h;
	}
	return(bottom);
}

static int _mp_pui_list_slots(mp_pui_page_t *page) {
	mp_list_item_t *item;
	int slots = 0;

	for(item = page->widgets.first; item; item = item->next)
		slots += mp_pui_widget_rows(item->user);
	return(slots);
}

static void _mp_pui_sync_focus(mp_pui_t *ui) {
	mp_list_item_t *item;
	int slot;

	if(!ui->current)
		return;
	slot = ui->focus;
	for(item = ui->current->widgets.first; item; item = item->next) {
		mp_pui_widget_t *widget = item->user;
		int rows = mp_pui_widget_rows(widget);

		if(rows <= 0)
			continue;
		if(slot >= 0 && slot < rows)
			mp_pui_widget_focus(widget, slot);
		else
			mp_pui_widget_focus(widget, -1);
		slot -= rows;
	}
}

static void _mp_pui_clamp(mp_pui_t *ui) {
	int height, bottom, maxScroll;

	if(!ui->current || !ui->display)
		return;
	height = ui->display->height;
	bottom = _mp_pui_widgets_bottom(ui->current) + _mp_pui_child_count(ui->current) * _mp_pui_line();
	maxScroll = bottom - height;
	if(maxScroll < 0)
		maxScroll = 0;
	if(ui->scroll < 0)
		ui->scroll = 0;
	if(ui->scroll > maxScroll)
		ui->scroll = maxScroll;
	ui->current->scroll = ui->scroll;
	/* a pager has no selection: no row is ever highlighted */
	ui->focus = -1;
	ui->current->focus = -1;
	_mp_pui_sync_focus(ui);
}

static void _mp_pui_open(mp_pui_t *ui, mp_pui_page_t *page) {
	if(!page)
		return;
	/* the Glass page sweeps the backlight; every other page starts at full */
	mp_pui_page_glass_release(ui);
	if(ui->current && ui->current != page) {
		ui->current->scroll = ui->scroll;
		ui->current->focus = ui->focus;
	}
	ui->current = page;
	ui->scroll = page->scroll;
	ui->focus = page->focus;
	mp_list_init(&page->widgets);
	if(page->build)
		page->build(page, page->user);
	_mp_pui_clamp(ui);
	ui->dirty0 = 0;
	ui->dirty1 = ui->display ? ui->display->height - 1 : 0;
	ui->row = 0;
	ui->pending = 1;
	/* the dwell starts once this paint has landed, not while it is in flight */
	ui->rotateAt = 0;
	if(ui->task)
		mp_task_wake(ui->task);
}

static int _mp_pui_under(mp_pui_page_t *page, mp_pui_page_t *root) {
	while(page) {
		if(page == root)
			return(1);
		page = page->parent;
	}
	return(0);
}

static mp_pui_page_t *_mp_pui_fallback(mp_pui_t *ui, mp_pui_page_t *page) {
	mp_list_item_t *item;
	mp_pui_page_t *prev = NULL;

	if(page->parent)
		return(page->parent);
	for(item = ui->pages.first; item; item = item->next) {
		mp_pui_page_t *other = item->user;

		if(other == page)
			break;
		if(!other->hidden && !other->parent)
			prev = other;
	}
	if(prev)
		return(prev);
	return(_mp_pui_home(ui) == page ? NULL : _mp_pui_home(ui));
}

static void _mp_pui_rotate(mp_pui_t *ui) {
	mp_pui_page_t *home = _mp_pui_home(ui);
	mp_pui_page_t *next = NULL;
	int index, count;

	if(!home)
		return;
	if(!ui->current || ui->current == home || ui->current->parent != home)
		next = _mp_pui_child_at(home, 0);
	else {
		count = _mp_pui_child_count(home);
		for(index = 0; index < count; index++) {
			if(_mp_pui_child_at(home, index) == ui->current) {
				next = _mp_pui_child_at(home, index + 1);
				break;
			}
		}
	}
	if(!next)
		next = home;
	if(next != ui->current)
		_mp_pui_open(ui, next);
	else
		mp_pui_refresh(ui);
}

/* the pager turns the other way: home -> last -> ... -> first -> home */
static void _mp_pui_rotate_prev(mp_pui_t *ui) {
	mp_pui_page_t *home = _mp_pui_home(ui);
	mp_pui_page_t *prev = NULL;
	int index, count;

	if(!home)
		return;
	count = _mp_pui_child_count(home);
	if(!ui->current || ui->current == home || ui->current->parent != home)
		prev = _mp_pui_child_at(home, count - 1);
	else {
		for(index = 0; index < count; index++) {
			if(_mp_pui_child_at(home, index) == ui->current) {
				prev = (index > 0) ? _mp_pui_child_at(home, index - 1) : home;
				break;
			}
		}
	}
	if(!prev)
		prev = home;
	if(prev != ui->current)
		_mp_pui_open(ui, prev);
	else
		mp_pui_refresh(ui);
}

int mp_pui_color_lit(mp_pui_color_t color) {
	unsigned int red = (unsigned int)((color >> 16) & 0xFFu);
	unsigned int green = (unsigned int)((color >> 8) & 0xFFu);
	unsigned int blue = (unsigned int)(color & 0xFFu);

	return(((red * 299u + green * 587u + blue * 114u) >> 10) > 0u);
}

int mp_pui_stride(int width, int bpp) {
	if(width <= 0)
		return(0);
	if(bpp == 1)
		return((width + 7) / 8);
	return(width * (bpp / 8));
}

static void _mp_pui_rgb(mp_pui_color_t color, unsigned int *red, unsigned int *green, unsigned int *blue) {
	*red = (unsigned int)((color >> 16) & 0xFFu);
	*green = (unsigned int)((color >> 8) & 0xFFu);
	*blue = (unsigned int)(color & 0xFFu);
}

void mp_pui_put(unsigned char *buffer, int bufW, int bpp, int x, int y, mp_pui_color_t color) {
	int stride, index;
	unsigned int red, green, blue, packed;

	if(!buffer || x < 0 || y < 0 || x >= bufW)
		return;
	stride = mp_pui_stride(bufW, bpp);
	if(stride <= 0)
		return;
	if(bpp == 1) {
		index = y * stride + (x >> 3);
		if(mp_pui_color_lit(color))
			buffer[index] |= (unsigned char)(0x80 >> (x & 7));
		else
			buffer[index] &= (unsigned char)~(0x80 >> (x & 7));
		return;
	}
	_mp_pui_rgb(color, &red, &green, &blue);
	if(bpp == 16) {
		packed = ((red & 0xF8u) << 8) | ((green & 0xFCu) << 3) | (blue >> 3);
		index = (y * bufW + x) * 2;
		buffer[index] = (unsigned char)(packed & 0xFFu);
		buffer[index + 1] = (unsigned char)((packed >> 8) & 0xFFu);
		return;
	}
	if(bpp == 24) {
		index = (y * bufW + x) * 3;
		buffer[index] = (unsigned char)red;
		buffer[index + 1] = (unsigned char)green;
		buffer[index + 2] = (unsigned char)blue;
	}
}

mp_pui_color_t mp_pui_get(const unsigned char *buffer, int bufW, int bpp, int x, int y) {
	int stride, index;
	unsigned int red, green, blue, packed;

	if(!buffer || x < 0 || y < 0 || x >= bufW)
		return(MP_PUI_COLOR_OFF);
	stride = mp_pui_stride(bufW, bpp);
	if(stride <= 0)
		return(MP_PUI_COLOR_OFF);
	if(bpp == 1) {
		index = y * stride + (x >> 3);
		if(buffer[index] & (unsigned char)(0x80 >> (x & 7)))
			return(MP_PUI_COLOR_ON);
		return(MP_PUI_COLOR_OFF);
	}
	if(bpp == 16) {
		index = (y * bufW + x) * 2;
		packed = (unsigned int)buffer[index] | ((unsigned int)buffer[index + 1] << 8);
		red = (packed >> 11) & 0x1Fu;
		green = (packed >> 5) & 0x3Fu;
		blue = packed & 0x1Fu;
		red = (red << 3) | (red >> 2);
		green = (green << 2) | (green >> 4);
		blue = (blue << 3) | (blue >> 2);
		return((mp_pui_color_t)((red << 16) | (green << 8) | blue));
	}
	if(bpp == 24) {
		index = (y * bufW + x) * 3;
		return((mp_pui_color_t)(((unsigned int)buffer[index] << 16) | ((unsigned int)buffer[index + 1] << 8) | buffer[index + 2]));
	}
	return(MP_PUI_COLOR_OFF);
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

static void _mp_pui_draw_row(mp_pui_t *ui, int y) {
	mp_pui_page_t *page = ui->current;
	mp_list_item_t *item;
	int width = ui->display->width;
	int content = y + ui->scroll;
	int bottom, line, childIndex, lineRow, slotsBefore;
	mp_pui_page_t *child;

	if(page->paint) {
		page->paint(page, ui->buffer, width, ui->display->bpp, content);
		return;
	}
	for(item = page->widgets.first; item; item = item->next) {
		mp_pui_widget_t *widget = item->user;
		int local = content - widget->y;

		if(local < 0 || local >= widget->h)
			continue;
		mp_pui_widget_draw(widget, ui->buffer, width, ui->display->bpp, local);
	}
	bottom = _mp_pui_widgets_bottom(page);
	if(content >= bottom) {
		line = _mp_pui_line();
		childIndex = (content - bottom) / line;
		lineRow = (content - bottom) % line;
		child = _mp_pui_child_at(page, childIndex);
		if(child && child->name) {
			mp_pui_paint_text(ui->buffer, width, ui->display->bpp, 2, width, 0, lineRow, &mp_pui_font_default, child->name, MP_PUI_COLOR_ON);
			slotsBefore = _mp_pui_list_slots(page);
			if(ui->focus == slotsBefore + childIndex)
				_mp_pui_invert(ui->buffer, width, ui->display->bpp, 0, width);
		}
	}
	if(page->decorate)
		page->decorate(page, ui->buffer, width, ui->display->bpp, content);
}

int mp_pui_init(mp_pui_t *ui, mp_pui_display_t *display, unsigned char *buffer, unsigned int bufferSize) {
	int stride;

	memset(ui, 0, sizeof(*ui));
	if(!display || !display->write || !buffer || display->width <= 0 || display->height <= 0)
		return(0);
	if(display->bpp != 1 && display->bpp != 16 && display->bpp != 24)
		return(0);
	stride = mp_pui_stride(display->width, display->bpp);
	if(stride <= 0 || bufferSize < (unsigned int)stride)
		return(0);
	ui->display = display;
	ui->buffer = buffer;
	ui->bufferSize = bufferSize;
	mp_list_init(&ui->pages);
	/* 0 until the opening paint lands: the dwell starts then, not at init */
	ui->rotateAt = 0;
	return(1);
}

void mp_pui_fini(mp_pui_t *ui) {
	if(!ui)
		return;
	mp_pui_detach(ui);
	ui->current = NULL;
	ui->pending = 0;
	mp_list_init(&ui->pages);
	ui->display = NULL;
}

mp_ret_t mp_pui_page_register(mp_pui_t *ui, mp_pui_page_t *page, const char *path, mp_pui_build_t build, void *user) {
	mp_pui_page_t *parent = NULL;
	int slash;

	if(!ui || !page || !_mp_pui_path_ok(path))
		return(FALSE);
	if(_mp_pui_by_path(ui, path) || _mp_pui_linked(ui, page))
		return(FALSE);
	slash = strchr(path, '/') != NULL;
	if(slash) {
		parent = _mp_pui_ensure_parents(ui, path);
		if(!parent)
			return(FALSE);
	}
	memset(page, 0, sizeof(*page));
	page->path = path;
	page->name = _mp_pui_leaf(path);
	if(!page->name[0])
		return(FALSE);
	page->build = build;
	page->user = user;
	page->parent = parent;
	page->ui = ui;
	mp_list_init(&page->widgets);
	mp_list_add_last(&ui->pages, &page->item, page);
	if(!ui->current)
		_mp_pui_open(ui, page);
	return(TRUE);
}

void mp_pui_page_unregister(mp_pui_t *ui, mp_pui_page_t *page) {
	int active;
	mp_pui_page_t *fallback;
	mp_pui_page_t *child;

	if(!ui || !page || page->ui != ui)
		return;
	active = ui->current && _mp_pui_under(ui->current, page);
	fallback = active ? _mp_pui_fallback(ui, page) : NULL;
	for(;;) {
		mp_list_item_t *item;

		child = NULL;
		for(item = ui->pages.first; item; item = item->next) {
			mp_pui_page_t *other = item->user;

			if(other->parent == page) {
				child = other;
				break;
			}
		}
		if(!child)
			break;
		mp_pui_page_unregister(ui, child);
	}
	if(page->ui != ui)
		return;
	mp_list_remove(&ui->pages, &page->item);
	_mp_pui_release_folder(ui, page);
	page->ui = NULL;
	if(!active)
		return;
	if(fallback && fallback->ui == ui && fallback != page)
		_mp_pui_open(ui, fallback);
	else {
		ui->current = _mp_pui_home(ui);
		if(ui->current)
			_mp_pui_open(ui, ui->current);
		else {
			ui->pending = 0;
		}
	}
}

#if MP_CPUS > 1
static mp_cpu_mutex_t _mp_pui_mutex = MP_CPU_MUTEX_INIT;

int mp_pui_lock(void) {
	return((int)mp_task_mutex_lock(&_mp_pui_mutex));
}

void mp_pui_unlock(int key) {
	mp_task_mutex_unlock(&_mp_pui_mutex, (mp_task_lock_t)key);
}
#else
int mp_pui_lock(void) {
	return(0);
}

void mp_pui_unlock(int key) {
	(void)key;
}
#endif

static void _mp_pui_page_show(mp_pui_page_t *page, int on);

void mp_pui_page_show(mp_pui_page_t *page, int on) {
	int _mp_pui_key = mp_pui_lock();
	_mp_pui_page_show(page, on);
	mp_pui_unlock(_mp_pui_key);
}

static void _mp_pui_page_show(mp_pui_page_t *page, int on) {
	if(!page)
		return;
	page->hidden = on ? 0 : 1;
	if(!page->ui)
		return;
	if(page->hidden && page->ui->current == page) {
		mp_pui_page_t *fallback = _mp_pui_fallback(page->ui, page);

		if(fallback && fallback != page)
			_mp_pui_open(page->ui, fallback);
		else
			page->ui->current = NULL;
		return;
	}
	if(page->ui->current == page)
		mp_pui_refresh(page->ui);
}

void mp_pui_page_refresh(mp_pui_page_t *page) {
	if(!page || !page->ui)
		return;
	int _mp_pui_key = mp_pui_lock();
	if(page->ui->current == page)
		mp_pui_refresh(page->ui);
	mp_pui_unlock(_mp_pui_key);
}

static void _mp_pui_apply_key(mp_pui_t *ui, mp_pui_key_t key) {
	unsigned long hz;

	if(!ui || !ui->current)
		return;
	ui->keyed = 1;
	ui->rotateAt = mp_clock_ticks();
	/* a key press parks the tour on the new page for the hold window */
	hz = mp_clock_tick_hz();
	ui->holdUntil = hz ? mp_clock_ticks() + (hz * (unsigned long)MP_PUI_HOLD_MS) / 1000UL : 0;
	if(key == MP_PUI_KEY_NEXT)
		_mp_pui_rotate(ui);
	else if(key == MP_PUI_KEY_PREV)
		_mp_pui_rotate_prev(ui);
}

void mp_pui_key(mp_pui_t *ui, mp_pui_key_t key) {
	mp_task_lock_t lock;
	unsigned char next;

	if(!ui)
		return;
	lock = mp_task_lock(MP_TASK_CEILING_KERNEL);
	next = (unsigned char)((ui->keyHead + 1) % MP_PUI_KEY_QUEUE);
	if(next != ui->keyTail)
		ui->keyQueue[ui->keyHead] = key, ui->keyHead = next;
	mp_task_unlock(lock);
	if(ui->task)
		mp_task_wake(ui->task);
}

void mp_pui_pump_keys(mp_pui_t *ui) {
	mp_task_lock_t lock;

	if(!ui)
		return;
	for(;;) {
		mp_pui_key_t key;

		lock = mp_task_lock(MP_TASK_CEILING_KERNEL);
		if(ui->keyHead == ui->keyTail) {
			mp_task_unlock(lock);
			return;
		}
		key = ui->keyQueue[ui->keyTail];
		ui->keyTail = (unsigned char)((ui->keyTail + 1) % MP_PUI_KEY_QUEUE);
		mp_task_unlock(lock);
		_mp_pui_apply_key(ui, key);
	}
}

int mp_pui_step(mp_pui_t *ui) {
	int y, width, stride;

	if(!ui || !ui->pending || !ui->current || !ui->display)
		return(0);
	y = ui->row;
	width = ui->display->width;
	stride = mp_pui_stride(width, ui->display->bpp);
	if(stride < 0)
		stride = 0;
	if((unsigned int)stride > ui->bufferSize)
		stride = (int)ui->bufferSize;
	/* zero bytes are MP_PUI_COLOR_OFF at every bpp */
	memset(ui->buffer, 0, (size_t)stride);
	_mp_pui_draw_row(ui, y);
	ui->display->write(ui->display, 0, y, width, 1, ui->buffer);
	if(((y & 7) == 7 || y == ui->dirty1) && ui->display->flush)
		ui->display->flush(ui->display);
	ui->row++;
	if(ui->row > ui->dirty1) {
		ui->pending = 0;
		return(0);
	}
	return(1);
}

int mp_pui_pending(mp_pui_t *ui) {
	return(ui && ui->pending);
}

void mp_pui_refresh(mp_pui_t *ui) {
	if(!ui || !ui->display)
		return;
	ui->dirty0 = 0;
	ui->dirty1 = ui->display->height - 1;
	ui->row = 0;
	ui->pending = 1;
	if(ui->task)
		mp_task_wake(ui->task);
}

void mp_pui_refresh_rows(mp_pui_t *ui, int y0, int y1) {
	int height;

	if(!ui || !ui->display)
		return;
	height = ui->display->height;
	if(y0 < 0)
		y0 = 0;
	if(y1 > height - 1)
		y1 = height - 1;
	if(y0 > y1)
		return;
	if(!ui->pending) {
		ui->dirty0 = y0;
		ui->dirty1 = y1;
		ui->row = y0;
		ui->pending = 1;
	}
	else {
		if(y0 < ui->dirty0) {
			ui->dirty0 = y0;
			ui->row = y0;
		}
		if(y1 > ui->dirty1)
			ui->dirty1 = y1;
	}
	if(ui->task)
		mp_task_wake(ui->task);
}

unsigned long mp_pui_idle(mp_pui_t *ui) {
	unsigned long hz, period, now, elapsed, remain;

	if(!ui || !ui->current)
		return(0);
	if(ui->pending)
		return(1);
	hz = mp_clock_tick_hz();
	if(hz == 0)
		return(1);
	now = mp_clock_ticks();
	/* a key press parks the page: wait out the hold before the tour resumes */
	if(ui->holdUntil && now < ui->holdUntil) {
		remain = ui->holdUntil - now;
		return(remain ? remain : 1);
	}
	/* hold just expired: restart the dwell from here */
	if(ui->holdUntil) {
		ui->holdUntil = 0;
		ui->rotateAt = now;
	}
	period = (hz * (unsigned long)MP_PUI_ROTATE_MS) / 1000UL;
	if(period == 0)
		period = 1;
	/* a fresh page: the opening paint cleared pending, so the dwell starts now */
	if(!ui->rotateAt) {
		ui->rotateAt = now;
		return(period);
	}
	elapsed = now - ui->rotateAt;
	if(elapsed >= period) {
		_mp_pui_rotate(ui);
		/* open cleared the stamp so the new page's dwell starts after its paint.
		 * The same page refreshing keeps the cadence from this instant. */
		if(ui->rotateAt)
			ui->rotateAt = now;
		return(1);
	}
	return(period - elapsed);
}

#endif
