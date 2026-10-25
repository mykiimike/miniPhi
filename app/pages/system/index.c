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
 * Home page. The logo, about, version, copyright, license, the flash, uptime, RAM and
 * the MCU sensors, and the buttons that open the sub-pages.
 */

#include "../index.h"

#ifdef SYSINFO_SCREEN

#include <string.h>

/* RAM: static data ends at _end, then the reserved heap; the stack sits at the top */
extern uint32_t _end, _estack;
extern char __heap_size, __stack_size;

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
	unsigned long origin = MP_FLASH_ORIGIN;
	unsigned long end, at;

	memset(flash, 0, sizeof(*flash));
	flash->size = _sysinfo_flash_bytes();
	end = mp_flash_image_end();
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

#ifdef SUPPORT_COMMON_DEVICE
static int _sysinfo_mcu_sensor(mp_kernel_t *kernel, mp_device_registry_t *sensor) {
	mp_device_registry_t *axis = NULL;

	if(sensor->class != MP_DEVICE_TEMPERATURE && sensor->class != MP_DEVICE_VOLTAGE)
		return(0);
	while((axis = mp_device_measure_next(kernel, axis)) != NULL) {
		if(mp_device_measure_axes(axis->class) == 3 && _sysinfo_measure_beside(axis, sensor))
			return(0);
	}
	return(1);
}
#endif

void _sysinfo_system_refresh(sysinfo_t *sysinfo) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	unsigned long hz = mp_clock_tick_hz();
	unsigned long seconds = hz ? mp_clock_ticks() / hz : 0;
	int index = 0;

	if(!sysinfo->screen.systemReady)
		return;

	{
		char state[MP_TUI_TEXT_MAX];

		_sysinfo_flash_state(state, sizeof(state));
		mp_tui_label_set(&sysinfo->screen.flashState, "%s", state);
	}
	{
		unsigned long speed = mp_clock_get_speed();

		mp_tui_label_set(&sysinfo->screen.clock, "Clock  %lu.%02lu MHz, %s", speed / 1000000, (speed / 10000) % 100,
			mp_power_name(kernel->power.level));
	}
	mp_tui_label_set(&sysinfo->screen.uptime, "Uptime   %lu:%02lu:%02lu",
		seconds / 3600, (seconds / 60) % 60, seconds % 60);
	mp_tui_label_set(&sysinfo->screen.tasks, "Tasks    %u/%u",
		kernel->tasks.usedNumber, (unsigned int)MP_TASK_MAX);
	mp_tui_bar_set(&sysinfo->screen.taskBar, kernel->tasks.usedNumber);
	{
		unsigned long total = (unsigned long)PHI_BUILD_RAM;
		unsigned long origin = (unsigned long)&_estack - total;
		unsigned long used = 0;
		unsigned long usedKb, totalKb;

		if((unsigned long)&_end >= origin) {
			used = (unsigned long)&_end - origin;
			used += (unsigned long)&__heap_size + (unsigned long)&__stack_size;
			if(used > total)
				used = total;
		}
		totalKb = total / 1024UL;
		usedKb = (used + 1023UL) / 1024UL;
		if(usedKb > totalKb)
			usedKb = totalKb;
		mp_tui_label_set(&sysinfo->screen.ram, "RAM      %lu/%luK", usedKb, totalKb);
		mp_tui_bar_set(&sysinfo->screen.ramBar, (int)usedKb);
	}
#ifdef SUPPORT_COMMON_MEM
	{
		mp_mem_state_t state;

		mp_mem_state(&state);
		mp_tui_label_set(&sysinfo->screen.pool, "Pool     %d/%d", state.used, state.total);
		mp_tui_bar_set(&sysinfo->screen.poolBar, state.used);
	}
#endif

#ifdef SUPPORT_COMMON_DEVICE
	{
		mp_device_registry_t *sensor = NULL;
		float v[3];

		while(index < 2 && (sensor = mp_device_measure_next(kernel, sensor)) != NULL) {
			if(!_sysinfo_mcu_sensor(kernel, sensor))
				continue;
			if(mp_device_measure_read(sensor->instance, v, NULL) != TRUE)
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: --", sensor->address);
			else if(sensor->class == MP_DEVICE_TEMPERATURE)
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: %.1f °C", sensor->address, (double)v[0]);
			else
				mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s: %.3f V", sensor->address, (double)v[0]);
		}
	}
#endif
	while(index < 2)
		mp_tui_label_set(&sysinfo->screen.sensors[index++], "%s", "");
}

static void _sysinfo_on_tasks(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;

	if(!sysinfo || sysinfo->screen.tasksIndex < 0)
		return;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.tasksIndex);
}

static void _sysinfo_on_devices(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;

	if(!sysinfo || sysinfo->screen.devicesIndex < 0)
		return;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.devicesIndex);
}

#ifdef SUPPORT_COMMON_USB
static void _sysinfo_on_usb(mp_tui_button_t *button) {
	sysinfo_t *sysinfo = button->user;

	if(!sysinfo || sysinfo->screen.usbIndex < 0)
		return;
	mp_tui_enter(&sysinfo->screen.tui, sysinfo->screen.usbIndex);
}
#endif

static void _sysinfo_logo_draw(mp_tui_widget_t *widget, mp_tui_draw_t *draw) {
	mp_tui_draw_bitmap(draw, &mp_tui_logo, widget->screen.x, widget->screen.y, widget->screen.w, widget->screen.h);
}

static const mp_tui_class_t _sysinfo_logo_class = { "logo", _sysinfo_logo_draw, NULL, NULL };

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
	int side;
	char text[MP_TUI_TEXT_MAX];
	mp_tui_widget_t *client;

	side = mp_tui_logo.w;
	if(side > width - 16)
		side = width - 16;
	mp_tui_widget_init(&sysinfo->screen.logo, &_sysinfo_logo_class, page, (width - side) / 2, y, side, side);
	y += side + 4;

	mp_tui_label(&sysinfo->screen.heading, page, 0, y, width, line, "");
	mp_tui_label_set(&sysinfo->screen.heading, "miniPhi %s", kernel->version);
	mp_tui_label_align(&sysinfo->screen.heading, MP_TUI_ALIGN_CENTER);
	mp_tui_label_color(&sysinfo->screen.heading, tui->theme.accent);
	y += line + 4;

	inner = 2;
	mp_tui_window(&sysinfo->screen.system, page, 4, y, width - 8, title + inner + 4, "System");
	client = MP_TUI_WIDGET(&sysinfo->screen.system);
	snprintf(text, sizeof(text), "Board  %s, app %s", PHI_BUILD_BOARD, PHI_BUILD_APP);
	mp_tui_label_wrap(&sysinfo->screen.board, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "CPU    %s", kernel->mcuName ? kernel->mcuName : "unknown");
	mp_tui_label_wrap(&sysinfo->screen.cpu, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Clock  %lu.%02lu MHz", speed / 1000000, (speed / 10000) % 100);
	mp_tui_label_wrap(&sysinfo->screen.clock, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Memory %lu KB flash, %lu KB RAM", PHI_BUILD_FLASH / 1024, PHI_BUILD_RAM / 1024);
	mp_tui_label_wrap(&sysinfo->screen.memory, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Build  %s %s", __DATE__, __TIME__);
	mp_tui_label_wrap(&sysinfo->screen.build, client, 6, &inner, innerW, line, font, text);
	snprintf(text, sizeof(text), "Target %s / %s / %s", PHI_BUILD_ARCH, PHI_BUILD_SERIES, PHI_BUILD_MCU);
	mp_tui_label_wrap(&sysinfo->screen.target, client, 6, &inner, innerW, line, font, text);
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
		mp_tui_label_wrap(&sysinfo->screen.flashSize, client, 6, &inner, innerW, line, font, text);
		snprintf(text, sizeof(text), "Used     %lu / %lu KB", used, total);
		mp_tui_label_wrap(&sysinfo->screen.flashUsed, client, 6, &inner, innerW, line, font, text);
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
		mp_tui_label_wrap(&sysinfo->screen.flashFree, client, 6, &inner, innerW, line, font, text);
		_sysinfo_flash_state(state, sizeof(state));
		mp_tui_label_wrap(&sysinfo->screen.flashState, client, 6, &inner, innerW, line, font, state);
#ifdef SUPPORT_COMMON_KFLASH
		{
			mp_kflash_state_t store;

			mp_kflash_state(kernel, &store);
			if(store.bank < 0)
				snprintf(text, sizeof(text), "Store    unavailable");
			else
				snprintf(text, sizeof(text), "Store    %d key%s, %u/%u B at 0x%08lX",
					store.keys, store.keys == 1 ? "" : "s", store.used, store.size, store.base);
			mp_tui_label_wrap(&sysinfo->screen.flashStore, client, 6, &inner, innerW, line, font, text);
		}
#endif
		mp_tui_resize(&sysinfo->screen.flash.widget, width - 8, title + inner + 4);
		y += sysinfo->screen.flash.widget.rect.h + 4;
	}

	inner = 2;
	mp_tui_window(&sysinfo->screen.status, page, 4, y, width - 8, title + line * 4 + 6, "Status");
	client = MP_TUI_WIDGET(&sysinfo->screen.status);
	mp_tui_label_wrap(&sysinfo->screen.uptime, client, 6, &inner, innerW, line, font, "Uptime");
	mp_tui_label(&sysinfo->screen.tasks, client, 6, inner, 110, line, "Tasks");
	mp_tui_bar(&sysinfo->screen.taskBar, client, 120, inner + 2, width - 140, line > 4 ? line - 4 : line, MP_TASK_MAX);
	inner += line;
	mp_tui_label(&sysinfo->screen.ram, client, 6, inner, 110, line, "RAM");
	mp_tui_bar(&sysinfo->screen.ramBar, client, 120, inner + 2, width - 140, line > 4 ? line - 4 : line,
		PHI_BUILD_RAM / 1024 > 0 ? (int)(PHI_BUILD_RAM / 1024) : 1);
	inner += line;
#ifdef SUPPORT_COMMON_MEM
	{
		mp_mem_state_t state;

		mp_mem_state(&state);
		mp_tui_label(&sysinfo->screen.pool, client, 6, inner, 110, line, "Pool");
		mp_tui_bar(&sysinfo->screen.poolBar, client, 120, inner + 2, width - 140, line > 4 ? line - 4 : line,
			state.total > 0 ? state.total : 1);
		inner += line;
	}
#endif
	mp_tui_label_wrap(&sysinfo->screen.sensors[0], client, 6, &inner, innerW, line, font, "");
	mp_tui_label_wrap(&sysinfo->screen.sensors[1], client, 6, &inner, innerW, line, font, "");
	mp_tui_label_color(&sysinfo->screen.sensors[0], tui->theme.muted);
	mp_tui_label_color(&sysinfo->screen.sensors[1], tui->theme.muted);
	mp_tui_resize(&sysinfo->screen.status.widget, width - 8, title + inner + 4);
	y += sysinfo->screen.status.widget.rect.h + 8;

	mp_tui_button(&sysinfo->screen.taskManager, page, 12, y, width - 24, line + 18, "Task manager");
	sysinfo->screen.taskManager.chevron = 1;
	sysinfo->screen.taskManager.onPress = _sysinfo_on_tasks;
	sysinfo->screen.taskManager.user = sysinfo;
	y += line + 18 + 6;

	mp_tui_button(&sysinfo->screen.devicesButton, page, 12, y, width - 24, line + 18, "Devices");
	sysinfo->screen.devicesButton.chevron = 1;
	sysinfo->screen.devicesButton.onPress = _sysinfo_on_devices;
	sysinfo->screen.devicesButton.user = sysinfo;
	y += line + 18 + 6;

#ifdef SUPPORT_COMMON_USB
	mp_tui_button(&sysinfo->screen.usbButton, page, 12, y, width - 24, line + 18, "USB");
	sysinfo->screen.usbButton.chevron = 1;
	sysinfo->screen.usbButton.onPress = _sysinfo_on_usb;
	sysinfo->screen.usbButton.user = sysinfo;
	y += line + 18 + 6;
#endif
	y += 2;

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
	_sysinfo_system_refresh(sysinfo);
}

int _sysinfo_system_open(sysinfo_t *sysinfo) {
	return(mp_tui_app(&sysinfo->screen.tui, "System", _sysinfo_system, sysinfo) ? 1 : 0);
}

#endif
