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

#ifndef _HAVE_RP2350_CLOCK_H
	#define _HAVE_RP2350_CLOCK_H

	/* SysTick rate: one tick is one millisecond */
	#define RP2350_TICK_RATE_HZ 1000UL
	#define RP2350_XOSC_HZ      12000000UL
	#define RP2350_SYS_HZ       150000000UL

	typedef enum {
		MHZ12_t,
		MHZ48_t,
		MHZ150_t,
	} mp_clock_freq_t;

	mp_ret_t mp_clock_init(mp_kernel_t *kernel);
	mp_ret_t mp_clock_fini(mp_kernel_t *kernel);
	void mp_clock_reset(mp_kernel_t *kernel);

	unsigned long mp_clock_ticks();
	unsigned long mp_clock_tick_hz();
	unsigned long mp_clock_get_speed();
	const char *mp_clock_name(mp_clock_freq_t clock);

	/* the USB PLL is not started by this port */
	mp_bool_t mp_clock_usb48(void);

	/* Reload this core's SysTick from the current tick period. */
	void mp_clock_core_tick(void);
	/* clk_sys was below 48 MHz and the USB controller just woke. */
	void mp_clock_usb_floor(void);

	void mp_clock_delay(int delay);
	void mp_clock_nanoDelay(unsigned long delay);

	unsigned long mp_clock_hclk(void);
	unsigned long mp_clock_peri(void);

#endif
