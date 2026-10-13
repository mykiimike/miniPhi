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
@defgroup mpArchSTM32 ST STM32

@ingroup mpArch

@brief ST STM32 implementation

*/


#ifndef _HAVE_STM32_INTERNAL_H
	#define _HAVE_STM32_INTERNAL_H

	#define FLOAT_DIV(a, b) (float)((float)a/(float)b)

	#include <stdint.h>
	#include <stdbool.h>

	/* registers of the selected series */
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
	#include "fsmc.h"
	#include "adc.h"

	typedef struct mp_arch_stm32_s mp_arch_t;

	struct mp_arch_stm32_s {
		/** SYSCLK in Hz once mp_clock_init() is done */
		unsigned long sysclk;

		/** YES when the HSE crystal started */
		mp_bool_t hse;

		mp_adc_t internalTemp;
		mp_adc_t internalVref;
		struct mp_sensor_s *sensorMCU;
		struct mp_sensor_s *sensorVDDA;

		/** last VDDA reading, scales the temperature conversion */
		float vdda;

		char mcuName[64];
	};

	mp_ret_t mp_machine_init(mp_kernel_t *kernel);
	mp_ret_t mp_machine_fini(mp_kernel_t *kernel);
	void mp_machine_reset(void) __attribute__((noreturn));

	/* bus clocks, valid after mp_clock_init() */
	unsigned long mp_clock_hclk(void);
	unsigned long mp_clock_pclk1(void);
	unsigned long mp_clock_pclk2(void);

#endif
