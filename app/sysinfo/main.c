/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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
 * System info: prints who is running on the console, then reports
 * uptime, tasks and sensors every SYSINFO_PERIOD ms.
 * Delays are milliseconds and follow mp_clock_tick_hz().
 * When the board has LEDs, the first one blinks SOS in Morse, the second HELP.
 */

#include <mp.h>
#include <pinout.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SYSINFO_OP MP_KERNEL_RES01

typedef struct sysinfo_s sysinfo_t;

struct sysinfo_s {
	mp_kernel_t kernel;

	mp_serial_t serial;
	mp_bool_t console;

	mp_task_t *task;
	int line;

#if defined(SUPPORT_DRV_LED) && defined(PHI_LED_FIRST)
	struct {
		mp_drv_led_t led;
		mp_bool_t ready;
		const char *pattern;
		int step;
	} leds[2];
	mp_task_t *morse;
#endif
};

static unsigned long _sysinfo_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);

	ticks = (ms / 1000) * hz + ((ms % 1000) * hz) / 1000;
	return(ticks ? ticks : 1);
}

static void _sysinfo_onBoot(void *user);
static void _sysinfo_op_set(void *user);
static void _sysinfo_op_unset(void *user);
static void _sysinfo_op_tick(void *user);
static void _sysinfo_printk(void *user, char *fmt, ...);
static mp_bool_t _sysinfo_line(sysinfo_t *sysinfo, int line);
static void _sysinfo_report(sysinfo_t *sysinfo);
static MP_TASK(_sysinfo_task);

static const char *_banner[] = {
	"           _       _ ____  _     _",
	" _ __ ___ (_)_ __ (_)  _ \\| |__ (_)",
	"| '_ ` _ \\| | '_ \\| | |_) | '_ \\| |",
	"| | | | | | | | | | |  __/| | | | |",
	"|_| |_| |_|_|_| |_|_|_|   |_| |_|_|",
};

#define SYSINFO_BANNER_LINES ((int)(sizeof(_banner) / sizeof(_banner[0])))

#if defined(SUPPORT_DRV_LED) && defined(PHI_LED_FIRST)
#define SYSINFO_MORSE

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

static void _sysinfo_morse_led(sysinfo_t *sysinfo, int index, mp_options_t *options, const char *pattern, char *who);
static MP_TASK(_sysinfo_morse);
#endif

static sysinfo_t _sysinfo;

int main(void) {
	sysinfo_t *sysinfo = &_sysinfo;

	memset(sysinfo, 0, sizeof(*sysinfo));

	/* initialize kernel */
	mp_kernel_init(&sysinfo->kernel, _sysinfo_onBoot, sysinfo);

	/* define the application state */
	mp_state_define(
		&sysinfo->kernel.states,
		SYSINFO_OP, "SYSINFO", sysinfo,
		_sysinfo_op_set,
		_sysinfo_op_unset,
		_sysinfo_op_tick
	);

	/* master loop */
	mp_kernel_loop(&sysinfo->kernel);

	/* terminate kernel */
	mp_kernel_fini(&sysinfo->kernel);

	return 0;
}

static void _sysinfo_onBoot(void *user) {
	sysinfo_t *sysinfo = user;
	mp_kernel_state(&sysinfo->kernel, SYSINFO_OP);
}

static void _sysinfo_op_set(void *user) {
	sysinfo_t *sysinfo = user;
	mp_options_t console[] = PHI_CONSOLE;
#ifdef SYSINFO_MORSE
	mp_options_t sos[] = PHI_LED_FIRST;
#ifdef PHI_LED_SECOND
	mp_options_t help[] = PHI_LED_SECOND;
#endif

	/* the LEDs do not need the console */
	_sysinfo_morse_led(sysinfo, 0, sos, _sos, "SOS");
#ifdef PHI_LED_SECOND
	_sysinfo_morse_led(sysinfo, 1, help, _help, "HELP");
#endif
	if(sysinfo->leds[0].ready == YES || sysinfo->leds[1].ready == YES)
		sysinfo->morse = mp_task_create(
			&sysinfo->kernel.tasks, "Morse", _sysinfo_morse, sysinfo,
			_sysinfo_ticks(SYSINFO_MORSE_UNIT)
		);
#endif

	if(mp_serial_open(&sysinfo->kernel, &sysinfo->serial, console, "Console") == FALSE)
		return;

	sysinfo->console = YES;
	mp_printk_set(_sysinfo_printk, sysinfo);

	/* lets the arch publish its own sensors, such as the die temperature */
	mp_machine_state_set(&sysinfo->kernel);

	/* one line per wake up: each write takes chunks from the memory pool */
	sysinfo->line = 0;
	sysinfo->task = mp_task_create(
		&sysinfo->kernel.tasks, "Sysinfo", _sysinfo_task, sysinfo,
		_sysinfo_ticks(SYSINFO_LINE_DELAY)
	);
}

static void _sysinfo_op_unset(void *user) {
	sysinfo_t *sysinfo = user;

	if(sysinfo->task) {
		mp_task_destroy(sysinfo->task);
		sysinfo->task = NULL;
	}

#ifdef SYSINFO_MORSE
	if(sysinfo->morse) {
		mp_task_destroy(sysinfo->morse);
		sysinfo->morse = NULL;
	}
	{
		int index;

		for(index = 0; index < SYSINFO_LEDS; index++) {
			if(sysinfo->leds[index].ready == NO)
				continue;
			mp_drv_led_turnOff(&sysinfo->leds[index].led);
			mp_drv_led_fini(&sysinfo->leds[index].led);
			sysinfo->leds[index].ready = NO;
		}
	}
#endif

	if(sysinfo->console == NO)
		return;

	mp_machine_state_unset(&sysinfo->kernel);
	mp_printk_unset();
	mp_serial_fini(&sysinfo->serial);
	sysinfo->console = NO;
}

static void _sysinfo_op_tick(void *user) {
	/* everything runs in the task */
}

static MP_TASK(_sysinfo_task) {
	sysinfo_t *sysinfo = task->user;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(sysinfo->line >= 0) {
		if(_sysinfo_line(sysinfo, sysinfo->line) == YES) {
			sysinfo->line++;
			return;
		}
		sysinfo->line = -1;
		task->delay = _sysinfo_ticks(SYSINFO_PERIOD);
	}

	_sysinfo_report(sysinfo);
}

#ifdef SYSINFO_MORSE
static void _sysinfo_morse_led(sysinfo_t *sysinfo, int index, mp_options_t *options, const char *pattern, char *who) {
	if(mp_drv_led_init(&sysinfo->kernel, &sysinfo->leds[index].led, options, who) == FALSE)
		return;
	sysinfo->leds[index].ready = YES;
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
		if(sysinfo->leds[index].ready == NO)
			continue;

		if(sysinfo->leds[index].pattern[sysinfo->leds[index].step] == '1')
			mp_drv_led_turnOn(&sysinfo->leds[index].led);
		else
			mp_drv_led_turnOff(&sysinfo->leds[index].led);

		if(sysinfo->leds[index].pattern[++sysinfo->leds[index].step] == '\0')
			sysinfo->leds[index].step = 0;
	}
}
#endif

static mp_bool_t _sysinfo_line(sysinfo_t *sysinfo, int line) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	unsigned long speed;

	if(line < SYSINFO_BANNER_LINES) {
		mp_printk("%s", _banner[line]);
		return(YES);
	}

	switch(line - SYSINFO_BANNER_LINES) {
		case 0:
			mp_printk("%s", "");
			break;

		case 1:
			mp_printk("miniPhi    %s", kernel->version);
			break;

		case 2:
			mp_printk("Build      %s %s", __DATE__, __TIME__);
			break;

		case 3:
			mp_printk("Board      %s, app %s", PHI_BUILD_BOARD, PHI_BUILD_APP);
			break;

		case 4:
#if defined(__TI_COMPILER_VERSION__)
			mp_printk("Compiler   %s %lu.%lu.%lu", PHI_BUILD_TOOLCHAIN,
				(unsigned long)__TI_COMPILER_VERSION__ / 1000000,
				((unsigned long)__TI_COMPILER_VERSION__ / 1000) % 1000,
				(unsigned long)__TI_COMPILER_VERSION__ % 1000);
#elif defined(__GNUC__)
			mp_printk("Compiler   %s %s", PHI_BUILD_TOOLCHAIN, __VERSION__);
#else
			mp_printk("Compiler   %s", PHI_BUILD_TOOLCHAIN);
#endif
			break;

		case 5:
			mp_printk("CPU        %s %s",
				kernel->mcuVendor ? kernel->mcuVendor : "unknown",
				kernel->mcuName ? kernel->mcuName : "unknown");
			break;

		case 6:
			mp_printk("Target     %s / %s / %s", PHI_BUILD_ARCH, PHI_BUILD_SERIES, PHI_BUILD_MCU);
			break;

		case 7:
			speed = mp_clock_get_speed();
			mp_printk("Clock      %lu.%02lu MHz", speed / 1000000, (speed / 10000) % 100);
			break;

		case 8:
			mp_printk("Memory     %lu KB flash, %lu KB RAM", PHI_BUILD_FLASH / 1024, PHI_BUILD_RAM / 1024);
			break;

		case 9:
			mp_printk("Kernel     %u tasks, %u states, %u bytes",
				(unsigned int)MP_TASK_MAX, (unsigned int)MP_STATE_MAX, (unsigned int)sizeof(*kernel));
			break;

		case 10:
#ifdef SUPPORT_COMMON_MEM
			mp_printk("Pool       %u bytes, %u-byte chunks", (unsigned int)MP_MEM_SIZE, (unsigned int)MP_MEM_CHUNK);
#else
			mp_printk("Pool       off");
#endif
			break;

		case 11:
			mp_printk("Modules    %s", PHI_BUILD_MODULES);
			break;

		case 12:
			mp_printk("Drivers    %s", PHI_BUILD_DRIVERS);
			break;

		case 13:
			mp_printk("%s", "");
			break;

		default:
			return(NO);
	}

	return(YES);
}

static void _sysinfo_report(sysinfo_t *sysinfo) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	unsigned long hz = mp_clock_tick_hz();
	unsigned long seconds = hz ? mp_clock_ticks() / hz : 0;

	mp_printk("up %lu:%02lu:%02lu, %u/%u tasks",
		seconds / 3600, (seconds / 60) % 60, seconds % 60,
		kernel->tasks.usedNumber, (unsigned int)MP_TASK_MAX);

#ifdef SUPPORT_COMMON_SENSOR
	{
		mp_list_item_t *item;
		mp_sensor_t *sensor;

		for(item = kernel->sensors.list.first; item != NULL; item = item->next) {
			sensor = item->user;
			switch(sensor->type) {
				case MP_SENSOR_TEMPERATURE:
					mp_printk("  %s: %.1f C", sensor->idName, (double)sensor->temperature.result);
					break;
				case MP_SENSOR_VOLTAGE:
					mp_printk("  %s: %.3f V", sensor->idName, (double)sensor->voltage.result);
					break;
				case MP_SENSOR_CURRENT:
					mp_printk("  %s: %.3f A", sensor->idName, (double)sensor->current.result);
					break;
				case MP_SENSOR_BAROMETER:
					mp_printk("  %s: %.1f Pa", sensor->idName, (double)sensor->barometer.result);
					break;
				case MP_SENSOR_ALTIMETER:
					mp_printk("  %s: %.1f", sensor->idName, (double)sensor->altimeter.result);
					break;
				case MP_SENSOR_3AXIS:
					mp_printk("  %s: %.2f %.2f %.2f", sensor->idName,
						(double)sensor->axis3.x, (double)sensor->axis3.y, (double)sensor->axis3.z);
					break;
			}
		}
	}
#endif
}

static void _sysinfo_printk(void *user, char *fmt, ...) {
	sysinfo_t *sysinfo = user;
	unsigned char buffer[128];
	va_list args;
	int size;

	va_start(args, fmt);
	size = vsnprintf((char *)buffer, sizeof(buffer) - 2, fmt, args);
	va_end(args);

	if(size < 0)
		return;
	if(size > (int)sizeof(buffer) - 3)
		size = sizeof(buffer) - 3;

	buffer[size++] = '\n';
	buffer[size++] = '\r';

	mp_serial_write(&sysinfo->serial, buffer, size);
}
