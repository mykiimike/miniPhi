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
@defgroup mpArchESP32 Espressif ESP32

@ingroup mpArch

@brief ESP32 family. The C6 series is the HP RISC-V core, the S3 series is the Xtensa LX7.

*/


#ifndef _HAVE_ESP32_INTERNAL_H
	#define _HAVE_ESP32_INTERNAL_H

	#define FLOAT_DIV(a, b) (float)((float)a/(float)b)

	#include <stdint.h>
	#include <stdbool.h>

	#include "series.h"
	#include "gate.h"
	#include "interrupt.h"
	#include "gpio.h"
	#include "timer.h"
	#include "clock.h"
	#include "i2c.h"
	#include "uart.h"
	#include "spi.h"
	#ifdef ESP32S3
		#include "adc.h"
	#endif

	typedef struct mp_arch_esp32_s mp_arch_t;

	struct mp_arch_esp32_s {
		/** CPU clock in Hz once mp_clock_init() is done */
		unsigned long hz;

		char mcuName[64];
	};

	mp_ret_t mp_machine_init(mp_kernel_t *kernel);
	mp_ret_t mp_machine_fini(mp_kernel_t *kernel);
	void mp_machine_reset(void) __attribute__((noreturn));

#endif
