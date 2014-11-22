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

#ifndef _MP_H
	#define MP_H

	#include <config.h>

	#define NULL (void *)0

	#define TRUE 0
	#define FALSE -1

	#define YES 1
	#define NO 0
	#define ON 1
	#define OFF 0

	typedef signed char mp_ret_t;
	typedef signed char mp_bool_t;

	#include "common/list.h"
	#include "common/task.h"

	typedef struct mp_kernel_s mp_kernel_t;
	typedef void (*mp_kernel_onBoot_t)(void *user);

	typedef struct mp_options_s mp_options_t;

	struct mp_options_s {
		char *key;
		char *value;
	};

	#ifdef __MSP430__
		#include <msp430.h>
		#include <string.h>
		#include <stdio.h>
		#include <stdlib.h>
		#include <msp430/internal.h>
	#endif

	#define MP_KERNEL_KPANIC 0
	#define MP_KERNEL_BOOT   1
	#define MP_KERNEL_RES01  2
	#define MP_KERNEL_RES02  3
	#define MP_KERNEL_RES03  4
	#define MP_KERNEL_RES04  5
	#define MP_KERNEL_RES05  6
	#define MP_KERNEL_RES06  7
	#define MP_KERNEL_RES07  8
	#define MP_KERNEL_RES08  9
	#define MP_KERNEL_RES09  10
	#define MP_KERNEL_RES10  11

	#include "common/mem.h"
	#include "common/state.h"
	#include "common/serial.h"
	#include "common/pinout.h"

	#include "drivers/button.h"
	#include "drivers/led.h"

	#include "drivers/lcd/nokia3310.h"

	#define MP_KERNEL_VERSION "1.0.0"

	struct mp_kernel_s {
		/** Vendor name  */
		char *mcuVendor;

		/** Microchip name  */
		char *mcuName;

		/** Kernel version  */
		char *version;

		/** Kernel states */
		mp_state_handler_t states;

		/** Kernel tasks */
		mp_task_handler_t tasks;

		/** Kernel on boot state */
		mp_kernel_onBoot_t onBoot;

		/** User pointer for onBoot */
		void *onBootUser;

#ifdef __MSP430__
		mp_adc_t internalTemp;
#endif

	};

	static inline char *mp_options_get(mp_options_t *options, char *key) {
		while(options->key != NULL && options->value != NULL) {
			if(strcmp(key, options->key) == 0)
				return(options->value);
			options++;
		}
		return(NULL);

	}

	static inline int mp_options_cmp(char *a, char *b) {
		return(strcmp(a, b));
	}

	void mp_kernel_init(mp_kernel_t *kernel, mp_kernel_onBoot_t onBoot, void *user);
	void mp_kernel_fini(mp_kernel_t *kernel);
	void mp_kernel_state(mp_kernel_t *kernel, char number);
	void mp_kernel_loop(mp_kernel_t *kernel);

#endif
