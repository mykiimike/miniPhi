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
	#define _MP_H

	#ifndef MP_MY_CONFIG
		#include <config.h>
	#endif

	#define NULL (void *)0

	#define TRUE 1
	#define FALSE 0

	#define YES TRUE
	#define NO FALSE

	#define ON TRUE
	#define OFF FALSE

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
		#include <math.h>
		#include <string.h>
		#include <stdio.h>
		#include <stdlib.h>
		#include <stdarg.h>
		#include <msp430/internal.h>

		#define PI (355.0 / 113.0)
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

	#include "common/utils.h"
	#include "common/mem.h"
	#include "common/circular.h"
	#include "common/state.h"
	#include "common/serial.h"
	#include "common/pinout.h"
	#include "common/printk.h"
	#include "common/quaternion.h"
	#include "common/sensor.h"

	/* Bluetooth */
	//#include "bluetooth/internal.h"

	#include "drivers/button.h"
	#include "drivers/led.h"

	#include "drivers/lcd/nokia3310.h"

	#include "drivers/sensors/LSM9DS0.h"

	#define MP_KERNEL_VERSION "1.0.2"

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

		/** Sensors handler */
		mp_sensor_handler_t sensors;

		/** Kernel on boot state */
		mp_kernel_onBoot_t onBoot;

		/** User pointer for onBoot */
		void *onBootUser;

#ifdef __MSP430__
		mp_adc_t internalTemp;
		mp_sensor_t *sensorMCU;
#endif

	};

	char *mp_options_get(mp_options_t *options, char *key);
	mp_ret_t mp_options_cmp(char *a, char *b);

	void mp_kernel_init(mp_kernel_t *kernel, mp_kernel_onBoot_t onBoot, void *user);
	void mp_kernel_fini(mp_kernel_t *kernel);
	void mp_kernel_state(mp_kernel_t *kernel, char number);
	void mp_kernel_loop(mp_kernel_t *kernel);
	void mp_kernel_panic(mp_kernel_t *kernel, int error);

	/* error codes for kpanic */
	#define KPANIC_MEM_SIZE 1
	#define KPANIC_MEM_OOM  2
#endif
