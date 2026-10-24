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

#ifndef _HAVE_STM32_CLOCK_H
	#define _HAVE_STM32_CLOCK_H

	/* SysTick rate: one tick is one millisecond */
	#define STM32_TICK_RATE_HZ 1000UL

	typedef enum {
		MHZ8_t,
		MHZ16_t,
		MHZ24_t,
		MHZ32_t,
		MHZ40_t,
		MHZ48_t,
		MHZ56_t,
		MHZ64_t,
		MHZ72_t,
		/* appended: the F1 clock table indexes the values above */
		MHZ12_t,
		MHZ84_t,
		MHZ96_t,
		MHZ120_t,
		MHZ144_t,
		MHZ168_t,
	} mp_clock_freq_t;

	mp_ret_t mp_clock_init(mp_kernel_t *kernel);
	mp_ret_t mp_clock_fini(mp_kernel_t *kernel);
	void mp_clock_reset(mp_kernel_t *kernel);

	unsigned long mp_clock_ticks();
	unsigned long mp_clock_tick_hz();
	unsigned long mp_clock_get_speed();
	const char *mp_clock_name(mp_clock_freq_t clock);

	/* waits, in ticks (ms) with WFI between them, and busy in nanoseconds */
	void mp_clock_delay(int delay);
	void mp_clock_nanoDelay(unsigned long delay);

#endif
