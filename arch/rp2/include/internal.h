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

/**
@defgroup mpArchRP2350 Raspberry Pi RP2350

@ingroup mpArch

@brief RP2350 Cortex-M33 implementation

*/


#ifndef _HAVE_RP2350_INTERNAL_H
	#define _HAVE_RP2350_INTERNAL_H

	#define FLOAT_DIV(a, b) (float)((float)a/(float)b)

	#include <stdint.h>
	#include <stdbool.h>

	#include "series.h"

	#include "flash.h"
	#include "gate.h"
	#include "interrupt.h"
	#include "gpio.h"
	#include "timer.h"
	#include "clock.h"
	#include "i2c.h"
	#include "uart.h"
	#include "spi.h"
	#include "adc.h"
	#include "dma.h"
	#include "pio.h"

	typedef struct mp_arch_rp2350_s mp_arch_t;

	struct mp_arch_rp2350_s {
		/** clk_sys in Hz once mp_clock_init() is done */
		unsigned long sysclk;

		/** YES when the 12 MHz crystal is the reference */
		mp_bool_t hse;

		char mcuName[64];
	};

	mp_ret_t mp_machine_init(mp_kernel_t *kernel);
	mp_ret_t mp_machine_fini(mp_kernel_t *kernel);
	void mp_machine_reset(void) __attribute__((noreturn));

	unsigned long mp_clock_hclk(void);
	unsigned long mp_clock_peri(void);

#endif
