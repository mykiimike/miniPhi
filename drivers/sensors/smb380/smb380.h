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

#ifndef _HAVE_MP_DRV_SMB380_H
	#define _HAVE_MP_DRV_SMB380_H

	/**
	 * Bosch SMB380 triaxial accelerometer on I2C, through regMaster.
	 * It publishes one 3-axis sensor in g and one temperature in °C.
	 * The interrupt pin is claimed when the board wires it; samples
	 * are polled, so the pin does not have to be connected.
	 *
	 * options: the i2c bus ones, address (0x38 or 0x39), range (2, 4
	 * or 8 g), bandwidth (25 to 1500 Hz), period in ms, int (optional).
	 */

	typedef struct mp_drv_smb380_s mp_drv_smb380_t;

	struct mp_drv_smb380_s {
		mp_kernel_t *kernel;
		mp_i2c_t i2c;
		mp_regMaster_t regMaster;
		mp_gpio_port_t *irq;
		mp_task_t *task;

		mp_device_driver_t binding;
		mp_device_registry_t accelDev;
		mp_device_registry_t tempDev;
		mp_device_measure_t accel;
		mp_device_measure_t temperature;

		unsigned char address;
		unsigned char range;
		unsigned char bandwidth;
		unsigned char chipId;
		unsigned char setup;
		unsigned char raw[7];
		int countsPerG;
		unsigned long period;
		mp_bool_t busy;
		/** the screen is off: the sample task waits and the chip sleeps */
		mp_bool_t asleep;
		/** last value written to the control register */
		unsigned char ctrl;
	};

	mp_ret_t mp_drv_smb380_init(mp_kernel_t *kernel, mp_drv_smb380_t *smb, mp_options_t *options, char *who);
	void mp_drv_smb380_fini(mp_drv_smb380_t *smb);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_ACCELEROMETER */
	extern const mp_device_ops_t mp_drv_smb380_device;
#endif

#endif
