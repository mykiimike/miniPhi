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
 * When the board has LEDs, morse.c blinks SOS on the first and HELP on the second.
 * On the first display of the board, screen.c opens the TUI and the pages in pages/. System is a scrollable page
 * with about, version, copyright, license and the live figures. Back returns
 * to the home.
 */

#include "sysinfo.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

/* opens or closes every accelerometer of the board */
static void _sysinfo_accel_run(sysinfo_t *sysinfo, mp_bool_t on) {
	mp_device_t *device;
	int rank;

	for(rank = 0; (device = mp_device_find(MP_DEVICE_ACCELEROMETER, rank)) != NULL; rank++) {
		if(on == YES)
			mp_device_open(&sysinfo->kernel, device);
		else
			mp_device_close(device);
	}
}

/* the accelerometers run while enabled and, with a screen, looked at */
void _sysinfo_accel_apply(sysinfo_t *sysinfo) {
	mp_bool_t on = sysinfo->accelEnabled == YES && _sysinfo_accel_shown(sysinfo) == YES ? YES : NO;

	if(on == sysinfo->accelRunning)
		return;
	sysinfo->accelRunning = on;
	_sysinfo_accel_run(sysinfo, on);
}

#ifdef SUPPORT_COMMON_KFLASH
static void _sysinfo_accel_changed(mp_kflash_var_t *var) {
	sysinfo_t *sysinfo = var->user;

	mp_printk("%s %s", var->key, sysinfo->accelEnabled == YES ? "on" : "off");
	_sysinfo_accel_apply(sysinfo);
}
#endif

void _sysinfo_accel_enable(sysinfo_t *sysinfo, mp_bool_t on) {
#ifdef SUPPORT_COMMON_KFLASH
	if(mp_kflash_set_bool(&sysinfo->kernel, SYSINFO_KEY_ACCEL, on) == TRUE)
		return;
	mp_printk("%s: the flash did not take it, until the next reset only", SYSINFO_KEY_ACCEL);
#endif
	sysinfo->accelEnabled = on ? YES : NO;
	_sysinfo_accel_apply(sysinfo);
}

static void _sysinfo_op_set(void *user) {
	sysinfo_t *sysinfo = user;
	mp_options_t console[] = PHI_CONSOLE;

	_sysinfo_morse_start(sysinfo);

#ifdef SUPPORT_COMMON_KFLASH
	sysinfo->accelListened = mp_kflash_listen_bool(&sysinfo->kernel, &sysinfo->accelVar,
		SYSINFO_KEY_ACCEL, &sysinfo->accelEnabled, YES, _sysinfo_accel_changed, sysinfo);
	if(sysinfo->accelListened == FALSE)
		sysinfo->accelEnabled = YES;
#else
	sysinfo->accelEnabled = YES;
#endif

	if(mp_serial_open(&sysinfo->kernel, &sysinfo->serial, console, "Console") == FALSE) {
		_sysinfo_screen_open(sysinfo);
		return;
	}

	sysinfo->console = YES;
	mp_printk_set(_sysinfo_printk, sysinfo);

	/* after the console, so the driver messages show */
	_sysinfo_screen_open(sysinfo);

	/* lets the arch publish its own sensors, such as the die temperature */
	mp_machine_state_set(&sysinfo->kernel);

	_sysinfo_accel_apply(sysinfo);

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

	_sysinfo_screen_close(sysinfo);
	_sysinfo_morse_stop(sysinfo);

	_sysinfo_accel_run(sysinfo, NO);
	sysinfo->accelRunning = NO;
#ifdef SUPPORT_COMMON_KFLASH
	if(sysinfo->accelListened == YES) {
		mp_kflash_unlisten(&sysinfo->kernel, &sysinfo->accelVar);
		sysinfo->accelListened = NO;
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
	mp_printk("  power %s %lu MHz, load %u%%, %lu wake/s, %lu deep",
		mp_power_name(kernel->power.level), (mp_clock_level_hz(kernel->power.level) + 500000UL) / 1000000UL,
		(unsigned int)kernel->power.load, kernel->power.wakeups, kernel->power.deep);

#ifdef SUPPORT_COMMON_SENSOR
	{
		mp_sensor_t *sensor = NULL;
		float v[3];

		while((sensor = mp_sensor_next(kernel, sensor)) != NULL) {
			if(mp_sensor_read(sensor, v, NULL) != TRUE)
				mp_printk("  %s: --", sensor->name);
			else if(mp_sensor_axes(sensor->type) == 3)
				mp_printk("  %s: %.2f %.2f %.2f %s", sensor->name,
					(double)v[0], (double)v[1], (double)v[2], mp_sensor_unit(sensor->type));
			else
				mp_printk("  %s: %.3f %s", sensor->name, (double)v[0], mp_sensor_unit(sensor->type));
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
