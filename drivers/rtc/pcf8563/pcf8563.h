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

#ifndef _HAVE_MP_DRV_RTC_PCF8563_H
	#define _HAVE_MP_DRV_RTC_PCF8563_H

	#include <mp.h>
	#include <i2c/i2c.h>

	/**
	 * @defgroup mpDrvPcf8563 NXP PCF8563 RTC
	 * @ingroup mpDrv
	 *
	 * A PCF8563 shares its I2C bus with a display that already owns SDA and
	 * SCL. The driver finds that display at init, borrows its mp_i2c_bus_t,
	 * and reads the civil-time registers. Address 0x51.
	 * options: address (0x51 when missing).
	 * @{
	 */

	typedef struct mp_drv_pcf8563_s mp_drv_pcf8563_t;

	struct mp_drv_pcf8563_s {
		mp_kernel_t *kernel;
		/** the OLED's bus, borrowed; the panel owns SDA and SCL */
		mp_i2c_bus_t *bus;
		unsigned char address;
		/** last register read, BCD as the chip stores it */
		unsigned char regs[7];
		mp_bool_t haveRegs;
		mp_device_driver_t binding;
		mp_device_registry_t dev;
		mp_device_time_t time;
		mp_task_t *task;
		mp_bool_t watched;
	};

	mp_ret_t mp_drv_pcf8563_init(mp_kernel_t *kernel, mp_drv_pcf8563_t *rtc, mp_options_t *options, char *who);
	void mp_drv_pcf8563_fini(mp_drv_pcf8563_t *rtc);

#ifdef SUPPORT_COMMON_DEVICE
	extern const mp_device_ops_t mp_drv_pcf8563_device;
#endif

	/** @} */

#endif
