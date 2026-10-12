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
@defgroup mpCommon miniPhi Base of system

@brief Implementation of features to build your firmware
*/


/**
@defgroup mpDriver miniPhi Drivers

@brief Implementation of different chips on miniPhi
*/

/**
@defgroup mpArch Architecture dependant implementation

@brief Differents processor architecture
*/


#ifndef _MP_H
	#define _MP_H

	#ifndef MP_MY_CONFIG
		#include <config.h>
	#endif

	#define NULL (void *)0

	#define TRUE 1
	#define FALSE 0

	#define true 1
	#define false 0

	#define YES TRUE
	#define NO FALSE

	#define ON TRUE
	#define OFF FALSE

	typedef signed char mp_ret_t;
	typedef signed char mp_bool_t;

	typedef struct mp_kernel_s mp_kernel_t;
	typedef void (*mp_kernel_onBoot_t)(void *user);

	typedef struct mp_options_s mp_options_t;

	struct mp_options_s {
		char *key;
		char *value;
	};

	#include <list.h>
	#include <task/task.h>

	#ifdef __MSP430__
		#include <msp430.h>
		#include <math.h>
		#include <string.h>
		#include <stdio.h>
		#include <stdlib.h>
		#include <stdarg.h>
		#include <arch/internal.h>

		#define PI (355.0 / 113.0)
	#endif

	/* Every arch implements these. The headers above may declare them too. */
	unsigned long mp_clock_ticks();
	unsigned long mp_clock_tick_hz();
	unsigned long mp_clock_get_speed();
	void mp_machine_state_set(mp_kernel_t *kernel);
	void mp_machine_state_unset(mp_kernel_t *kernel);

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

	/* build/include only holds the headers of the modules and drivers selected */
	#include <utils.h>
	#ifdef SUPPORT_COMMON_MEM
		#include <mem/mem.h>
	#endif
	#ifdef SUPPORT_COMMON_CIRCULAR
		#include <circular/circular.h>
	#endif
	#include <state.h>
	#ifdef SUPPORT_COMMON_HCI
		#include <hci/hci.h>
	#endif
	#ifdef SUPPORT_COMMON_SERIAL
		#include <serial/serial.h>
	#endif
	#ifdef SUPPORT_COMMON_PINOUT
		#include <pinout/pinout.h>
	#endif
	#include <printk.h>
	#ifdef SUPPORT_COMMON_QUATERNION
		#include <quaternion/quaternion.h>
	#endif
	#ifdef SUPPORT_COMMON_SENSOR
		#include <sensor/sensor.h>
	#endif
	#ifdef SUPPORT_COMMON_REGMASTER
		#include <regmaster/regMaster.h>
	#endif
	#ifdef SUPPORT_COMMON_KALMAN
		#include <kalman/kalman.h>
	#endif

	#ifdef SUPPORT_DRV_BUTTON
		#include <button/button.h>
	#endif
	#ifdef SUPPORT_DRV_LED
		#include <led/led.h>
	#endif
	#ifdef SUPPORT_DRV_LCD_NOKIA3310
		#include <nokia3310/nokia3310.h>
	#endif
	#ifdef SUPPORT_DRV_NRF8001
		#include <nrf8001/nRF8001.h>
	#endif
	#ifdef SUPPORT_DRV_LSM9DS0
		#include <lsm9ds0/LSM9DS0.h>
	#endif
	#ifdef SUPPORT_DRV_TMP006
		#include <tmp006/TMP006.h>
	#endif
	#ifdef SUPPORT_DRV_MPL3115A2
		#include <mpl3115a2/MPL3115A2.h>
	#endif
	#ifdef SUPPORT_DRV_ADS1115
		#include <ads1115/ADS1115.h>
	#endif
	#ifdef SUPPORT_DRV_INA219
		#include <ina219/INA219.h>
	#endif
	#ifdef SUPPORT_DRV_ADS124X
		#include <ads124x/ADS124x.h>
	#endif

	#define MP_KERNEL_VERSION "1.0.5"

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

		/** Internal tick timer */
		mp_timer_t tickTimer;

		/** Kernel on boot state */
		mp_kernel_onBoot_t onBoot;

		/** User pointer for onBoot */
		void *onBootUser;

#ifdef __MSP430__
		mp_adc_t internalTemp;
		mp_sensor_t *sensorMCU;

		mp_arch_msp430_t msp430;
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
