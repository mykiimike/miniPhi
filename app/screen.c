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
 * Opens the first display of the board, then the pages in pages/. While the splash
 * is up, the self-tests run and the first LED blinks; Morse starts when they finish.
 * The task refreshes the live figures every 250 ms, 50 ms on an accelerometer,
 * 20 ms while a camera picture is on screen, and every second while the screen is off.
 */

#include "pages/index.h"

#if defined(__has_include)
#if __has_include(<xiao-esp32c6/dial.h>)
#include <xiao-esp32c6/dial.h>
#define SYSINFO_PUI_DIAL 1
#endif
#endif

#ifdef SYSINFO_SCREEN

/* four rows: eight no longer fit once the Lepton frame is in RAM */
#define SYSINFO_SCREEN_ROWS 4

#ifndef SYSINFO_SCREEN_COLUMNS
#define SYSINFO_SCREEN_COLUMNS 320
#endif

static mp_tui_color_t _sysinfo_pixels[SYSINFO_SCREEN_COLUMNS * SYSINFO_SCREEN_ROWS];

static MP_TASK(_sysinfo_screen);

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

/* left open: a driver registers itself from open, and close drops that registration.
 * A camera is opened at High, long enough for its driver to read the module,
 * then closed. Selecting its view opens it again. */
static void _sysinfo_devices_load(sysinfo_t *sysinfo) {
	int n;

	for(n = 0; n < mp_device_count(); n++) {
		mp_device_t *device = mp_device_at(n);

		if(!device || device->opened == YES)
			continue;
		if(device->driver && device->driver->type == MP_DEVICE_CAMERA)
			continue;
		mp_device_open(&sysinfo->kernel, device);
	}
#ifdef SUPPORT_COMMON_DEVICE
	_sysinfo_dev_camera_settle(sysinfo);
#endif
	/* open leaves an accelerometer awake, so the next apply sends it to sleep */
	sysinfo->accelRunning = YES;
}

/* the display is still painting; Morse still owns the LEDs */
static void _sysinfo_devices_release(sysinfo_t *sysinfo, mp_bool_t leds) {
	int n;

	for(n = 0; n < mp_device_count(); n++) {
		mp_device_t *device = mp_device_at(n);

		if(!device || device->opened == NO || device == sysinfo->screen.display)
			continue;
		if(leds == NO && device->driver && device->driver->type == MP_DEVICE_LED)
			continue;
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
	mp_tui_hibernate(tui, sysinfo->hibernate == YES);
	/* a touch panel keeps the finger-sized chrome; without one the rows are one text line */
	mp_tui_nav(tui, mp_device_registry_find(kernel, MP_DEVICE_TOUCH, 0) != NULL);
	sysinfo->screen.display = display;

	if(_sysinfo_pages_open(sysinfo) == 0) {
		mp_tui_fini(tui);
		mp_device_close(display);
		sysinfo->screen.display = NULL;
		return;
	}

	_sysinfo_touch_run(sysinfo, YES);
	_sysinfo_devices_load(sysinfo);

#ifdef SUPPORT_COMMON_USB
	_sysinfo_usb_watch(sysinfo);
#endif

	sysinfo->screen.ready = YES;
	mp_tui_attach(kernel, tui, "TUI");
	sysinfo->screen.task = mp_task_create(&kernel->tasks, "Screen", _sysinfo_screen, sysinfo, _sysinfo_ticks(250));
}

void _sysinfo_screen_close(sysinfo_t *sysinfo) {
	if(sysinfo->screen.ready == NO)
		return;
	if(sysinfo->screen.task) {
		mp_task_destroy(sysinfo->screen.task);
		sysinfo->screen.task = NULL;
	}
	_sysinfo_touch_run(sysinfo, NO);
	/* the overlay reads the camera buffer; close it before the device does */
	mp_tui_fullscreen_close(&sysinfo->screen.tui);
	_sysinfo_devices_release(sysinfo, NO);
#ifdef SUPPORT_COMMON_DEVICE
	_sysinfo_dev_camera_release(sysinfo);
#endif
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

	_sysinfo_accel_apply(sysinfo);
	_sysinfo_pages_refresh(sysinfo);

	/* the page changed */
	delay = _sysinfo_ticks(_sysinfo_screen_period(sysinfo));
	if(task->signal != MP_TASK_SIG_OK || task->delay != delay)
		mp_task_delay(task, delay);
}

#else

#ifdef SUPPORT_COMMON_PUI

static mp_device_t *_sysinfo_pui_display;
static mp_task_t *_sysinfo_pui_tick_task;

/* while a built-in page is on the glass, its reading still moves */
static MP_TASK(_sysinfo_pui_tick) {
	sysinfo_t *sysinfo = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	/* a class page joins the tour only while that device exists.
	 * Each other measure already has its own page. */
	mp_pui_page_show(mp_pui_page_temp_page(),   mp_pui_page_temp_present(&sysinfo->kernel)  ? 1 : 0);
	mp_pui_page_show(mp_pui_page_clock_page(),  mp_pui_page_clock_present(&sysinfo->kernel) ? 1 : 0);
	mp_pui_page_show(mp_pui_page_charge_page(), mp_pui_page_charge_present(&sysinfo->kernel) ? 1 : 0);
	mp_pui_page_faces_sync(&sysinfo->pui);
	mp_pui_pages_refresh(&sysinfo->pui);
	mp_task_delay(task, _sysinfo_ticks(1000));
}

/**
 * Every button turns the page on the press. A serial containing "prev"
 * (D0) or "down" (dial down, stored as dial-down) steps back. The others
 * advance: D1 "next", dial up, dial press.
 */
static int _sysinfo_pui_back(mp_device_registry_t *dev) {
	char type[16];
	char model[MP_DEVICE_ADDRESS];
	char serial[MP_DEVICE_ADDRESS];
	int index;

	if(!dev)
		return(0);
	if(mp_device_address_split(dev->address, type, (int)sizeof(type), &index,
		model, (int)sizeof(model), serial, (int)sizeof(serial)) == FALSE)
		return(0);
	if(strstr(serial, "prev") || strstr(serial, "down"))
		return(1);
	return(0);
}

static void _sysinfo_pui_button(mp_device_button_t *button, int down, void *user) {
	sysinfo_t *sysinfo = user;
	int i;

	if(!sysinfo || !down)
		return;
	for(i = 0; i < MP_PUI_BUTTONS_MAX; i++) {
		if(sysinfo->puiButton[i] != button)
			continue;
		mp_pui_key(&sysinfo->pui, _sysinfo_pui_back(sysinfo->puiButtonDev[i])
			? MP_PUI_KEY_PREV : MP_PUI_KEY_NEXT);
		return;
	}
}

#endif

mp_bool_t _sysinfo_accel_shown(sysinfo_t *sysinfo) {
	(void)sysinfo;
	return(YES);
}

void _sysinfo_screen_open(sysinfo_t *sysinfo) {
#ifdef SUPPORT_COMMON_PUI
	mp_device_t *display = mp_device_find(MP_DEVICE_DISPLAY, 0);
	mp_pui_display_t *face;
	mp_pui_t *ui = &sysinfo->pui;

	if(mp_device_open(&sysinfo->kernel, display) == FALSE)
		return;
	face = mp_device_face(display);
	if(!face || !mp_pui_init(ui, face, sysinfo->puiBand, sizeof(sysinfo->puiBand))) {
		mp_device_close(display);
		return;
	}
	_sysinfo_pui_display = display;
	ui->kernel = &sysinfo->kernel;
	if(mp_pui_page_about(ui) == FALSE) {
		mp_pui_fini(ui);
		mp_device_close(display);
		_sysinfo_pui_display = NULL;
		return;
	}
	/* open the board's other devices so their drivers register.
	 * The display is already open and skipped. */
	{
		int n;
		for(n = 0; n < mp_device_count(); n++) {
			mp_device_t *device = mp_device_at(n);
			if(!device || device->opened == YES || device == display)
				continue;
			if(device->driver && device->driver->type == MP_DEVICE_CAMERA)
				continue;
			mp_device_open(&sysinfo->kernel, device);
		}
	}
	/* temperature, clock and charge keep one page. Every other measure
	 * device gets its own page, then the CPU page. */
	mp_pui_page_temp(ui);
	mp_pui_page_clock(ui);
	mp_pui_page_charge(ui);
	mp_pui_page_faces(ui);
	mp_pui_page_cpu(ui);
	/* the tick refreshes the built-in page that is on the glass */
	_sysinfo_pui_tick_task = mp_task_create(&sysinfo->kernel.tasks, "pui face", _sysinfo_pui_tick, sysinfo, _sysinfo_ticks(1000));
	if(!mp_pui_attach(&sysinfo->kernel, ui, "PUI")) {
		mp_pui_fini(ui);
		mp_device_close(display);
		_sysinfo_pui_display = NULL;
		return;
	}
	mp_pui_page_about_start(ui);
#ifdef SYSINFO_PUI_DIAL
	/* registers dial up / down / press before the buttons are wired */
	mp_board_dial(&sysinfo->kernel);
#endif
	/* every button: serial "prev" or "down" steps back, the rest advance */
	{
		int n;

		for(n = 0; n < MP_PUI_BUTTONS_MAX; n++) {
			mp_device_registry_t *dev = mp_device_registry_find(&sysinfo->kernel, MP_DEVICE_BUTTON, n);
			mp_device_button_t *face;

			if(!dev)
				break;
			face = mp_device_button(dev, &sysinfo->puiButtonSub[n]);
			sysinfo->puiButtonDev[n] = dev;
			sysinfo->puiButton[n] = face;
			if(!face)
				continue;
			face->on_change = _sysinfo_pui_button;
			face->user = sysinfo;
		}
	}
#else
	/* opening the panel leaves the driver's picture on the glass */
	mp_device_open(&sysinfo->kernel, mp_device_find(MP_DEVICE_DISPLAY, 0));
#endif
}

void _sysinfo_screen_close(sysinfo_t *sysinfo) {
#ifdef SUPPORT_COMMON_PUI
	if(!_sysinfo_pui_display)
		return;
	if(_sysinfo_pui_tick_task) {
		mp_task_destroy(_sysinfo_pui_tick_task);
		_sysinfo_pui_tick_task = NULL;
	}
	{
		int n;

		for(n = 0; n < MP_PUI_BUTTONS_MAX; n++) {
			if(sysinfo->puiButton[n]) {
				sysinfo->puiButton[n]->on_change = NULL;
				sysinfo->puiButton[n]->user = NULL;
				sysinfo->puiButton[n] = NULL;
			}
			mp_device_subscribe_end(&sysinfo->puiButtonSub[n]);
			sysinfo->puiButtonDev[n] = NULL;
		}
	}
	mp_pui_fini(&sysinfo->pui);
	mp_device_close(_sysinfo_pui_display);
	_sysinfo_pui_display = NULL;
#else
	(void)sysinfo;
#endif
}

#endif
