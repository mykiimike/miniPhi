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
 * Shared by main.c (boot and the console report), screen.c (the TUI)
 * and morse.c (the LEDs).
 */

#ifndef _MINIPHI_APP_SYSINFO_H
#define _MINIPHI_APP_SYSINFO_H

#include <mp.h>
#include <pinout.h>

#define SYSINFO_OP MP_KERNEL_RES01

typedef struct sysinfo_s sysinfo_t;

struct sysinfo_s {
	mp_kernel_t kernel;

	mp_serial_t serial;
	mp_bool_t console;

	mp_task_t *task;
	int line;

#if defined(SUPPORT_DRV_SMB380) && defined(PHI_SMB380_FIRST)
	mp_drv_smb380_t accel;
	mp_bool_t accelReady;
#endif

#if defined(SUPPORT_DRV_LED) && defined(PHI_LED_FIRST)
	struct {
		mp_drv_led_t led;
		mp_bool_t ready;
		const char *pattern;
		int step;
	} leds[2];
	mp_task_t *morse;
#endif

#if defined(SUPPORT_DRV_LCD_ILI932X) && defined(PHI_ILI932X_LCD)
	struct {
		mp_drv_ili932x_t lcd;
		mp_tui_t tui;
		mp_bool_t ready;
		mp_task_t *task;
#ifdef SUPPORT_COMMON_INPUT
		mp_input_t keys;
#endif
#if defined(SUPPORT_DRV_TOUCH_RESISTIVE) && defined(PHI_RESISTIVE_TOUCH)
		mp_drv_resistive_t touch;
		mp_bool_t touchReady;
#endif
		int systemReady;
		mp_tui_label_t heading;
		mp_tui_label_t copyright;
		mp_tui_label_t license;
		mp_tui_window_t system;
		mp_tui_window_t status;
		mp_tui_label_t board;
		mp_tui_label_t cpu;
		mp_tui_label_t clock;
		mp_tui_label_t memory;
		mp_tui_label_t build;
		mp_tui_label_t target;
		mp_tui_label_t uptime;
		mp_tui_label_t tasks;
		mp_tui_bar_t taskBar;
		mp_tui_label_t sensors[3];
	} screen;
#endif
};

#if defined(SUPPORT_DRV_LED) && defined(PHI_LED_FIRST)
#define SYSINFO_MORSE
#endif

#if defined(SUPPORT_DRV_LCD_ILI932X) && defined(PHI_ILI932X_LCD)
#define SYSINFO_SCREEN
#endif

/* delays are milliseconds and follow mp_clock_tick_hz() */
static inline unsigned long _sysinfo_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);

	ticks = (ms / 1000) * hz + ((ms % 1000) * hz) / 1000;
	return(ticks ? ticks : 1);
}

void _sysinfo_morse_start(sysinfo_t *sysinfo);
void _sysinfo_morse_stop(sysinfo_t *sysinfo);
void _sysinfo_screen_open(sysinfo_t *sysinfo);
void _sysinfo_screen_close(sysinfo_t *sysinfo);

#endif
