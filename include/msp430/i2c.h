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

#ifndef _HAVE_MSP430_I2C_H
	#define _HAVE_MSP430_I2C_H

	typedef struct mp_i2c_s mp_i2c_t;

	typedef void (*mp_i2c_callback_t)(mp_i2c_t *);

	struct mp_i2c_s {


		/** internal: gate */
		mp_gate_t *gate;
		mp_gpio_port_t *sda;
		mp_gpio_port_t *clk;

		mp_task_t *task;
	};


	#define _I2C_CTLW0   0x00
	#define _I2C_CTL0    0x01
	#define _I2C_CTL1    0x00
	#define _I2C_BRW     0x06
	#define _I2C_BR0     0x06
	#define _I2C_BR1     0x07

	#define _I2C_STAT    0x0a
	#define _I2C_RXBUF   0x0c
	#define _I2C_TXBUF   0x0e

	#define _I2C_IE      0x1c
	#define _I2C_IFG     0x1d
	#define _I2C_IV      0x1e

	#define _I2C_REG8(_port, _type) \
		*((volatile char *)(_port->_baseAddress+_type))

	static inline void mp_i2c_enable_rx(mp_i2c_t *spi) {
		/* check CTS */
		_I2C_REG8(spi->gate, _I2C_CTL0) |= UCRXIE;
	}

	static inline void mp_i2c_disable_rx(mp_i2c_t *spi) {
		_I2C_REG8(spi->gate, _I2C_CTL0) &= ~UCRXIE;
	}

	static inline void mp_i2c_enable_tx(mp_i2c_t *spi) {
		/* check CTS */
		_I2C_REG8(spi->gate, _I2C_CTL0) |= UCTXIE;
	}

	static inline void mp_i2c_disable_tx(mp_i2c_t *spi) {
		_I2C_REG8(spi->gate, _I2C_CTL0) &= ~UCTXIE;
	}

	static inline unsigned char mp_i2c_rx(mp_i2c_t *spi) {
		while (!(_I2C_REG8(spi->gate, _I2C_IFG) & UCRXIFG));
		return(_I2C_REG8(spi->gate, _I2C_RXBUF));
	}

	static inline void mp_i2c_tx(mp_i2c_t *spi, unsigned char data) {
		while (!(_I2C_REG8(spi->gate, _I2C_IFG) & UCTXIFG));
		_I2C_REG8(spi->gate, _I2C_TXBUF) = data;
	}


#endif
