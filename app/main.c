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
/* extra silent passes after the printed one, one check per wake.
 * A KEM is much slower than a hash, so it repeats less often. */
#define SYSINFO_SELFTEST_PASSES 400
#define SYSINFO_SELFTEST_KEM_PASSES 1

static int _sysinfo_selftest_passes(const mp_crypto_selftest_t *test) {
	const char *family;

	if(!test || !test->family)
		return(SYSINFO_SELFTEST_PASSES);
	family = test->family;
	if(family[0] == 'k' && family[1] == 'e' && family[2] == 'm' && family[3] == '\0')
		return(SYSINFO_SELFTEST_KEM_PASSES);
	return(SYSINFO_SELFTEST_PASSES);
}

static MP_TASK(_sysinfo_selftest) {
	sysinfo_t *sysinfo = task->user;
	const mp_crypto_selftest_t *test;
	unsigned long t0;
	unsigned long us;
	mp_bool_t ok;

	if(task->signal == MP_TASK_SIG_STOP) {
		mp_power_release(&sysinfo->kernel, &sysinfo->selftestPower);
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

	/* one wake so the governor applies the floor before the first check */
	if(sysinfo->selftestPower.held == NO) {
		mp_power_hold(&sysinfo->kernel, &sysinfo->selftestPower, MP_POWER_HIGH, "Selftest");
		mp_task_delay(task, 1);
		return;
	}

	if(sysinfo->selftestIndex >= mp_crypto_selftest_count()) {
		int count = mp_crypto_selftest_count();
		int extra = sysinfo->selftestIndex - count;

		if(extra == 0) {
			mp_printk("SELFTEST: %lu MHz, %lu us",
				(mp_clock_get_speed() + 500000UL) / 1000000UL, sysinfo->selftestUs);
			mp_printk("SELFTEST: burn %lu passes, kem %lu%s",
				(unsigned long)SYSINFO_SELFTEST_PASSES,
				(unsigned long)SYSINFO_SELFTEST_KEM_PASSES,
				sysinfo->morse ? "" : ", Morse off");
		}
		if(extra >= count * SYSINFO_SELFTEST_PASSES) {
			mp_printk("SELFTEST: burn done");
			mp_power_release(&sysinfo->kernel, &sysinfo->selftestPower);
			sysinfo->selftest = NULL;
			task->signal = MP_TASK_SIG_DEAD;
			return;
		}
		test = mp_crypto_selftest_at(extra % count);
		if((extra / count) >= _sysinfo_selftest_passes(test)) {
			sysinfo->selftestIndex++;
			mp_task_delay(task, 0);
			return;
		}
		if(test && test->run && test->run() != YES)
			mp_printk("SELFTEST: %s failed", test->label);
		sysinfo->selftestIndex++;
		mp_task_delay(task, 1);
		return;
	}

	if(sysinfo->selftestIndex == 0)
		mp_printk("SELFTEST: %s %lu MHz",
			mp_power_name(mp_power_level(&sysinfo->kernel)),
			(mp_clock_get_speed() + 500000UL) / 1000000UL);

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
	mp_printk_set(_sysinfo_printk, sysinfo);

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
#ifdef ESP32C6
		{
			const unsigned char *addr;
			mp_ret_t mp_c6_mac_up(unsigned long hz);
			void mp_c6_mac_down(void);
			const unsigned char *mp_c6_mac_addr(void);

			if(mp_c6_mac_up(2437000000UL) == TRUE && (addr = mp_c6_mac_addr()) != NULL) {
				mp_printk("MAC up ok, %02x:%02x:%02x:%02x:%02x:%02x",
					addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
#if defined(MP_ARCH_RF_TONE)
				{
					void mp_c6_rf_tone(int on);
					unsigned long t0;

					mp_c6_rf_tone(1);
					mp_printk("RF tone on");
					t0 = mp_clock_ticks();
					while((mp_clock_ticks() - t0) < 8000UL)
						;
					mp_c6_rf_tone(0);
					mp_printk("RF tone off");
				}
#elif defined(MP_ARCH_RF_SEND)
				{
					mp_ret_t mp_c6_mac_tx(void);
					mp_ret_t mp_c6_mac_rx(void);
					unsigned long mp_c6_mac_tx_word(void);
					unsigned long mp_c6_mac_tx_pwr(void);
					unsigned long mp_c6_mac_tx_ctrl(void);
					unsigned long mp_c6_mac_tx_state(void);
					int mp_c6_rf_cca(void);
					unsigned long t0;
					unsigned long step;
					unsigned long kicks;

					mp_printk("RF rx %s", mp_c6_mac_rx() == TRUE ? "armed" : "failed");
					if(mp_c6_mac_tx() != TRUE)
						mp_printk("RF tx failed");
					else {
						mp_printk("RF tx %08lx pwr %08lx cca %d ctrl %08lx st %08lx",
							mp_c6_mac_tx_word(), mp_c6_mac_tx_pwr(),
							mp_c6_rf_cca(), mp_c6_mac_tx_ctrl(),
							mp_c6_mac_tx_state());
						t0 = mp_clock_ticks();
						while((mp_clock_ticks() - t0) < 500UL)
							;
						mp_printk("RF tx wait %08lx", mp_c6_mac_tx_word());
					}
					kicks = 0;
					t0 = mp_clock_ticks();
					while((mp_clock_ticks() - t0) < 14000UL) {
						if(mp_c6_mac_tx() != TRUE)
							break;
						kicks++;
						step = mp_clock_ticks();
						while((mp_clock_ticks() - step) < 500UL)
							;
					}
					mp_printk("RF tx off %lu %08lx", kicks, mp_c6_mac_tx_word());
				}
#elif defined(MP_ARCH_RF_LISTEN)
				{
					int mp_c6_rf_rssi(void);
					mp_ret_t mp_c6_mac_rx(void);
					unsigned long mp_c6_mac_rx_word(void);
					const unsigned char *mp_c6_mac_rx_buf(void);
					const unsigned char *buf;
					int rssi, rssi_hi;
					int n;
					unsigned long t0, step;
					unsigned long word;
					unsigned char b[8];
					unsigned i;

					mp_printk("RF rx %s", mp_c6_mac_rx() == TRUE ? "armed" : "failed");
					mp_device_phy_tune(phy, 2437000000UL, 20000000UL);
					for(n = 0; n < 12; n++) {
						rssi_hi = -32768;
						t0 = mp_clock_ticks();
						while((mp_clock_ticks() - t0) < 1000UL) {
							rssi = mp_c6_rf_rssi();
							if(rssi > rssi_hi)
								rssi_hi = rssi;
							step = mp_clock_ticks();
							while((mp_clock_ticks() - step) < 100UL)
								;
						}
					word = mp_c6_mac_rx_word();
					buf = mp_c6_mac_rx_buf();
					for(i = 0; i < 8; i++)
						b[i] = buf ? buf[i] : 0;
					/* C6 MAC RX hardware state, all read in the assembly:
					 * 0x600A4088 next desc (hal_mac_rx_read_rxdscrnext),
					 * 0x600A408C last desc (hal_mac_rx_read_rxdscrlast),
					 * 0x600A4E24/4E28 RX end info (hal_mac_rx_get_end_info),
					 * 0x600A4C48 event, bit 2 = RX done (hal_mac_interrupt_get_event).
					 * The SRAM descriptor alone never tells us whether the
					 * hardware accepted a frame; these do. */
					{
						volatile unsigned long *r = (volatile unsigned long *)0x600A4000UL;
						mp_printk("RF hear rssi %d rx %08lx %02x%02x%02x%02x%02x%02x%02x%02x nx %08lx la %08lx st %08lx ev %08lx",
							rssi_hi, word,
							b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
							r[0x88/4], r[0x8C/4], r[0xA8/4], r[0xC48/4]);
					}
					}
				}
#endif
			} else
				mp_printk("MAC up failed");
			mp_c6_mac_down();
		}
#endif
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

#ifdef ESP32C6
	mp_uart_write_sync(buffer, size);
#else
	mp_serial_write(&sysinfo->serial, buffer, size);
#endif
}
