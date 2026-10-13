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

/*
 * On an ILI932x screen the TUI opens on a home of applications. Accelerometer
 * is first: whichever 3-axis sensor is present, as numbers and as a bubble
 * level. System is last: a scrollable page with about, version, copyright,
 * license, the flash, uptime and the MCU sensors. Task manager opens from System and
 * lists the kernel tasks. Back steps out one page.
 */

#include "sysinfo.h"

#include <stdio.h>
#include <string.h>

#ifdef SYSINFO_SCREEN

/* one display row is enough; more rows mean fewer, longer writes */
#define SYSINFO_SCREEN_ROWS 8

static mp_tui_color_t _sysinfo_pixels[320 * SYSINFO_SCREEN_ROWS];

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

static void _sysinfo_text(mp_tui_label_t *label, mp_tui_widget_t *parent, int x, int *y, int w, int line, const mp_tui_font_t *font, const char *text) {
	char wrapped[MP_TUI_TEXT_MAX];
	int rows;

	snprintf(wrapped, sizeof(wrapped), "%s", text);
	rows = _sysinfo_wrap(wrapped, font, w);
	mp_tui_label(label, parent, x, *y, w, rows * line, wrapped);
	*y += rows * line;
}

static void _sysinfo_screen_refresh(sysinfo_t *sysinfo);
static MP_TASK(_sysinfo_screen);

#ifdef SUPPORT_COMMON_SENSOR
/* a temperature published beside an accelerometer, such as "SMB380 Temp" */
static int _sysinfo_accel_companion(mp_sensor_t *axis, mp_sensor_t *sensor) {
	int length = (int)strlen(axis->idName);

	if(sensor->type != MP_SENSOR_TEMPERATURE || length <= 0)
		return(0);
	if(strncmp(sensor->idName, axis->idName, (size_t)length) != 0)
		return(0);
	return(sensor->idName[length] == '\0' || sensor->idName[length] == ' ');
}

static mp_sensor_t *_sysinfo_accel_find(mp_kernel_t *kernel) {
	mp_list_item_t *item;
	mp_sensor_t *sensor;

	for(item = kernel->sensors.list.first; item != NULL; item = item->next) {
		sensor = item->user;
		if(sensor->type == MP_SENSOR_3AXIS)
			return(sensor);
	}
	return(NULL);
}

static mp_sensor_t *_sysinfo_accel_temperature(mp_kernel_t *kernel, mp_sensor_t *axis) {
	mp_list_item_t *item;
	mp_sensor_t *sensor;

	if(!axis)
		return(NULL);
	for(item = kernel->sensors.list.first; item != NULL; item = item->next) {
		sensor = item->user;
		if(_sysinfo_accel_companion(axis, sensor))
			return(sensor);
	}
	return(NULL);
}

static int _sysinfo_mcu_sensor(mp_kernel_t *kernel, mp_sensor_t *sensor) {
	mp_list_item_t *item;
	mp_sensor_t *axis;

	if(sensor->type != MP_SENSOR_TEMPERATURE && sensor->type != MP_SENSOR_VOLTAGE)
		return(0);
	for(item = kernel->sensors.list.first; item != NULL; item = item->next) {
		axis = item->user;
		if(axis->type == MP_SENSOR_3AXIS && _sysinfo_accel_companion(axis, sensor))
			return(0);
	}
	return(1);
}

static int _sysinfo_isqrt(int n) {
	int root = 0;
	int bit = 1 << 14;

	if(n <= 0)
		return(0);
	while(bit > n)
		bit >>= 2;
	while(bit) {
		if(root + bit <= n) {
			n -= root + bit;
			root = (root >> 1) + bit;
		}
		else
			root >>= 1;
		bit >>= 2;
	}
	return(root);
}

static void _sysinfo_disc(mp_tui_draw_t *draw, int cx, int cy, int radius, mp_tui_color_t color) {
	int dy, span;

	for(dy = -radius; dy <= radius; dy++) {
		span = _sysinfo_isqrt(radius * radius - dy * dy);
		mp_tui_draw_hline(draw, cx - span, cy + dy, span * 2 + 1, color);
	}
}

static void _sysinfo_ring(mp_tui_draw_t *draw, int cx, int cy, int radius, mp_tui_color_t color) {
	int dy, span;

	if(radius < 1)
		return;
	for(dy = -radius; dy <= radius; dy++) {
		span = _sysinfo_isqrt(radius * radius - dy * dy);
		mp_tui_draw_hline(draw, cx - span, cy + dy, 2, color);
		if(span > 1)
			mp_tui_draw_hline(draw, cx + span - 1, cy + dy, 2, color);
	}
}

/* the glass is ±1 g; the bubble is where X (right) and Y (up) pull */
static void _sysinfo_level_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	sysinfo_level_t *level = (sysinfo_level_t *)widget;
	mp_tui_theme_t *theme = &widget->tui->theme;
	int x = widget->screen.x;
	int y = widget->screen.y;
	int w = widget->screen.w;
	int cx = x + w / 2;
	int cy = y + w / 2;
	int radius = w / 2 - 4;
	int travel, px, py, bx, by;

	if(radius < 8)
		return;
	travel = radius - 8;
	px = level->mx;
	py = level->my;
	if(px > 100)
		px = 100;
	if(px < -100)
		px = -100;
	if(py > 100)
		py = 100;
	if(py < -100)
		py = -100;
	bx = cx + px * travel / 100;
	by = cy - py * travel / 100;

	mp_tui_draw_fill(draw, x, y, w, widget->screen.h, theme->surface);
	_sysinfo_disc(draw, cx, cy, radius, theme->background);
	_sysinfo_ring(draw, cx, cy, radius, theme->border);
	_sysinfo_ring(draw, cx, cy, travel, theme->muted);
	mp_tui_draw_hline(draw, cx - radius, cy, radius * 2 + 1, theme->foreground);
	mp_tui_draw_vline(draw, cx, cy - radius, radius * 2 + 1, theme->foreground);
	mp_tui_draw_text(draw, theme->font, cx + travel - 8, cy - theme->font->lineHeight - 1, "X", theme->muted);
	mp_tui_draw_text(draw, theme->font, cx + 4, cy - travel, "Y", theme->muted);
	_sysinfo_disc(draw, bx, by, 6, theme->accent);
	{
		const char *scale = "+/- 1 g";
		int tw = mp_tui_text_width(theme->font, scale);

		mp_tui_draw_text(draw, theme->font, x + (w - tw) / 2, y + w + 2, scale, theme->muted);
	}
}

static const mp_tui_class_t _sysinfo_level_class = { "level", _sysinfo_level_draw, NULL, NULL };

static void _sysinfo_accel_values(sysinfo_t *sysinfo);

static int _sysinfo_cg(float g) {
	int v;

	if(g >= 0.0f)
		v = (int)(g * 100.0f + 0.5f);
	else
		v = (int)(g * 100.0f - 0.5f);
	if(v > 400)
		v = 400;
	if(v < -400)
		v = -400;
	return(v);
}

static void _sysinfo_accel_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	mp_tui_t *tui = page->tui;
	const mp_tui_font_t *font = tui->theme.font;
	int line = font->lineHeight + 1;
	int side = page->rect.h - 8 - line;
	int x, y, w;

	if(side > page->rect.w - 128)
		side = page->rect.w - 128;
	if(side > 168)
		side = 168;
	if(side < 96)
		side = 96;

	sysinfo->screen.level.mx = 0;
	sysinfo->screen.level.my = 0;
	sysinfo->screen.level.mz = 0;
	mp_tui_widget_init(&sysinfo->screen.level.widget, &_sysinfo_level_class, page, 4, 4, side, side + line + 2);

	x = side + 16;
	y = 8;
	w = page->rect.w - x - 8;
	_sysinfo_text(&sysinfo->screen.accelName, page, x, &y, w, line, font, "");
	mp_tui_label_color(&sysinfo->screen.accelName, tui->theme.accent);
	y += 4;
	_sysinfo_text(&sysinfo->screen.accelX, page, x, &y, w, line, font, "X");
	_sysinfo_text(&sysinfo->screen.accelY, page, x, &y, w, line, font, "Y");
	_sysinfo_text(&sysinfo->screen.accelZ, page, x, &y, w, line, font, "Z");
	y += 4;
	_sysinfo_text(&sysinfo->screen.accelTemperature, page, x, &y, w, line, font, "");
	mp_tui_label_color(&sysinfo->screen.accelTemperature, tui->theme.muted);

	sysinfo->screen.accelReady = 1;
	_sysinfo_accel_values(sysinfo);
}

static void _sysinfo_accel_values(sysinfo_t *sysinfo) {
	mp_sensor_t *axis = _sysinfo_accel_find(&sysinfo->kernel);
	mp_sensor_t *temperature = _sysinfo_accel_temperature(&sysinfo->kernel, axis);
	sysinfo_level_t *level = &sysinfo->screen.level;
	int mx, my, mz;

	if(!axis) {
		mp_tui_label_set(&sysinfo->screen.accelName, "%s", "No accelerometer");
		mp_tui_label_set(&sysinfo->screen.accelX, "%s", "X  --");
		mp_tui_label_set(&sysinfo->screen.accelY, "%s", "Y  --");
		mp_tui_label_set(&sysinfo->screen.accelZ, "%s", "Z  --");
		mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s", "");
		if(level->mx || level->my || level->mz) {
			level->mx = 0;
			level->my = 0;
			level->mz = 0;
			mp_tui_invalidate(&level->widget);
		}
		return;
	}

	mp_tui_label_set(&sysinfo->screen.accelName, "%s", axis->idName);
	mp_tui_label_set(&sysinfo->screen.accelX, "X  % .2f g", (double)axis->axis3.x);
	mp_tui_label_set(&sysinfo->screen.accelY, "Y  % .2f g", (double)axis->axis3.y);
	mp_tui_label_set(&sysinfo->screen.accelZ, "Z  % .2f g", (double)axis->axis3.z);
	if(temperature)
		mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s  %.1f °C",
			temperature->idName, (double)temperature->temperature.result);
	else
		mp_tui_label_set(&sysinfo->screen.accelTemperature, "%s", "");

	mx = _sysinfo_cg(axis->axis3.x);
	my = _sysinfo_cg(axis->axis3.y);
	mz = _sysinfo_cg(axis->axis3.z);
	level->mz = mz;
	if(mx == level->mx && my == level->my)
		return;
	level->mx = mx;
	level->my = my;
	mp_tui_invalidate(&level->widget);
}
#endif

static const char *_sysinfo_task_state(mp_task_signal_t signal) {
	switch(signal) {
		case MP_TASK_SIG_SLEEP: return("sleep");
		case MP_TASK_SIG_PENDING: return("pending");
		case MP_TASK_SIG_STOP: return("stop");
		case MP_TASK_SIG_DEAD: return("dead");
		default: return("ready");
	}
}

static void _sysinfo_on_tasks(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;

	if(!sysinfo || sysinfo->screen.tasksIndex < 0)
		return;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.tasksIndex);
}

static const mp_tui_column_t _sysinfo_task_columns[] = {
	{ "Task", 0, MP_TUI_ALIGN_LEFT },
	{ "State", 78, MP_TUI_ALIGN_LEFT },
	{ "Period", 72, MP_TUI_ALIGN_RIGHT },
};

static void _sysinfo_tasks_values(sysinfo_t *sysinfo) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	mp_list_item_t *item;
	mp_task_t *task;
	unsigned long hz = mp_clock_tick_hz();
	int n = 0;
	int rows = (int)kernel->tasks.usedNumber;
	char period[16];

	if(rows > MP_TASK_MAX)
		rows = MP_TASK_MAX;
	mp_tui_table_rows(&sysinfo->screen.taskTable, rows);
	mp_tui_label_set(&sysinfo->screen.taskCount, "%u / %u",
		kernel->tasks.usedNumber, (unsigned int)MP_TASK_MAX);

	for(item = kernel->tasks.usedList.first; item != NULL && n < rows; item = item->next, n++) {
		task = item->user;
		if(!task->delay || !hz)
			snprintf(period, sizeof(period), "tick");
		else
			snprintf(period, sizeof(period), "%lu ms", (task->delay * 1000UL) / hz);
		mp_tui_table_set(&sysinfo->screen.taskTable, n, 0, "%s", task->name ? task->name : "(unnamed)");
		mp_tui_table_set(&sysinfo->screen.taskTable, n, 1, "%s", _sysinfo_task_state(task->signal));
		mp_tui_table_set(&sysinfo->screen.taskTable, n, 2, "%s", period);
	}
}

static void _sysinfo_tasks_page(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	const mp_tui_font_t *font = page->tui->theme.font;
	int line = font->lineHeight + 1;
	int width = page->rect.w - 16;

	mp_tui_label(&sysinfo->screen.taskCount, page, 8, 4, width, line, "");
	mp_tui_label_color(&sysinfo->screen.taskCount, page->tui->theme.accent);
	mp_tui_table(&sysinfo->screen.taskTable, page, 8, 4 + line + 4, width,
		_sysinfo_task_columns, 3, &sysinfo->screen.taskCell[0][0], MP_TASK_MAX);
	sysinfo->screen.tasksReady = 1;
	_sysinfo_tasks_values(sysinfo);
}

/* the image is linked at the start of the internal flash; .data is stored after the code */
extern uint32_t _sidata, _sdata, _edata;

#define SYSINFO_FLASH_ORIGIN 0x08000000UL

typedef struct {
	unsigned long size;
	unsigned long blank;
	unsigned long written;
	unsigned long freeAt;
} sysinfo_flash_t;

static unsigned long _sysinfo_flash_bytes(void) {
	unsigned long kb = FLASH_SIZE_KB;

	/* the factory value is the real size; a blank or wild read falls back to the build */
	if(kb < 16 || kb > 1024)
		return((unsigned long)PHI_BUILD_FLASH);
	return(kb * 1024UL);
}

static int _sysinfo_page_blank(const uint32_t *page) {
	unsigned long words = MP_FLASH_PAGE_SIZE / sizeof(uint32_t);
	unsigned long i;

	for(i = 0; i < words; i++)
		if(page[i] != 0xFFFFFFFFUL)
			return(0);
	return(1);
}

/* pages after the image: blank ones are the persistent space, the rest already hold data */
static void _sysinfo_flash_measure(sysinfo_flash_t *flash) {
	unsigned long page = MP_FLASH_PAGE_SIZE;
	unsigned long origin = SYSINFO_FLASH_ORIGIN;
	unsigned long end, at;

	memset(flash, 0, sizeof(*flash));
	flash->size = _sysinfo_flash_bytes();
	end = (unsigned long)&_sidata + ((unsigned long)&_edata - (unsigned long)&_sdata);
	if(end < origin)
		end = origin;
	if(end > origin + flash->size)
		end = origin + flash->size;

	at = (end + page - 1UL) & ~(page - 1UL);
	flash->freeAt = at;
	while(at + page <= origin + flash->size) {
		if(_sysinfo_page_blank((const uint32_t *)at))
			flash->blank += page;
		else
			flash->written += page;
		at += page;
	}
}

static void _sysinfo_flash_state(char *text, size_t size) {
	unsigned long sr = FLASH->SR;
	const char *lock;
	const char *guard;

	if(sr & FLASH_SR_BSY)
		lock = "busy";
	else if(FLASH->CR & FLASH_CR_LOCK)
		lock = "locked";
	else
		lock = "unlocked";

	if(FLASH->OBR & FLASH_OBR_RDPRT)
		guard = "read protected";
	else if(FLASH->WRPR != 0xFFFFFFFFUL)
		guard = "write protected";
	else
		guard = "open";

	if(sr & FLASH_SR_PGERR)
		snprintf(text, size, "State    %s, program error", lock);
	else if(sr & FLASH_SR_WRPRTERR)
		snprintf(text, size, "State    %s, protect error", lock);
	else
		snprintf(text, size, "State    %s, %s", lock, guard);
}

static void _sysinfo_system(mp_tui_widget_t *page, void *user) {
	sysinfo_t *sysinfo = user;
	mp_tui_t *tui = page->tui;
	mp_kernel_t *kernel = &sysinfo->kernel;
	const mp_tui_font_t *font = tui->theme.font;
	unsigned long speed = mp_clock_get_speed();
	int line = font->lineHeight + 1;
	int title = font->lineHeight + 4;
	int width = page->rect.w;
	int innerW = width - 22;
	int y = 4;
	int inner;
	char text[MP_TUI_TEXT_MAX];
	mp_tui_widget_t *client;

	mp_tui_label(&sysinfo->screen.heading, page, 0, y, width, line, "");
	mp_tui_label_set(&sysinfo->screen.heading, "miniPhi %s - %s", kernel->version, PHI_BUILD_BOARD);
	mp_tui_label_align(&sysinfo->screen.heading, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.heading, tui->theme.accent);
	y += line + 4;

	inner = 2;
	mp_tui_window(&sysinfo->screen.system, page, 4, y, width - 8, title + inner + 4, "System");
	client = MP_TUI_WIDGET(&sysinfo->screen.system);
	snprintf(text, sizeof(text), "Board  %s, app %s", PHI_BUILD_BOARD, PHI_BUILD_APP);
	_sysinfo_text(&sysinfo->screen.board, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "CPU    %s", kernel->mcuName ? kernel->mcuName : "unknown");
	_sysinfo_text(&sysinfo->screen.cpu, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Clock  %lu.%02lu MHz", speed / 1000000, (speed / 10000) % 100);
	_sysinfo_text(&sysinfo->screen.clock, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Memory %lu KB flash, %lu KB RAM", PHI_BUILD_FLASH / 1024, PHI_BUILD_RAM / 1024);
	_sysinfo_text(&sysinfo->screen.memory, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Build  %s %s", __DATE__, __TIME__);
	_sysinfo_text(&sysinfo->screen.build, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Target %s / %s / %s", PHI_BUILD_ARCH, PHI_BUILD_SERIES, PHI_BUILD_MCU);
	_sysinfo_text(&sysinfo->screen.target, client, 6, &inner, innerW, line, font, text);
	mp_tui_resize(&sysinfo->screen.system.widget, width - 8, title + inner + 4);
	y += sysinfo->screen.system.widget.rect.h + 4;

	{
		sysinfo_flash_t info;
		unsigned long total, used;
		char state[MP_TUI_TEXT_MAX];
		int barH = line > 4 ? line - 4 : line;

		_sysinfo_flash_measure(&info);
		total = info.size / 1024UL;
		used = (info.size - info.blank) / 1024UL;
		if(used > total)
			used = total;

		inner = 2;
		mp_tui_window(&sysinfo->screen.flash, page, 4, y, width - 8, title + inner + 4, "Flash");
		client = MP_TUI_WIDGET(&sysinfo->screen.flash);
		if(info.size != (unsigned long)PHI_BUILD_FLASH)
			snprintf(text, sizeof(text), "Size     %lu KB chip, build %lu KB, page %lu KB",
				info.size / 1024UL, (unsigned long)PHI_BUILD_FLASH / 1024UL,
				(unsigned long)MP_FLASH_PAGE_SIZE / 1024UL);
		else
			snprintf(text, sizeof(text), "Size     %lu KB, page %lu KB",
				info.size / 1024UL, (unsigned long)MP_FLASH_PAGE_SIZE / 1024UL);
		_sysinfo_text(&sysinfo->screen.flashSize, client, 6, &inner, innerW, line, font, text);
		snprintf(text, sizeof(text), "Used     %lu / %lu KB", used, total);
		_sysinfo_text(&sysinfo->screen.flashUsed, client, 6, &inner, innerW, line, font, text);
		mp_tui_bar(&sysinfo->screen.flashBar, client, 6, inner + 2, innerW, barH, total > 0 ? (int)total : 1);
		mp_tui_bar_set(&sysinfo->screen.flashBar, (int)used);
		inner += line;
		if(info.written)
			snprintf(text, sizeof(text), "Free     %lu KB blank, %lu KB written",
				info.blank / 1024UL, info.written / 1024UL);
		else if(info.blank)
			snprintf(text, sizeof(text), "Free     %lu KB blank at 0x%08lX",
				info.blank / 1024UL, info.freeAt);
		else
			snprintf(text, sizeof(text), "Free     none");
		_sysinfo_text(&sysinfo->screen.flashFree, client, 6, &inner, innerW, line, font, text);
		_sysinfo_flash_state(state, sizeof(state));
		_sysinfo_text(&sysinfo->screen.flashState, client, 6, &inner, innerW, line, font, state);
		mp_tui_resize(&sysinfo->screen.flash.widget, width - 8, title + inner + 4);
		y += sysinfo->screen.flash.widget.rect.h + 4;
	}

	inner = 2;
	mp_tui_window(&sysinfo->screen.status, page, 4, y, width - 8, title + line * 4 + 6, "Status");
	client = MP_TUI_WIDGET(&sysinfo->screen.status);
	_sysinfo_text(&sysinfo->screen.uptime, client, 6, &inner, innerW, line, font, "Uptime");
	mp_tui_label(&sysinfo->screen.tasks, client, 6, inner, 110, line, "Tasks");
	mp_tui_bar(&sysinfo->screen.taskBar, client, 120, inner + 2, width - 140, line > 4 ? line - 4 : line, MP_TASK_MAX);
	inner += line;
	_sysinfo_text(&sysinfo->screen.sensors[0], client, 6, &inner, innerW, line, font, "");
	_sysinfo_text(&sysinfo->screen.sensors[1], client, 6, &inner, innerW, line, font, "");
	mp_tui_label_color(&sysinfo->screen.sensors[0], tui->theme.muted);
	mp_tui_label_color(&sysinfo->screen.sensors[1], tui->theme.muted);
	mp_tui_resize(&sysinfo->screen.status.widget, width - 8, title + inner + 4);
	y += sysinfo->screen.status.widget.rect.h + 8;

	mp_tui_button(&sysinfo->screen.taskManager, page, 12, y, width - 24, line + 18, "Task manager");
	sysinfo->screen.taskManager.chevron = 1;
	sysinfo->screen.taskManager.onPress = _sysinfo_on_tasks;
	sysinfo->screen.taskManager.user = sysinfo;
	y += line + 18 + 8;

	/* under the panels when the page is full, on the bottom edge when it fits */
	if(y + line * 2 < page->rect.h)
		y = page->rect.h - line * 2 - 4;
	mp_tui_label(&sysinfo->screen.copyright, page, 0, y, width, line, "(c) Michael Vergoz");
	mp_tui_label_align(&sysinfo->screen.copyright, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.copyright, tui->theme.muted);
	y += line;
	mp_tui_label(&sysinfo->screen.license, page, 0, y, width, line, "GNU GPL v3 or later");
	mp_tui_label_align(&sysinfo->screen.license, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.license, tui->theme.muted);

	sysinfo->screen.systemReady = 1;
	_sysinfo_screen_refresh(sysinfo);
}

#ifdef SUPPORT_COMMON_INPUT
/* the console is a keyboard for the TUI; j, k and b stand for down, up and back */
static void _sysinfo_console_key(sysinfo_t *sysinfo, unsigned char c) {
	mp_input_t *keys = &sysinfo->screen.keys;

	if(c == 'j' || c == 'J')
		mp_input_key(keys, MP_INPUT_KEY_DOWN);
	else if(c == 'k' || c == 'K')
		mp_input_key(keys, MP_INPUT_KEY_UP);
	else if(c == 'b' || c == 'B')
		mp_input_key(keys, MP_INPUT_KEY_ESCAPE);
	else
		mp_input_terminal(keys, c);
}
#endif

static void _sysinfo_console_read(sysinfo_t *sysinfo) {
#ifdef SUPPORT_COMMON_INPUT
	mp_circular_t *cir;
	mp_circular_buffer_t *buffer;
	int index;

	if(sysinfo->console == NO)
		return;
	cir = &sysinfo->serial.rxCir;
	while(cir->totalSize > 0) {
		buffer = mp_circular_read(cir);
		if(!buffer || buffer->size == 0) {
			if(buffer)
				mp_mem_free(&sysinfo->kernel, buffer);
			return;
		}
		for(index = 0; index < buffer->size; index++)
			_sysinfo_console_key(sysinfo, buffer->data[index]);
		mp_mem_free(&sysinfo->kernel, buffer);
	}
#else
	(void)sysinfo;
#endif
}

void _sysinfo_screen_open(sysinfo_t *sysinfo) {
	mp_options_t options[] = PHI_ILI932X_LCD;
	mp_kernel_t *kernel = &sysinfo->kernel;
	mp_tui_t *tui = &sysinfo->screen.tui;

	if(mp_drv_ili932x_init(kernel, &sysinfo->screen.lcd, options, "LCD") == FALSE)
		return;
	if(!mp_tui_init(tui, &sysinfo->screen.lcd.display, _sysinfo_pixels, sizeof(_sysinfo_pixels) / sizeof(_sysinfo_pixels[0]))) {
		mp_drv_ili932x_fini(&sysinfo->screen.lcd);
		return;
	}

#ifdef SUPPORT_COMMON_SENSOR
	sysinfo->screen.accelIndex = -1;
	if(mp_tui_app(tui, "Accelerometer", _sysinfo_accel_page, sysinfo))
		sysinfo->screen.accelIndex = tui->appCount - 1;
#endif
	if(!mp_tui_app(tui, "System", _sysinfo_system, sysinfo)) {
		mp_tui_fini(tui);
		mp_drv_ili932x_fini(&sysinfo->screen.lcd);
		return;
	}
	sysinfo->screen.tasksIndex = -1;
	if(mp_tui_child(tui, "Task manager", _sysinfo_tasks_page, sysinfo))
		sysinfo->screen.tasksIndex = tui->appCount - 1;

#ifdef SUPPORT_COMMON_INPUT
	if(sysinfo->console == YES)
		mp_input_init(kernel, &sysinfo->screen.keys, MP_INPUT_CAP_KEYS, "Console");
#endif
#if defined(SUPPORT_DRV_TOUCH_RESISTIVE) && defined(PHI_RESISTIVE_TOUCH)
	{
		mp_options_t touch[] = PHI_RESISTIVE_TOUCH;
		sysinfo->screen.touchReady = mp_drv_resistive_init(kernel, &sysinfo->screen.touch, touch, "Touch") == TRUE;
	}
#endif

	sysinfo->screen.ready = YES;
	mp_tui_attach(kernel, tui, "TUI");
	sysinfo->screen.task = mp_task_create(&kernel->tasks, "Screen", _sysinfo_screen, sysinfo, _sysinfo_ticks(50));
}

void _sysinfo_screen_close(sysinfo_t *sysinfo) {
	if(sysinfo->screen.ready == NO)
		return;
	if(sysinfo->screen.task) {
		mp_task_destroy(sysinfo->screen.task);
		sysinfo->screen.task = NULL;
	}
#ifdef SUPPORT_COMMON_INPUT
	mp_input_fini(&sysinfo->screen.keys);
#endif
#if defined(SUPPORT_DRV_TOUCH_RESISTIVE) && defined(PHI_RESISTIVE_TOUCH)
	if(sysinfo->screen.touchReady == YES) {
		mp_drv_resistive_fini(&sysinfo->screen.touch);
		sysinfo->screen.touchReady = NO;
	}
#endif
	mp_tui_fini(&sysinfo->screen.tui);
	mp_drv_ili932x_fini(&sysinfo->screen.lcd);
	sysinfo->screen.ready = NO;
}

static void _sysinfo_screen_refresh(sysinfo_t *sysinfo) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	unsigned long hz = mp_clock_tick_hz();
	unsigned long seconds = hz ? mp_clock_ticks() / hz : 0;
	int index = 0;

#ifdef SUPPORT_COMMON_SENSOR
	/* System's widgets do not exist until that page is opened, and must not gate this one */
	if(sysinfo->screen.accelReady && sysinfo->screen.tui.opened == sysinfo->screen.accelIndex)
		_sysinfo_accel_values(sysinfo);
#endif
	if(sysinfo->screen.tasksReady && sysinfo->screen.tui.opened == sysinfo->screen.tasksIndex)
		_sysinfo_tasks_values(sysinfo);

	if(!sysinfo->screen.systemReady)
		return;

	{
		char state[MP_TUI_TEXT_MAX];

		_sysinfo_flash_state(state, sizeof(state));
		mp_tui_label_set(&sysinfo->screen.flashState, "%s", state);
	}
	mp_tui_label_set(&sysinfo->screen.uptime, "Uptime   %lu:%02lu:%02lu",
		seconds / 3600, (seconds / 60) % 60, seconds % 60);
	mp_tui_label_set(&sysinfo->screen.tasks, "Tasks    %u/%u",
		kernel->tasks.usedNumber, (unsigned int)MP_TASK_MAX);
	mp_tui_bar_set(&sysinfo->screen.taskBar, kernel->tasks.usedNumber);

#ifdef SUPPORT_COMMON_SENSOR
	{
		mp_list_item_t *item;
		mp_sensor_t *sensor;

		for(item = kernel->sensors.list.first; item != NULL && index < 2; item = item->next) {
			sensor = item->user;
			if(!_sysinfo_mcu_sensor(kernel, sensor))
				continue;
			if(sensor->type == MP_SENSOR_TEMPERATURE)
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: %.1f °C", sensor->idName, (double)sensor->temperature.result);
			else
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: %.3f V", sensor->idName, (double)sensor->voltage.result);
		}
	}
#endif
	while(index < 2)
		mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s", "");
}

static MP_TASK(_sysinfo_screen) {
	sysinfo_t *sysinfo = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	_sysinfo_console_read(sysinfo);
	_sysinfo_screen_refresh(sysinfo);
}

#else

void _sysinfo_screen_open(sysinfo_t *sysinfo) {
	(void)sysinfo;
}

void _sysinfo_screen_close(sysinfo_t *sysinfo) {
	(void)sysinfo;
}

#endif
