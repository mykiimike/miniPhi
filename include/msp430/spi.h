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

	typedef void (*mp_spi_callback_t)(mp_spi_t *);

	struct mp_spi_s {

		/** SPI frequency */
		unsigned long frequency;

#define MP_SPI_TX_BUFFER 20
#define MP_SPI_RX_BUFFER 20

		unsigned char tx_buffer[MP_SPI_TX_BUFFER];
		unsigned int tx_size;
		unsigned int tx_pos;
		unsigned int tx_reference;

		unsigned char rx_buffer[MP_SPI_RX_BUFFER];
		unsigned int rx_size;

		mp_spi_callback_t onRead;
		mp_spi_callback_t onWriteEnd;
		mp_spi_callback_t onReadInterrupt;
		mp_spi_callback_t onWriteInterrupt;
		void *user;

		/** internal: gate */
		mp_gate_t *gate;
		mp_gpio_port_t *simo;
		mp_gpio_port_t *somi;
		mp_gpio_port_t *clk;
		mp_list_item_t item;

		mp_task_t *task;
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


	static inline void mp_spi_enable_rx(mp_spi_t *spi) {
		/* check CTS */
		_SPI_REG8(spi->gate, _SPI_CTL0) |= UCRXIE;
	}

	static inline void mp_spi_disable_rx(mp_spi_t *spi) {
		_SPI_REG8(spi->gate, _SPI_CTL0) &= ~UCRXIE;
	}

	static inline void mp_spi_enable_tx(mp_spi_t *spi) {
		/* check CTS */
		_SPI_REG8(spi->gate, _SPI_CTL0) |= UCTXIE;
	}

	static inline void mp_spi_disable_tx(mp_spi_t *spi) {
		_SPI_REG8(spi->gate, _SPI_CTL0) &= ~UCTXIE;
	}

	static inline unsigned char mp_spi_rx(mp_spi_t *spi) {
		while (!(_SPI_REG8(spi->gate, _SPI_IFG) & UCRXIFG));
		return(_SPI_REG8(spi->gate, _SPI_RXBUF));
	}

	static inline void mp_spi_tx(mp_spi_t *spi, unsigned char data) {
		while (!(_SPI_REG8(spi->gate, _SPI_IFG) & UCTXIFG));
		_SPI_REG8(spi->gate, _SPI_TXBUF) = data;
	}


#endif
