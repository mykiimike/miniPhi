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

/**
@file i2c.h
@brief I2C master with the MSP430 USCI call model

regMaster drives the bus the USCI way: pick the direction, ask for a
START, then move one byte per TX or RX interrupt and ask for a STOP
before the last byte. The F1 block wants an explicit SB / ADDR
handshake, so i2c.c keeps a small phase machine that turns those calls
into the right register sequence. Master only.
*/

#ifndef _HAVE_STM32_I2C_H
	#define _HAVE_STM32_I2C_H

	typedef struct mp_i2c_s mp_i2c_t;

	typedef void (*mp_i2c_callback_t)(mp_i2c_t *);

	typedef enum {
		MP_I2C_FL_NACK = 1,
		MP_I2C_FL_STOP = 2,
		MP_I2C_FL_START = 4,
		MP_I2C_FL_TX = 8,
		MP_I2C_FL_RX = 16,
	} mp_i2c_flag_t;

	typedef enum {
		MP_I2C_PHASE_IDLE = 0,
		MP_I2C_PHASE_ADDRESS,
		MP_I2C_PHASE_DATA,
	} mp_i2c_phase_t;

	typedef void (*mp_i2c_interrupt_t)(mp_i2c_t *i2c, mp_i2c_flag_t flag);

	struct mp_i2c_s {
		mp_i2c_interrupt_t intDispatch;

		mp_gate_t *gate;

		mp_gpio_port_t *sda;
		mp_gpio_port_t *clk;

		mp_list_item_t item;

		void *user;

		I2C_TypeDef *regs;

		/* 7-bit slave address */
		unsigned char slave;

		/* 1 transmit, 0 receive: what the next START will do */
		char mode;

		/* direction of the transfer on the wire */
		char wire;

		mp_i2c_phase_t phase;

		/* STOP is armed: the next received byte is the last one */
		mp_bool_t stopArmed;

		mp_bool_t txEnabled;
		mp_bool_t rxEnabled;

		/* SCL in Hz, kept to retime the bus */
		unsigned long frequency;

		/* tick of the last START or served event, for mp_i2c_watch() */
		unsigned long since;
	};

	mp_ret_t mp_i2c_init();
	mp_ret_t mp_i2c_fini();
	mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_t *i2c, mp_options_t *options, char *who);
	void mp_i2c_retime(void);
	mp_bool_t mp_i2c_quiet(void);
	void mp_i2c_watch(void);

	/* options: frequency in Hz (100000 by default, 400000 for fast mode) */
	mp_ret_t mp_i2c_setup(mp_i2c_t *i2c, mp_options_t *options);
	mp_ret_t mp_i2c_close(mp_i2c_t *i2c);
	/** clock the bus free and rearm the peripheral; the pins stay claimed */
	void mp_i2c_reset(mp_i2c_t *i2c);

	void mp_i2c_enable_rx(mp_i2c_t *i2c);
	void mp_i2c_disable_rx(mp_i2c_t *i2c);
	void mp_i2c_enable_tx(mp_i2c_t *i2c);
	void mp_i2c_disable_tx(mp_i2c_t *i2c);

	void mp_i2c_mode(mp_i2c_t *i2c, char mode);
	void mp_i2c_txStart(mp_i2c_t *i2c);
	void mp_i2c_txStop(mp_i2c_t *i2c);
	void mp_i2c_txNACK(mp_i2c_t *i2c);
	void mp_i2c_waitStart(mp_i2c_t *i2c);
	void mp_i2c_waitStop(mp_i2c_t *i2c);
	void mp_i2c_clearFlags(mp_i2c_t *i2c);

	static inline unsigned char mp_i2c_rx(mp_i2c_t *i2c) {
		return(i2c->regs->DR);
	}

	static inline void mp_i2c_tx(mp_i2c_t *i2c, unsigned char data) {
		i2c->regs->DR = data;
	}

	static inline void mp_i2c_waitRX(mp_i2c_t *i2c) {
		while (!(i2c->regs->SR1 & I2C_SR1_RXNE));
	}

	static inline void mp_i2c_waitTX(mp_i2c_t *i2c) {
		while (!(i2c->regs->SR1 & I2C_SR1_TXE));
	}

	static inline void mp_i2c_setSlaveAddress(mp_i2c_t *i2c, unsigned short address) {
		i2c->slave = address & 0x7F;
	}

	static inline unsigned char mp_i2c_getSlaveAddress(mp_i2c_t *i2c) {
		return(i2c->slave);
	}

	static inline void mp_i2c_setMyAddress(mp_i2c_t *i2c, unsigned short address) {
		i2c->regs->OAR1 = I2C_OAR1_BIT14 | ((address & 0x7F) << 1);
	}

	static inline void mp_i2c_setInterruption(mp_i2c_t *i2c, mp_i2c_interrupt_t cb) {
		i2c->intDispatch = cb;
	}

#endif
