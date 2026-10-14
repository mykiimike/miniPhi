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
 * When the board has LEDs, the first one blinks SOS in Morse, the second HELP.
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

static void _sysinfo_morse_led(sysinfo_t *sysinfo, int index, const char *pattern) {
	mp_device_t *device = mp_device_find(MP_DEVICE_LED, index);

	if(mp_device_open(&sysinfo->kernel, device) == FALSE)
		return;
	sysinfo->leds[index].device = device;
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
		if(!sysinfo->leds[index].device)
			continue;

		mp_device_set(sysinfo->leds[index].device, sysinfo->leds[index].pattern[sysinfo->leds[index].step] == '1');

		if(sysinfo->leds[index].pattern[++sysinfo->leds[index].step] == '\0')
			sysinfo->leds[index].step = 0;
	}
}

void _sysinfo_morse_start(sysinfo_t *sysinfo) {
	/* the LEDs do not need the console */
	_sysinfo_morse_led(sysinfo, 0, _sos);
	_sysinfo_morse_led(sysinfo, 1, _help);
	if(sysinfo->leds[0].device || sysinfo->leds[1].device)
		sysinfo->morse = mp_task_create(
			&sysinfo->kernel.tasks, "Morse", _sysinfo_morse, sysinfo,
			_sysinfo_ticks(SYSINFO_MORSE_UNIT)
		);
}

void _sysinfo_morse_stop(sysinfo_t *sysinfo) {
	int index;

	if(sysinfo->morse) {
		mp_task_destroy(sysinfo->morse);
		sysinfo->morse = NULL;
	}
	for(index = 0; index < SYSINFO_LEDS; index++) {
		if(!sysinfo->leds[index].device)
			continue;
		mp_device_close(sysinfo->leds[index].device);
		sysinfo->leds[index].device = NULL;
	}
}
