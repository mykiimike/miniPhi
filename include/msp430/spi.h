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

	typedef enum {
		MP_SPI_CLK_PHASE_CHANGE,
		MP_SPI_CLK_PHASE_CAPTURE
	} mp_spi_phase_t;

	typedef enum {
		MP_SPI_CLK_POLARITY_LOW,
		MP_SPI_CLK_POLARITY_HIGH
	} mp_spi_polarity_t;

	typedef enum {
		MP_SPI_MASTER,
		MP_SPI_SLAVE
	} mp_spi_role_t;

	typedef enum {
		MP_SPI_MSB,
		MP_SPI_LSB
	} mp_spi_first_t;

	typedef enum {
		MP_SPI_MODE_3PIN,
		MP_SPI_MODE_4PIN_LOW,
		MP_SPI_MODE_4PIN_HIGH
	} mp_spi_mode_t;

	typedef enum {
		MP_SPI_BIT_7,
		MP_SPI_BIT_8,
	} mp_spi_bit_t;

	typedef enum {
		MP_SPI_SYNC,
		MP_SPI_ASYNC,
	} mp_spi_sync_t;

	struct mp_spi_s {
		/** some architectures require a gate */
		char *gateId;

		/** SPI SIMO configuration GPIO port */
		mp_gpio_pair_t simo;

		/** SPI SOMI configuration GPIO port */
		mp_gpio_pair_t somi;

		/** SPI Clock configuration GPIO port */
		mp_gpio_pair_t clock;

		/** SPI Select configuration GPIO port */
		mp_gpio_pair_t ste;

		/** SPI frequency */
		unsigned long frequency;

#define MP_SPI_TX_BUFFER 80
#define MP_SPI_RX_BUFFER 80

		unsigned char tx_buffer[MP_SPI_TX_BUFFER];
		unsigned int tx_size;
		unsigned int tx_pos;
		unsigned int tx_reference;

		unsigned char rx_buffer[MP_SPI_RX_BUFFER];
		unsigned int rx_size;

		mp_spi_callback_t onRead;
		mp_spi_callback_t onWriteEnd;

		/** internal: gate */
		mp_gate_t *_gate;
		mp_gpio_port_t *_simo;
		mp_gpio_port_t *_somi;
		mp_gpio_port_t *_clock;
		mp_gpio_port_t *_ste;
		mp_list_item_t item;

		mp_task_t *task;
	};

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
		_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCRXIE;
	}

	static inline void mp_spi_disable_rx(mp_spi_t *spi) {
		_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~UCRXIE;
	}

	static inline void mp_spi_enable_tx(mp_spi_t *spi) {
		/* check CTS */
		_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCTXIE;
	}

	static inline void mp_spi_disable_tx(mp_spi_t *spi) {
		_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~UCTXIE;
	}

#endif
