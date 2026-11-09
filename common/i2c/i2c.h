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

#ifndef _HAVE_MP_COMMON_I2C_H
	#define _HAVE_MP_COMMON_I2C_H

	#include <mp.h>

	/**
	 * @defgroup mpCommonI2C Shared I2C bus
	 * @ingroup mpCommon
	 *
	 * A synchronous 100 kHz I2C master on two open-drain GPIO pins. One bus
	 * owns SDA and SCL and serves every device on the same pair of pins; the
	 * kernel is cooperative, so a flush and a read never overlap mid-byte. A
	 * driver that needs the bus opens it once and shares the handle.
	 * @{
	 */

	typedef struct mp_i2c_bus_s mp_i2c_bus_t;

	struct mp_i2c_bus_s {
		mp_kernel_t *kernel;
		mp_gpio_port_t *sda;
		mp_gpio_port_t *scl;
		/** half-clock in microseconds; 5 gives 100 kHz */
		unsigned long half_us;
		/** 7-bit slave address of the current transfer */
		unsigned char address;
	};

	/** Claim SDA and SCL. half_us 0 defaults to 5. Returns 1 on success. */
	mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_bus_t *bus, mp_options_t *options, char *who);
	/** Release the pins. */
	void mp_i2c_close(mp_i2c_bus_t *bus);
	/** Change the half-clock (half_us 0 keeps the current value). */
	void mp_i2c_setup(mp_i2c_bus_t *bus, unsigned long half_us);

	/** start, addr+W, payload, stop. Returns 1 when the device acked every byte. */
	int mp_i2c_tx(mp_i2c_bus_t *bus, unsigned char address, const unsigned char *data, int len);
	/** start, addr+W, reg, payload, stop. */
	int mp_i2c_write_reg(mp_i2c_bus_t *bus, unsigned char address, unsigned char reg, const unsigned char *data, int len);
	/** start, addr+W, reg, restart, addr+R, read len, stop. NAK the last byte. */
	int mp_i2c_read_reg(mp_i2c_bus_t *bus, unsigned char address, unsigned char reg, unsigned char *buf, int len);

	/** @} */

#endif
