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
 * On an ILI932x screen the TUI opens on a home of applications. System is the
 * first: a scrollable page with about, version, copyright, license and the
 * live figures. Back returns to the home.
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

	inner = 2;
	mp_tui_window(&sysinfo->screen.status, page, 4, y, width - 8, title + line * 4 + 6, "Status");
	client = MP_TUI_WIDGET(&sysinfo->screen.status);
	_sysinfo_text(&sysinfo->screen.uptime, client, 6, &inner, innerW, line, font, "Uptime");
	mp_tui_label(&sysinfo->screen.tasks, client, 6, inner, 110, line, "Tasks");
	mp_tui_bar(&sysinfo->screen.taskBar, client, 120, inner + 2, width - 140, line > 4 ? line - 4 : line, MP_TASK_MAX);
	inner += line;
	_sysinfo_text(&sysinfo->screen.sensors[0], client, 6, &inner, innerW, line, font, "");
	_sysinfo_text(&sysinfo->screen.sensors[1], client, 6, &inner, innerW, line, font, "");
	_sysinfo_text(&sysinfo->screen.sensors[2], client, 6, &inner, innerW, line, font, "");
	mp_tui_label_color(&sysinfo->screen.sensors[0], tui->theme.muted);
	mp_tui_label_color(&sysinfo->screen.sensors[1], tui->theme.muted);
	mp_tui_label_color(&sysinfo->screen.sensors[2], tui->theme.muted);
	mp_tui_resize(&sysinfo->screen.status.widget, width - 8, title + inner + 4);
	y += sysinfo->screen.status.widget.rect.h + 8;

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

	if(!mp_tui_app(tui, "System", _sysinfo_system, sysinfo)) {
		mp_tui_fini(tui);
		mp_drv_ili932x_fini(&sysinfo->screen.lcd);
		return;
	}

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

	if(!sysinfo->screen.systemReady)
		return;

	mp_tui_label_set(&sysinfo->screen.uptime, "Uptime   %lu:%02lu:%02lu",
		seconds / 3600, (seconds / 60) % 60, seconds % 60);
	mp_tui_label_set(&sysinfo->screen.tasks, "Tasks    %u/%u",
		kernel->tasks.usedNumber, (unsigned int)MP_TASK_MAX);
	mp_tui_bar_set(&sysinfo->screen.taskBar, kernel->tasks.usedNumber);

#ifdef SUPPORT_COMMON_SENSOR
	{
		mp_list_item_t *item;
		mp_sensor_t *sensor;

		for(item = kernel->sensors.list.first; item != NULL && index < 3; item = item->next) {
			sensor = item->user;
			if(sensor->type == MP_SENSOR_TEMPERATURE)
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: %.1f °C", sensor->idName, (double)sensor->temperature.result);
			else if(sensor->type == MP_SENSOR_VOLTAGE)
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: %.3f V", sensor->idName, (double)sensor->voltage.result);
			else if(sensor->type == MP_SENSOR_3AXIS)
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: %.2f %.2f %.2f g", sensor->idName,
					(double)sensor->axis3.x, (double)sensor->axis3.y, (double)sensor->axis3.z);
		}
	}
#endif
	while(index < 3)
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
