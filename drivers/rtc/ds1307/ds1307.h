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

#ifndef _HAVE_MP_DRV_DS1307_H
	#define _HAVE_MP_DRV_DS1307_H

	/**
	 * Maxim DS1307 real-time clock. The bus is bit-banged: the chip's maximum
	 * clock is 100 kHz, and the pins are whichever pair the board names, so
	 * this does not use the I2C peripheral. VCC must be 5 V or the chip stays
	 * silent. The battery keeps the oscillator and does not power the bus.
	 *
	 * It publishes one MP_DEVICE_CLOCK. The first subscriber starts a read
	 * every second. Commands: running (the oscillator) and sqw
	 * (off, 1hz, 4khz, 8khz, 32khz).
	 *
	 * options: model, serial, sda, clk, address (0x68 when missing).
	 */

	typedef struct mp_drv_ds1307_s mp_drv_ds1307_t;

	struct mp_drv_ds1307_s {
		mp_kernel_t *kernel;
		mp_gpio_port_t *sda;
		mp_gpio_port_t *scl;
		mp_task_t *task;

		mp_device_driver_t binding;
		mp_device_registry_t dev;
		mp_device_time_t time;

		const char *model;
		const char *serial;
		unsigned char address;
		/** last image of registers 0x00 to 0x07 */
		unsigned char regs[8];
		mp_bool_t haveRegs;
		/** YES while a subscriber holds the driver */
		mp_bool_t watched;

		mp_bool_t pending;
		unsigned char wrAt;
		unsigned char wrLen;
		unsigned char wr[8];
	};

	mp_ret_t mp_drv_ds1307_init(mp_kernel_t *kernel, mp_drv_ds1307_t *rtc, mp_options_t *options, char *who);
	void mp_drv_ds1307_fini(mp_drv_ds1307_t *rtc);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_CLOCK */
	extern const mp_device_ops_t mp_drv_ds1307_device;
#endif

#endif
