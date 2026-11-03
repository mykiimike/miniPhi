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
 * The first lamp blinks SOS, the second HELP.
 * A board LED fills a rank. A rank still empty is a channel of the pixel,
 * red then green, so one pixel runs both.
 */

#include "sysinfo.h"

/* one char per Morse unit, '1' lit: dot 1, dash 3, gaps 1 / 3 (letter) / 7 (word) */
static const char _sos[] =
	"10101" "000"
	"11101110111" "000"
	"10101" "0000000";

static const char _help[] =
	"1010101" "000"
	"1" "000"
	"101110101" "000"
	"10111011101" "0000000";

#define SYSINFO_LEDS ((int)(sizeof(((sysinfo_t *)0)->leds) / sizeof(((sysinfo_t *)0)->leds[0])))

/* 255 is full. The onboard pixel is harsh at that, so a lamp comes back here. */
#define SYSINFO_PIXEL_LEVEL 16

static int _sysinfo_lamp_live(sysinfo_t *sysinfo, int index) {
	return(sysinfo->leds[index].device || sysinfo->leds[index].pixel);
}

static void _sysinfo_lamp_bind(sysinfo_t *sysinfo, int index) {
	mp_device_t *device;
	mp_device_registry_t *reg;

	if(_sysinfo_lamp_live(sysinfo, index))
		return;
	device = mp_device_find(MP_DEVICE_LED, index);
	if(mp_device_open(&sysinfo->kernel, device) == TRUE) {
		sysinfo->leds[index].device = device;
		return;
	}
	if(!sysinfo->pixel) {
		reg = mp_device_registry_find(&sysinfo->kernel, MP_DEVICE_PIXEL, 0);
		sysinfo->pixel = mp_device_pixel(reg, &sysinfo->pixelSub);
		if(sysinfo->pixel)
			mp_device_pixel_level(sysinfo->pixel, 0, SYSINFO_PIXEL_LEVEL);
	}
	if(!sysinfo->pixel || index >= MP_DEVICE_PIXEL_CHANNELS)
		return;
	sysinfo->leds[index].pixel = sysinfo->pixel;
	sysinfo->leds[index].channel = index;
}

static void _sysinfo_lamp_write(sysinfo_t *sysinfo, int index, int on) {
	if(sysinfo->leds[index].device)
		mp_device_set(sysinfo->leds[index].device, on);
	else if(sysinfo->leds[index].pixel)
		mp_device_pixel_channel_set(sysinfo->leds[index].pixel, 0, sysinfo->leds[index].channel, on);
}

void _sysinfo_lamp_set(sysinfo_t *sysinfo, int index, int on) {
	if(!sysinfo || index < 0 || index >= SYSINFO_LEDS)
		return;
	_sysinfo_lamp_bind(sysinfo, index);
	_sysinfo_lamp_write(sysinfo, index, on);
}

static void _sysinfo_morse_led(sysinfo_t *sysinfo, int index, const char *pattern) {
	_sysinfo_lamp_bind(sysinfo, index);
	if(!_sysinfo_lamp_live(sysinfo, index))
		return;
	sysinfo->leds[index].pattern = pattern;
	sysinfo->leds[index].step = 0;
}

/* each LED loops over its own message, one Morse unit per wake up */
static MP_TASK(_sysinfo_morse) {
	sysinfo_t *sysinfo = task->user;
	int index;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	for(index = 0; index < SYSINFO_LEDS; index++) {
		if(!sysinfo->leds[index].pattern)
			continue;

		_sysinfo_lamp_write(sysinfo, index, sysinfo->leds[index].pattern[sysinfo->leds[index].step] == '1');

		if(sysinfo->leds[index].pattern[++sysinfo->leds[index].step] == '\0')
			sysinfo->leds[index].step = 0;
	}
}

void _sysinfo_morse_start(sysinfo_t *sysinfo) {
	/* already running: a second create would leave the first task in the list */
	if(sysinfo->morse)
		return;
	/* the LEDs do not need the console */
	_sysinfo_morse_led(sysinfo, 0, _sos);
	_sysinfo_morse_led(sysinfo, 1, _help);
	if(sysinfo->leds[0].pattern || sysinfo->leds[1].pattern)
		sysinfo->morse = mp_task_create(
			&sysinfo->kernel.tasks, "Morse", _sysinfo_morse, sysinfo,
			_sysinfo_ticks(SYSINFO_MORSE_UNIT)
		);
	/* Above the background, so a long task does not stretch a Morse unit.
	 * Cooperative builds store the level and do not preempt. */
	if(sysinfo->morse)
		mp_task_priority(sysinfo->morse, 1);
}

void _sysinfo_morse_stop(sysinfo_t *sysinfo) {
	int index;

	if(sysinfo->morse) {
		mp_task_destroy(sysinfo->morse);
		sysinfo->morse = NULL;
	}
	for(index = 0; index < SYSINFO_LEDS; index++) {
		if(sysinfo->leds[index].pattern)
			_sysinfo_lamp_write(sysinfo, index, 0);
		if(sysinfo->leds[index].device)
			mp_device_close(sysinfo->leds[index].device);
		sysinfo->leds[index].device = NULL;
		sysinfo->leds[index].pixel = NULL;
		sysinfo->leds[index].pattern = NULL;
		sysinfo->leds[index].step = 0;
	}
	if(sysinfo->pixelSub.device)
		mp_device_subscribe_end(&sysinfo->pixelSub);
	sysinfo->pixel = NULL;
}
