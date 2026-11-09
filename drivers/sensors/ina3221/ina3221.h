/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                   *
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

#ifndef _HAVE_MP_DRV_INA3221_H
	#define _HAVE_MP_DRV_INA3221_H

	/**
	 * TI INA3221, three-channel current and bus monitor on I2C.
	 * The driver opens a shared mp_i2c_bus_t on the pins the board names.
	 * Address 0x40 when A0 is grounded, through 0x43. Each enabled channel
	 * publishes a voltage (V), a current (A) and a power (W). The shunt
	 * voltage LSB is 40 µV and the bus voltage LSB is 8 mV; current is the
	 * shunt voltage divided by the shunt named in milliohms. Channel 3 is
	 * the third pair of registers. A missing shunt option is 50 mOhm.
	 *
	 * options: sda, clk, address (0x40 when missing), shunt (milliohms),
	 * channels (digits 1, 2 and 3; missing enables all three),
	 * label1, label2, label3.
	 */

	#define MP_DRV_INA3221_CHANNELS 3

	#include <i2c/i2c.h>

	typedef struct mp_drv_ina3221_s mp_drv_ina3221_t;

	struct mp_drv_ina3221_s {
		mp_kernel_t *kernel;
		mp_i2c_bus_t bus;
		unsigned char address;
		/** bit 0 is channel 1 */
		unsigned char mask;
		/** enabled channels, in order, as a 0-based hardware index */
		unsigned char hw[MP_DRV_INA3221_CHANNELS];
		unsigned char count;
		/** amperes per shunt volt, 1000 / milliohms */
		float ampsPerVolt;
		mp_bool_t watched;
		mp_bool_t running;
		mp_task_t *task;
		char label[MP_DRV_INA3221_CHANNELS][12];
#ifdef SUPPORT_COMMON_DEVICE
		mp_device_driver_t binding;
		mp_device_registry_t volt[MP_DRV_INA3221_CHANNELS];
		mp_device_registry_t curr[MP_DRV_INA3221_CHANNELS];
		mp_device_registry_t power[MP_DRV_INA3221_CHANNELS];
		mp_device_measure_t voltM[MP_DRV_INA3221_CHANNELS];
		mp_device_measure_t currM[MP_DRV_INA3221_CHANNELS];
		mp_device_measure_t powerM[MP_DRV_INA3221_CHANNELS];
#endif
	};

	mp_ret_t mp_drv_ina3221_init(mp_kernel_t *kernel, mp_drv_ina3221_t *meter, mp_options_t *options, char *who);
	void mp_drv_ina3221_fini(mp_drv_ina3221_t *meter);

#ifdef SUPPORT_COMMON_DEVICE
	extern const mp_device_ops_t mp_drv_ina3221_device;
#endif

#endif
