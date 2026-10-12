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

/**
@defgroup mpArchTiMSP430 Texas Instrument MSP430

@ingroup mpArch

@brief Ti MSP430 implementation

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

*/


#ifndef _HAVE_MSP430_INTERNAL_H
	#define _HAVE_MSP430_INTERNAL_H

	#define FLOAT_DIV(a, b) (float)((float)a/(float)b)

	#include <msp430.h>
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

	typedef struct mp_arch_msp430_s mp_arch_msp430_t;

	struct mp_arch_msp430_s {
		mp_clock_sched_t scheduler;
	};

	typedef unsigned char uint8_t;
	typedef signed char int8_t;

	typedef unsigned int uint16_t;
	typedef signed int int16_t;

	typedef unsigned long uint32_t;
	typedef signed long int32_t;

	typedef unsigned char bool;

	mp_ret_t mp_machine_init(mp_kernel_t *kernel);
	mp_ret_t mp_machine_fini(mp_kernel_t *kernel);
	mp_ret_t mp_machine_loop(mp_kernel_t *kernel);
	void mp_machine_state_set(mp_kernel_t *kernel);
	void mp_machine_state_unset(mp_kernel_t *kernel);

#endif
