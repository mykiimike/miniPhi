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

#ifndef _HAVE_MSP430_SPI_H
	#define _HAVE_MSP430_SPI_H

	typedef struct mp_spi_s mp_spi_t;
/*
	typedef enum {
		MP_SPI_FL_TX = UCTXIFG,
		MP_SPI_FL_RX = UCRXIFG,
	} mp_spi_flag_t;
*/
	typedef enum {
		MP_SPI_IV_TX = USCI_UCTXIFG,
		MP_SPI_IV_RX = USCI_UCRXIFG,
	} mp_spi_iv_t;

	typedef void (*mp_spi_interrupt_t)(mp_spi_t *spi, mp_spi_iv_t iv);

	struct mp_spi_s {

		/** SPI frequency */
		unsigned long frequency;

		/** internal: gate */
		mp_gate_t *gate;
		mp_gpio_port_t *simo;
		mp_gpio_port_t *somi;
		mp_gpio_port_t *clk;
		mp_list_item_t item;

		mp_spi_interrupt_t intDispatch;
		void *user;

		mp_task_t *task;

		unsigned char ie;
	};

	void mp_spi_init();
	void mp_spi_fini();
	mp_ret_t mp_spi_open(
		mp_kernel_t *kernel,
		mp_spi_t *spi,
		mp_options_t *options,
		char *who
	);
	mp_ret_t mp_spi_setup(mp_spi_t *spi, mp_options_t *options);
	mp_ret_t mp_spi_close(mp_spi_t *spi);
	void mp_spi_write(mp_spi_t *spi, unsigned char *input, int size);

	#define _SPI_CTLW0   0x00
	#define _SPI_CTL0    0x01
	#define _SPI_CTL1    0x00
	#define _SPI_BRW     0x06
	#define _SPI_BR0     0x06
	#define _SPI_BR1     0x07
	#define _SPI_MCTL    0x08
	#define _SPI_STATW   0x0a
	#define _SPI_RXBUF   0x0c
	#define _SPI_TXBUF   0x0e

	#define _SPI_IE      0x1c
	#define _SPI_IFG     0x1d
	#define _SPI_IV      0x1e

	#define _SPI_REG8(_port, _type) \
		*((volatile char *)(_port->_baseAddress+_type))

	#define _SPI_REG16(_port, _type) \
		*((volatile short *)(_port->_baseAddress+_type))

	static inline void mp_spi_enable_rx(mp_spi_t *spi) {
		/* check CTS */
		_SPI_REG8(spi->gate, _SPI_IE) |= UCRXIE;
		spi->ie |= UCRXIE;
	}

	static inline void mp_spi_disable_rx(mp_spi_t *spi) {
		_SPI_REG8(spi->gate, _SPI_IE) &= ~UCRXIE;
		spi->ie &= ~UCRXIE;
	}

	static inline void mp_spi_enable_tx(mp_spi_t *spi) {
		/* check CTS */
		_SPI_REG8(spi->gate, _SPI_IE) |= UCTXIE;
		spi->ie |= UCTXIE;
	}

	static inline void mp_spi_disable_tx(mp_spi_t *spi) {
		_SPI_REG8(spi->gate, _SPI_IE) &= ~UCTXIE;
		spi->ie &= ~UCTXIE;
	}

	static inline void mp_spi_enable_both(mp_spi_t *spi) {
		/* check CTS */
		_SPI_REG8(spi->gate, _SPI_IE) |= UCTXIE + UCRXIE;
		spi->ie |= UCTXIE | UCRXIE;
	}

	static inline void mp_spi_disable_both(mp_spi_t *spi) {
		_SPI_REG8(spi->gate, _SPI_IE) &= ~(UCTXIE + UCRXIE);
		spi->ie &= ~(UCTXIE | UCRXIE);
	}

	static inline void mp_spi_disable_store(mp_spi_t *spi) {
		_SPI_REG8(spi->gate, _SPI_IE) &= ~(UCTXIE + UCRXIE);
	}

	static inline void mp_spi_disable_restore(mp_spi_t *spi) {
		_SPI_REG8(spi->gate, _SPI_IE) = spi->ie;
	}

	unsigned char mp_spi_rx(mp_spi_t *spi);
	void mp_spi_tx(mp_spi_t *spi, unsigned char data);

	/*
	mp_spi_flag_t mp_spi_flags_get(mp_spi_t *spi);
	void mp_spi_flags_set(mp_spi_t *spi, mp_spi_flag_t data);
	*/

	static inline void mp_spi_setInterruption(mp_spi_t *spi, mp_spi_interrupt_t cb) {
		spi->intDispatch = cb;
	}

#endif
