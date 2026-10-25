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
 * Opens the first display of the board, then the pages in pages/. The console
 * is a keyboard for the TUI; j, k and b stand for down, up and back. The task
 * refreshes the live figures every 250 ms, 50 ms on an accelerometer,
 * 20 ms while a camera picture is on screen, and every second while the screen is off.
 * A byte on the console wakes it.
 */

#include "pages/index.h"

#ifdef SYSINFO_SCREEN

/* four rows: eight no longer fit once the Lepton frame is in RAM */
#define SYSINFO_SCREEN_ROWS 4

static mp_tui_color_t _sysinfo_pixels[320 * SYSINFO_SCREEN_ROWS];

static MP_TASK(_sysinfo_screen);

#ifdef SUPPORT_COMMON_DEVICE
static void _sysinfo_console_key(sysinfo_t *sysinfo, unsigned char c) {
	mp_device_input_t *keys = &sysinfo->screen.keys;

	if(c == 'j' || c == 'J')
		mp_device_input_key(keys, MP_DEVICE_INPUT_KEY_DOWN);
	else if(c == 'k' || c == 'K')
		mp_device_input_key(keys, MP_DEVICE_INPUT_KEY_UP);
	else if(c == 'b' || c == 'B')
		mp_device_input_key(keys, MP_DEVICE_INPUT_KEY_ESCAPE);
	else
		mp_device_input_terminal(keys, c);
}
#endif

static void _sysinfo_console_read(sysinfo_t *sysinfo) {
#ifdef SUPPORT_COMMON_DEVICE
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

/* every registered touch panel feeds the TUI; the first subscriber starts it */
static void _sysinfo_touch_run(sysinfo_t *sysinfo, mp_bool_t on) {
#ifdef SUPPORT_COMMON_DEVICE
	mp_device_registry_t *device;
	int index;

	if(on == NO) {
		while(sysinfo->screen.touchCount > 0)
			mp_device_subscribe_end(&sysinfo->screen.touchSub[--sysinfo->screen.touchCount]);
		return;
	}
	index = 0;
	while(sysinfo->screen.touchCount < 2 &&
		(device = mp_device_registry_find(&sysinfo->kernel, MP_DEVICE_TOUCH, index)) != NULL) {
		index++;
		if(mp_device_touch(device, &sysinfo->screen.touchSub[sysinfo->screen.touchCount]) != NULL)
			sysinfo->screen.touchCount++;
	}
#else
	(void)sysinfo;
	(void)on;
#endif
}

/* each board device is loaded once so its driver names it; a camera stays loaded */
static void _sysinfo_devices_load(sysinfo_t *sysinfo) {
	int n;

	for(n = 0; n < mp_device_count(); n++) {
		mp_device_t *device = mp_device_at(n);

		if(!device || device->opened == YES)
			continue;
		if(mp_device_open(&sysinfo->kernel, device) == FALSE)
			continue;
		if(device->driver->type != MP_DEVICE_CAMERA)
			mp_device_close(device);
	}
}

/* shutdown closes the cameras loaded at open */
static void _sysinfo_camera_run(sysinfo_t *sysinfo, mp_bool_t on) {
	mp_device_t *device;
	int rank;

	for(rank = 0; (device = mp_device_find(MP_DEVICE_CAMERA, rank)) != NULL; rank++) {
		if(on == YES)
			mp_device_open(&sysinfo->kernel, device);
		else
			mp_device_close(device);
	}
}

void _sysinfo_screen_open(sysinfo_t *sysinfo) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	mp_tui_t *tui = &sysinfo->screen.tui;
	mp_device_t *display = mp_device_find(MP_DEVICE_DISPLAY, 0);
	mp_tui_display_t *face;

	if(mp_device_open(kernel, display) == FALSE)
		return;
	face = mp_device_face(display);
	if(!face || !mp_tui_init(tui, face, _sysinfo_pixels, sizeof(_sysinfo_pixels) / sizeof(_sysinfo_pixels[0]))) {
		mp_device_close(display);
		return;
	}
	sysinfo->screen.display = display;

	if(_sysinfo_pages_open(sysinfo) == 0) {
		mp_tui_fini(tui);
		mp_device_close(display);
		sysinfo->screen.display = NULL;
		return;
	}

#ifdef SUPPORT_COMMON_DEVICE
	if(sysinfo->console == YES)
		mp_device_input_init(kernel, &sysinfo->screen.keys, MP_DEVICE_INPUT_CAP_KEYS, "Console");
#endif
	_sysinfo_touch_run(sysinfo, YES);
	_sysinfo_devices_load(sysinfo);

#ifdef SUPPORT_COMMON_USB
	_sysinfo_usb_watch(sysinfo);
#endif

	sysinfo->screen.ready = YES;
	mp_tui_attach(kernel, tui, "TUI");
	sysinfo->screen.task = mp_task_create(&kernel->tasks, "Screen", _sysinfo_screen, sysinfo, _sysinfo_ticks(250));
	if(sysinfo->console == YES)
		sysinfo->serial.reader = sysinfo->screen.task;
}

void _sysinfo_screen_close(sysinfo_t *sysinfo) {
	if(sysinfo->screen.ready == NO)
		return;
	if(sysinfo->screen.task) {
		sysinfo->serial.reader = NULL;
		mp_task_destroy(sysinfo->screen.task);
		sysinfo->screen.task = NULL;
	}
#ifdef SUPPORT_COMMON_DEVICE
	mp_device_input_fini(&sysinfo->screen.keys);
#endif
	_sysinfo_touch_run(sysinfo, NO);
	_sysinfo_camera_run(sysinfo, NO);
	mp_tui_fini(&sysinfo->screen.tui);
	mp_device_close(sysinfo->screen.display);
	sysinfo->screen.display = NULL;
	sysinfo->screen.ready = NO;
}

static mp_bool_t _sysinfo_accel_page_on(sysinfo_t *sysinfo) {
	return(_sysinfo_dev_accel_shown(sysinfo));
}

mp_bool_t _sysinfo_accel_shown(sysinfo_t *sysinfo) {
	if(sysinfo->screen.ready == NO)
		return(YES);
	return(mp_tui_awake(&sysinfo->screen.tui) && _sysinfo_accel_page_on(sysinfo) == YES ? YES : NO);
}

static unsigned long _sysinfo_screen_period(sysinfo_t *sysinfo) {
	if(!mp_tui_awake(&sysinfo->screen.tui))
		return(1000);
	if(_sysinfo_accel_page_on(sysinfo) == YES)
		return(50);
	if(_sysinfo_dev_camera_shown(sysinfo) == YES)
		return(20);
	return(250);
}

static MP_TASK(_sysinfo_screen) {
	sysinfo_t *sysinfo = task->user;
	unsigned long delay;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	_sysinfo_console_read(sysinfo);
	_sysinfo_accel_apply(sysinfo);
	_sysinfo_pages_refresh(sysinfo);

	/* woken by the console, or the page changed */
	delay = _sysinfo_ticks(_sysinfo_screen_period(sysinfo));
	if(task->signal != MP_TASK_SIG_OK || task->delay != delay)
		mp_task_delay(task, delay);
}

#else

mp_bool_t _sysinfo_accel_shown(sysinfo_t *sysinfo) {
	(void)sysinfo;
	return(YES);
}

void _sysinfo_screen_open(sysinfo_t *sysinfo) {
	(void)sysinfo;
}

void _sysinfo_screen_close(sysinfo_t *sysinfo) {
	(void)sysinfo;
}

#endif
