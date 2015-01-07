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

	#define MP_I2C_BUFFER_SIZE 32

	#define MP_I2C_READY 0
	#define MP_I2C_MRX   1
	#define MP_I2C_MTX   2
	#define MP_I2C_SRX   3
	#define MP_I2C_STX   4

	struct mp_i2c_s {
		unsigned char rxBuffer[MP_I2C_BUFFER_SIZE];
		unsigned char rxBufferIndex;
		unsigned char rxBufferLength;

		unsigned char txBuffer[MP_I2C_BUFFER_SIZE];
		unsigned char txBufferIndex;
		unsigned char txBufferLength;

		mp_interrupt_t *inte;

		unsigned char transmitting;

		/** internal: gate */
		mp_gate_t *gate;
		mp_gpio_port_t *sda;
		mp_gpio_port_t *clk;


		mp_list_item_t item;
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

	#define _I2C_OA      0x10
	#define _I2C_SA      0x12

	#define _I2C_IE      0x1c
	#define _I2C_IFG     0x1d
	#define _I2C_IV      0x1e

	mp_ret_t mp_i2c_init();
	mp_ret_t mp_i2c_fini();
	mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_t *i2c, mp_options_t *options, char *who);
	mp_ret_t mp_i2c_setup(mp_i2c_t *i2c, mp_options_t *options);
	mp_ret_t mp_i2c_close(mp_i2c_t *i2c);

	#define _I2C_REG8(_port, _type) \
		*((volatile char *)(_port->_baseAddress+_type))

	#define _I2C_REG16(_port, _type) \
		*((volatile short *)(_port->_baseAddress+_type))

	static inline void mp_i2c_enable_rx(mp_i2c_t *i2c) {
		/* check CTS */
		_I2C_REG8(i2c->gate, _I2C_CTL0) |= UCRXIE;
	}

	static inline void mp_i2c_disable_rx(mp_i2c_t *i2c) {
		_I2C_REG8(i2c->gate, _I2C_CTL0) &= ~UCRXIE;
	}

	static inline void mp_i2c_enable_tx(mp_i2c_t *i2c) {
		/* check CTS */
		_I2C_REG8(i2c->gate, _I2C_CTL0) |= UCTXIE;
	}

	static inline void mp_i2c_disable_tx(mp_i2c_t *i2c) {
		_I2C_REG8(i2c->gate, _I2C_CTL0) &= ~UCTXIE;
	}

	static inline unsigned char mp_i2c_rx(mp_i2c_t *i2c) {
		return(_I2C_REG8(i2c->gate, _I2C_RXBUF));
	}

	static inline void mp_i2c_tx(mp_i2c_t *i2c, unsigned char data) {
		_I2C_REG8(i2c->gate, _I2C_TXBUF) = data;
	}

	static inline void mp_i2c_waitRX(mp_i2c_t *i2c) {
		while (!(_I2C_REG8(i2c->gate, _I2C_IFG) & UCRXIFG));
	}

	static inline void mp_i2c_waitTX(mp_i2c_t *i2c) {
		while (!(_I2C_REG8(i2c->gate, _I2C_IFG) & UCTXIFG));
	}

	static inline void mp_i2c_mode(mp_i2c_t *i2c, char mode)  {


		/* Receiver */
		if(mode == 0) {
			//_I2C_REG8(i2c->gate, _I2C_IFG) &= ~(UCRXIFG);
			_I2C_REG8(i2c->gate, _I2C_CTL1) &= ~(UCTR);
		}
		else {
			//_I2C_REG8(i2c->gate, _I2C_IFG) &= ~(UCTXIFG);
			_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCTR;
		}
	}

	static inline void mp_i2c_txNACK(mp_i2c_t *i2c) {
		_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCTXNACK;
	}

	static inline void mp_i2c_txStop(mp_i2c_t *i2c) {
		_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCTXSTP;
		while (_I2C_REG8(i2c->gate, _I2C_CTL1) & UCTXSTP);
	}

	static inline void mp_i2c_txStart(mp_i2c_t *i2c) {
		_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCTXSTT;
	}

	static inline void mp_i2c_setSlaveAddress(mp_i2c_t *i2c, unsigned short address) {
		_I2C_REG8(i2c->gate, _I2C_SA) = address;
	}

	static inline void mp_i2c_setMyAddress(mp_i2c_t *i2c, unsigned short address) {
		_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCSWRST;
		_I2C_REG8(i2c->gate, _I2C_OA) = address;
		_I2C_REG8(i2c->gate, _I2C_CTL1) &= ~(UCSWRST);
	}

#endif
