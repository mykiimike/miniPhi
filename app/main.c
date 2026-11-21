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
 * uptime, tasks and sensors every SYSINFO_PERIOD ms. The console is
 * one client. With the terminal codec in the build that client is the
 * terminal, and printk is not wired. Without it, printk owns the line.
 * Delays are milliseconds and follow mp_clock_tick_hz().
 * When the board has lamps, morse.c blinks SOS on the first and HELP on the second.
 * A pixel's red and green fill the ranks a board LED left empty.
 * When a self-test is in the build, Morse still owns the lamps. The checks
 * start once the banner has been printed, at the high clock, and one result
 * is printed per wake. The fast checks then repeat, still one per wake, so
 * Morse keeps the lamps through the whole series. A KEM runs once more.
 * With a TUI they wait until the splash logo is on screen.
 * On the first display of the board, screen.c opens the TUI and the pages in pages/. System is a scrollable page
 * with about, version, copyright, license and the live figures. Back returns
 * to the home.
 */

#include "sysinfo.h"

#include <stdio.h>
#include <string.h>

#ifdef SUPPORT_COMMON_TERM
static void _sysinfo_term_start(sysinfo_t *sysinfo);
#endif

static void _sysinfo_onBoot(void *user);
static void _sysinfo_op_set(void *user);
static void _sysinfo_op_unset(void *user);
static void _sysinfo_op_tick(void *user);
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

/* 0 sleeps the chip. The driver stays registered; close is for shutdown. */
static void _sysinfo_accel_run(sysinfo_t *sysinfo, mp_bool_t on) {
	mp_device_t *device;
	int rank;

	for(rank = 0; (device = mp_device_find(MP_DEVICE_ACCELEROMETER, rank)) != NULL; rank++) {
		if(on == YES) {
			if(device->opened == NO && mp_device_open(&sysinfo->kernel, device) == FALSE)
				continue;
			mp_device_set(device, 1);
		}
		else if(device->opened == YES)
			mp_device_set(device, 0);
	}
}

/* the screen has let go of its devices; this closes whatever is still open */
static void _sysinfo_devices_shutdown(sysinfo_t *sysinfo) {
	int n;

	(void)sysinfo;
	for(n = 0; n < mp_device_count(); n++) {
		mp_device_t *device = mp_device_at(n);

		if(device && device->opened == YES)
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

static void _sysinfo_hibernate_apply(sysinfo_t *sysinfo) {
#ifdef SYSINFO_SCREEN
	if(sysinfo->screen.ready == YES)
		mp_tui_hibernate(&sysinfo->screen.tui, sysinfo->hibernate == YES);
#else
	(void)sysinfo;
#endif
}

#ifdef SUPPORT_COMMON_KFLASH
static void _sysinfo_accel_changed(mp_kflash_var_t *var) {
	sysinfo_t *sysinfo = var->user;

	mp_printk("%s %s", var->key, sysinfo->accelEnabled == YES ? "on" : "off");
	_sysinfo_accel_apply(sysinfo);
}

static void _sysinfo_hibernate_changed(mp_kflash_var_t *var) {
	sysinfo_t *sysinfo = var->user;

	_sysinfo_hibernate_apply(sysinfo);
	mp_printk("%s %s", var->key, sysinfo->hibernate == YES ? "on" : "off");
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

void _sysinfo_hibernate_enable(sysinfo_t *sysinfo, mp_bool_t on) {
#ifdef SUPPORT_COMMON_KFLASH
	if(mp_kflash_set_bool(&sysinfo->kernel, SYSINFO_KEY_HIBERNATE, on) == TRUE)
		return;
	mp_printk("%s: the flash did not take it, until the next reset only", SYSINFO_KEY_HIBERNATE);
#endif
	sysinfo->hibernate = on ? YES : NO;
	_sysinfo_hibernate_apply(sysinfo);
}

#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST
static MP_TASK(_sysinfo_selftest) {
	sysinfo_t *sysinfo = task->user;
	const mp_crypto_selftest_t *test;
	unsigned long t0;
	unsigned long us;
	mp_bool_t ok;

	if(task->signal == MP_TASK_SIG_STOP) {
		if(!sysinfo->morse)
			_sysinfo_lamp_set(sysinfo, 0, 0);
		sysinfo->selftest = NULL;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

#ifdef SYSINFO_SCREEN
	/* the logo is on screen once the splash holds; before that, keep waiting */
	if(sysinfo->screen.ready == YES && sysinfo->screen.tui.splash < 3 && !sysinfo->screen.tui.splashHold) {
		mp_task_delay(task, 1);
		return;
	}
#endif
	/* the banner task owns the console until its last line */
	if(sysinfo->task && sysinfo->line >= 0) {
		mp_task_delay(task, 1);
		return;
	}

	if(sysinfo->selftestIndex >= mp_crypto_selftest_count()) {
		mp_printk("SELFTEST: %lu MHz, %lu us",
			(mp_clock_get_speed() + 500000UL) / 1000000UL, sysinfo->selftestUs);
		sysinfo->selftest = NULL;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(sysinfo->selftestIndex == 0) {
		mp_crypto_selftest_hw_kernel(&sysinfo->kernel);
		mp_printk("SELFTEST: %s %lu MHz",
			mp_power_name(mp_power_level(&sysinfo->kernel)),
			(mp_clock_get_speed() + 500000UL) / 1000000UL);
	}

	test = mp_crypto_selftest_at(sysinfo->selftestIndex);
	if(test && test->run) {
		t0 = mp_clock_us();
		ok = test->run() == YES ? YES : NO;
		us = mp_clock_us() - t0;
		sysinfo->selftestUs += us;
		mp_printk("SELFTEST: %s %s %lu us",
			test->label, ok == YES ? "passed" : "failed", us);
	}
	sysinfo->selftestIndex++;
	mp_task_delay(task, 1);
}

void _sysinfo_selftest_start(sysinfo_t *sysinfo) {
	if(sysinfo->selftest || mp_crypto_selftest_count() <= 0)
		return;
	sysinfo->selftest = mp_task_create(
		&sysinfo->kernel.tasks, "Selftest", _sysinfo_selftest, sysinfo, 1
	);
}
#endif

static void _sysinfo_op_set(void *user) {
	sysinfo_t *sysinfo = user;
	mp_options_t console[] = PHI_CONSOLE;

	_sysinfo_morse_start(sysinfo);

#ifdef SUPPORT_COMMON_KFLASH
	sysinfo->accelListened = mp_kflash_listen_bool(&sysinfo->kernel, &sysinfo->accelVar,
		SYSINFO_KEY_ACCEL, &sysinfo->accelEnabled, YES, _sysinfo_accel_changed, sysinfo);
	if(sysinfo->accelListened == FALSE)
		sysinfo->accelEnabled = YES;
	sysinfo->hibernateListened = mp_kflash_listen_bool(&sysinfo->kernel, &sysinfo->hibernateVar,
		SYSINFO_KEY_HIBERNATE, &sysinfo->hibernate, NO, _sysinfo_hibernate_changed, sysinfo);
	if(sysinfo->hibernateListened == FALSE)
		sysinfo->hibernate = NO;
#else
	sysinfo->accelEnabled = YES;
	sysinfo->hibernate = NO;
#endif

	if(mp_serial_open(&sysinfo->kernel, &sysinfo->serial, console, "Console") == FALSE) {
		_sysinfo_screen_open(sysinfo);
		_sysinfo_morse_start(sysinfo);
#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST
		_sysinfo_selftest_start(sysinfo);
#endif
		mp_task_selftest_start(&sysinfo->kernel.tasks);
		return;
	}

	sysinfo->console = YES;
#ifdef SUPPORT_COMMON_TERM
	_sysinfo_term_start(sysinfo);
#else
	mp_serial_printk(&sysinfo->serial);
#endif

#ifdef MP_ARCH_RF_TEST
	mp_uart_host_wait(15000);
#ifdef SUPPORT_COMMON_DEVICE
	{
		static mp_device_subscribe_t _phySub;
		mp_device_registry_t *found;
		mp_device_phy_t *phy = NULL;
		mp_ret_t opened = FALSE;

		found = mp_device_registry_find(&sysinfo->kernel, MP_DEVICE_PHY, 0);
		if(found)
			phy = mp_device_phy(found, &_phySub);
		if(phy && mp_device_phy_open(phy) == TRUE && mp_device_phy_hold(phy) == TRUE
			&& mp_device_phy_tune(phy, 2437000000UL, 20000000UL) == TRUE)
			opened = TRUE;
		mp_printk("RF open %s", opened == TRUE ? "ok" : "failed");
		if(phy) {
			mp_device_phy_release(phy);
			mp_device_phy_close(phy);
			mp_printk("RF reopen %s", mp_device_phy_open(phy) == TRUE ? "ok" : "failed");
		} else
			mp_printk("RF reopen failed");
		mp_arch_rf_test(phy);
	}
#else
	mp_printk("RF open failed");
	mp_printk("RF reopen failed");
#endif
#endif

	/* after the console, so the driver messages show */
	_sysinfo_screen_open(sysinfo);
	_sysinfo_morse_start(sysinfo);
#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST
	_sysinfo_selftest_start(sysinfo);
#endif
	mp_task_selftest_start(&sysinfo->kernel.tasks);

	/* lets the arch publish its own sensors, such as the die temperature */
	mp_machine_state_set(&sysinfo->kernel);

	_sysinfo_accel_apply(sysinfo);

	/* one line per wake up: each write takes chunks from the memory pool.
	   the term splash already painted the logo, so start at the info rows */
	sysinfo->line = sysinfo->splash ? SYSINFO_BANNER_LINES : 0;
	sysinfo->task = mp_task_create(
		&sysinfo->kernel.tasks, "Sysinfo", _sysinfo_task, sysinfo,
		_sysinfo_ticks(SYSINFO_LINE_DELAY)
	);
}

#ifdef SUPPORT_COMMON_TERM

/* the identity block. Labels are 11 columns, the gutter included, so the
 * values line up. The splash shows them once; System/Info shows them live. */
#define SYSINFO_INFO_LINES 13

static void _sysinfo_term_info(sysinfo_t *sysinfo, char value[][256]) {
	mp_kernel_t *kernel = &sysinfo->kernel;
	const char *serial = mp_machine_serial();
	unsigned long speed = mp_clock_get_speed();

	snprintf(value[0], 256, "%s", kernel->version);
	snprintf(value[1], 256, "%s %s", __DATE__, __TIME__);
	if(serial)
		snprintf(value[2], 256, "%s, app %s, serial %s", PHI_BUILD_BOARD, PHI_BUILD_APP, serial);
	else
		snprintf(value[2], 256, "%s, app %s", PHI_BUILD_BOARD, PHI_BUILD_APP);
#if defined(__TI_COMPILER_VERSION__)
	snprintf(value[3], 256, "%s %lu.%lu.%lu", PHI_BUILD_TOOLCHAIN,
		(unsigned long)__TI_COMPILER_VERSION__ / 1000000,
		((unsigned long)__TI_COMPILER_VERSION__ / 1000) % 1000,
		(unsigned long)__TI_COMPILER_VERSION__ % 1000);
#elif defined(__GNUC__)
	snprintf(value[3], 256, "%s %s", PHI_BUILD_TOOLCHAIN, __VERSION__);
#else
	snprintf(value[3], 256, "%s", PHI_BUILD_TOOLCHAIN);
#endif
	snprintf(value[4], 256, "%s %s",
		kernel->mcuVendor ? kernel->mcuVendor : "unknown",
		kernel->mcuName ? kernel->mcuName : "unknown");
	snprintf(value[5], 256, "%s / %s / %s", PHI_BUILD_ARCH, PHI_BUILD_SERIES, PHI_BUILD_MCU);
	snprintf(value[6], 256, "%lu.%02lu MHz", speed / 1000000, (speed / 10000) % 100);
	snprintf(value[7], 256, "%lu KB flash, %lu KB RAM", PHI_BUILD_FLASH / 1024, PHI_BUILD_RAM / 1024);
	snprintf(value[8], 256, "%u tasks, %s, %u states, %u bytes",
		(unsigned int)MP_TASK_MAX, mp_task_scheduler_name(),
		(unsigned int)MP_STATE_MAX, (unsigned int)sizeof(*kernel));
	{
		int cores = mp_task_cpus();

		if(cores < 1)
			cores = 1;
		if(cores > 1)
			snprintf(value[9], 256, "%d cores, SMP", cores);
		else if(MP_CPUS > 1)
			snprintf(value[9], 256, "1 of %d cores", (int)MP_CPUS);
		else
			snprintf(value[9], 256, "1 core");
	}
#ifdef SUPPORT_COMMON_MEM
	snprintf(value[10], 256, "%u bytes, %u-byte chunks", (unsigned int)MP_MEM_SIZE, (unsigned int)MP_MEM_CHUNK);
#else
	snprintf(value[10], 256, "%s", "off");
#endif
	snprintf(value[11], 256, "%s", PHI_BUILD_MODULES);
	snprintf(value[12], 256, "%s", PHI_BUILD_DRIVERS);
}

static const char *_sysinfo_info_label[SYSINFO_INFO_LINES] = {
	"miniPhi    ", "Build      ", "Board      ", "Compiler   ",
	"CPU        ", "Target     ", "Clock      ", "Memory     ",
	"Kernel     ", "Cores      ", "Pool       ", "Modules    ",
	"Drivers    ",
};

/* a page title: bold white, a rule under it as wide as the body */
static void _sysinfo_page_title(mp_term_ui_t *ui, const char *title) {
	int width = mp_term_ui_cols(ui) - 4;

	mp_term_ui_put(ui, 1, 2, title, 7, 0, MP_TERM_UI_BOLD);
	if(width > 0)
		mp_term_ui_fill(ui, 2, 2, width, 'q', 4, 0, MP_TERM_UI_DEC);
}

static int _sysinfo_page_row(mp_term_ui_t *ui, int row, const char *label, const char *value, int fg) {
	int n;

	mp_term_ui_put(ui, row, 2, label, 6, 0, MP_TERM_UI_BOLD);
	n = mp_term_ui_put(ui, row, 14, value, fg, 0, 0);
	return(n > 0 ? n : 1);
}

static void _sysinfo_info_paint(mp_term_ui_t *ui, void *user) {
	static char value[SYSINFO_INFO_LINES][256];
	int i, row;

	_sysinfo_term_info(user, value);
	mp_term_ui_clear(ui);
	_sysinfo_page_title(ui, "System information");
	row = 4;
	for(i = 0; i < SYSINFO_INFO_LINES && row < mp_term_ui_rows(ui); i++)
		row += _sysinfo_page_row(ui, row, _sysinfo_info_label[i], value[i], 7);
	mp_term_ui_hint(ui, "Esc menu  Ctrl-L redraw");
}

static void _sysinfo_power_paint(mp_term_ui_t *ui, void *user) {
	sysinfo_t *sysinfo = user;
	mp_power_t *power = &sysinfo->kernel.power;
	mp_list_item_t *item;
	char line[96];
	unsigned long speed = mp_clock_get_speed();
	unsigned long hz = mp_clock_tick_hz();
	unsigned long sec = hz ? mp_clock_ticks() / hz : 0;
	int row, load, bar, width, i;

	mp_term_ui_clear(ui);
	_sysinfo_page_title(ui, "Power");
	_sysinfo_page_row(ui, 4, "Level", mp_power_name(mp_power_level(&sysinfo->kernel)), 3);
	snprintf(line, sizeof(line), "%lu.%02lu MHz", speed / 1000000, (speed / 10000) % 100);
	_sysinfo_page_row(ui, 5, "Clock", line, 7);

	/* the load as a gauge: reverse cells filled, DEC checkerboard empty */
	load = power->load > 100 ? 100 : power->load;
	snprintf(line, sizeof(line), "%3d %%", load);
	_sysinfo_page_row(ui, 6, "Load", line, load > 60 ? 1 : 2);
	width = mp_term_ui_cols(ui) - 24;
	if(width > 40)
		width = 40;
	if(width > 4) {
		bar = (load * width + 50) / 100;
		for(i = 0; i < width; i++)
			mp_term_ui_fill(ui, 6, 20 + i, 1, i < bar ? ' ' : 'a',
				i < bar ? 0 : 4, i < bar ? (load > 60 ? 1 : 2) : 0,
				i < bar ? 0 : MP_TERM_UI_DEC);
	}
	snprintf(line, sizeof(line), "%02lu:%02lu:%02lu",
		(sec / 3600UL) % 100UL, (sec / 60UL) % 60UL, sec % 60UL);
	_sysinfo_page_row(ui, 7, "Up", line, 7);

	mp_term_ui_put(ui, 9, 2, "Holds", 7, 0, MP_TERM_UI_BOLD);
	row = 10;
	for(item = power->holds.first; item && row < mp_term_ui_rows(ui); item = item->next) {
		mp_power_hold_t *hold = item->user;

		snprintf(line, sizeof(line), "%s%s",
			hold->nodeep == YES ? "no deep sleep" : mp_power_name(hold->floor),
			hold->expires == YES ? ", timed" : "");
		row += _sysinfo_page_row(ui, row, hold->who ? hold->who : "?", line, 7);
	}
	if(row == 10)
		mp_term_ui_put(ui, row, 2, "none, the governor follows the load", 4, 0, MP_TERM_UI_BOLD);
	mp_term_ui_hint(ui, "Esc menu  Ctrl-L redraw");
}

static const mp_term_page_t _sysinfo_info_page = {
	_sysinfo_info_paint, NULL, 1000, &_sysinfo,
};

static const mp_term_page_t _sysinfo_power_page = {
	_sysinfo_power_paint, NULL, 1000, &_sysinfo,
};

/* the logo and the identity block, copied into the shell before the task runs */
static void _sysinfo_term_lines(sysinfo_t *sysinfo, mp_term_banner_line_t *lines, int *n) {
	static char value[SYSINFO_INFO_LINES][256];
	int i, at = 0;

	for(i = 0; i < SYSINFO_BANNER_LINES; i++, at++) {
		lines[at].label = "";
		lines[at].value = _banner[i];
	}
	lines[at].label = "";
	lines[at].value = "";
	at++;
	_sysinfo_term_info(sysinfo, value);
	for(i = 0; i < SYSINFO_INFO_LINES; i++, at++) {
		lines[at].label = _sysinfo_info_label[i];
		lines[at].value = value[i];
	}
	*n = at;
}

static void _sysinfo_term_start(sysinfo_t *sysinfo) {
	mp_term_banner_line_t lines[SYSINFO_BANNER_LINES + 1 + SYSINFO_INFO_LINES];
	int n;

	if(sysinfo->term.task || sysinfo->console == NO)
		return;
	if(mp_term_ui_init(&sysinfo->ui, &sysinfo->term) != TRUE)
		return;
	mp_term_ui_place(&sysinfo->ui, PHI_BUILD_BOARD);
	mp_term_ui_add(&sysinfo->ui, "System/Info", &_sysinfo_info_page);
	mp_term_ui_add(&sysinfo->ui, "System/Power", &_sysinfo_power_page);
	_sysinfo_term_lines(sysinfo, lines, &n);
	if(mp_term_ui_start(&sysinfo->ui, &sysinfo->serial, lines, n) != TRUE)
		return;
	sysinfo->splash = YES;
}

static void _sysinfo_term_stop(sysinfo_t *sysinfo) {
	mp_term_ui_stop(&sysinfo->ui);
}

#endif

static void _sysinfo_op_unset(void *user) {
	sysinfo_t *sysinfo = user;

	if(sysinfo->task) {
		mp_task_destroy(sysinfo->task);
		sysinfo->task = NULL;
	}
#ifdef SUPPORT_COMMON_TERM
	_sysinfo_term_stop(sysinfo);
#endif

	_sysinfo_accel_run(sysinfo, NO);
	sysinfo->accelRunning = NO;
#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST
	if(sysinfo->selftest) {
		mp_task_destroy(sysinfo->selftest);
		sysinfo->selftest = NULL;
	}
#endif
	_sysinfo_screen_close(sysinfo);
	_sysinfo_morse_stop(sysinfo);
	_sysinfo_devices_shutdown(sysinfo);
#ifdef SUPPORT_COMMON_KFLASH
	if(sysinfo->accelListened == YES) {
		mp_kflash_unlisten(&sysinfo->kernel, &sysinfo->accelVar);
		sysinfo->accelListened = NO;
	}
	if(sysinfo->hibernateListened == YES) {
		mp_kflash_unlisten(&sysinfo->kernel, &sysinfo->hibernateVar);
		sysinfo->hibernateListened = NO;
	}
#endif

	if(sysinfo->console == NO)
		return;

	mp_machine_state_unset(&sysinfo->kernel);
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

#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST
	/* the series prints next; the uptime report follows it */
	if(sysinfo->selftest) {
		mp_task_delay(task, 1);
		return;
	}
#endif

	task->delay = _sysinfo_ticks(SYSINFO_PERIOD);
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
			{
				const char *serial = mp_machine_serial();

				if(serial)
					mp_printk("Board      %s, app %s, serial %s", PHI_BUILD_BOARD, PHI_BUILD_APP, serial);
				else
					mp_printk("Board      %s, app %s", PHI_BUILD_BOARD, PHI_BUILD_APP);
			}
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
			mp_printk("Kernel     %u tasks, %s, %u states, %u bytes",
				(unsigned int)MP_TASK_MAX, mp_task_scheduler_name(),
				(unsigned int)MP_STATE_MAX, (unsigned int)sizeof(*kernel));
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

#ifdef SUPPORT_COMMON_DEVICE
/* same digits as printf %.Nf, without the malloc that resets the chip when it fails */
static void _sysinfo_fixed(char *dst, int cap, float value, int decimals) {
	int scale = 1;
	int n, whole, frac;
	int neg;

	if(!dst || cap < 2)
		return;
	if(decimals < 0)
		decimals = 0;
	if(decimals > 3)
		decimals = 3;
	for(n = 0; n < decimals; n++)
		scale *= 10;
	neg = value < 0.0f;
	if(neg)
		value = -value;
	whole = (int)value;
	frac = (int)((value - (float)whole) * (float)scale + 0.5f);
	if(frac >= scale) {
		frac -= scale;
		whole++;
	}
	if(decimals == 0)
		snprintf(dst, (size_t)cap, "%s%d", neg ? "-" : "", whole);
	else if(decimals == 1)
		snprintf(dst, (size_t)cap, "%s%d.%d", neg ? "-" : "", whole, frac);
	else if(decimals == 2)
		snprintf(dst, (size_t)cap, "%s%d.%02d", neg ? "-" : "", whole, frac);
	else
		snprintf(dst, (size_t)cap, "%s%d.%03d", neg ? "-" : "", whole, frac);
}
#endif

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

#ifdef SUPPORT_COMMON_USB
	{
		int n;

		for(n = 0; n < mp_usb_count(); n++) {
			mp_usb_port_t *port = mp_usb_at(n);

			mp_printk("  usb %s lock %s, %s, %s%s%s",
				port->name, mp_usb_lock_name(port->fixed),
				mp_usb_role_name(port->role),
				port->fault == YES ? "fault" : mp_usb_link_name(port->link),
				port->speed == MP_USB_SPEED_NONE || port->fault == YES ? "" : ", ",
				port->speed == MP_USB_SPEED_NONE || port->fault == YES ? "" : mp_usb_speed_name(port->speed));
		}
	}
#endif

#ifdef SUPPORT_COMMON_DEVICE
	{
		mp_device_registry_t *sensor = NULL;
		float v[3];

		while((sensor = mp_device_measure_next(kernel, sensor)) != NULL) {
			char a[16], b[16], c[16];

			if(mp_device_measure_read(sensor->instance, v, NULL) != TRUE)
				mp_printk("  %s: --", sensor->address);
			else if(mp_device_measure_axes(sensor->class) == 3) {
				_sysinfo_fixed(a, (int)sizeof(a), v[0], 2);
				_sysinfo_fixed(b, (int)sizeof(b), v[1], 2);
				_sysinfo_fixed(c, (int)sizeof(c), v[2], 2);
				mp_printk("  %s: %s %s %s %s", sensor->address, a, b, c, mp_device_measure_unit(sensor->class));
			}
			else {
				_sysinfo_fixed(a, (int)sizeof(a), v[0], 3);
				mp_printk("  %s: %s %s", sensor->address, a, mp_device_measure_unit(sensor->class));
			}
		}
	}
#endif
}
