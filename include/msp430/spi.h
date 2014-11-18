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
		MP_SPI_MODE_3PIN,
		MP_SPI_MODE_4PIN_LOW,
		MP_SPI_MODE_4PIN_HIGH
	} mp_spi_mode_t;

	typedef enum {
		MP_SPI_BIT_7,
		MP_SPI_BIT_8,
	} mp_spi_bit_t;

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

		/** who is handling SPI */
		char *who;

		mp_gpio_port_t *_simo;
		mp_gpio_port_t *_somi;
		mp_gpio_port_t *_clock;
		mp_gpio_port_t *_ste;
		mp_list_item_t item;
	};

#endif
