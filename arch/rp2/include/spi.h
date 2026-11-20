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

#ifndef _HAVE_RP2350_SPI_H
	#define _HAVE_RP2350_SPI_H

	#ifdef SUPPORT_COMMON_DMA
		#include <dma/dma.h>
	#endif

	/**
	 * common/spi owns mp_spi_open / mp_spi_tx when SUPPORT_COMMON_SPI is on
	 * (the ST7789 bit-bangs GPIO). The hardware master is compiled only when
	 * that module is absent, so the two never export the same symbol.
	 */

	typedef struct mp_spi_s mp_spi_t;

	struct mp_spi_s {
		unsigned long frequency;
		mp_gate_t *gate;
		mp_gpio_port_t *simo;
		mp_gpio_port_t *somi;
		mp_gpio_port_t *clk;
		mp_list_item_t item;
		SPI_TypeDef *regs;

		#ifdef SUPPORT_COMMON_DMA
			/** held by mp_spi_engine(), released by mp_spi_close() */
			mp_dma_engine_t *dmaRx;
			mp_dma_engine_t *dmaTx;
		#endif
	};

	void mp_spi_init();
	void mp_spi_fini();
	void mp_spi_retime(void);
	mp_bool_t mp_spi_quiet(void);

	#ifndef SUPPORT_COMMON_SPI
		mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_t *spi, mp_options_t *options, char *who);
		mp_ret_t mp_spi_setup(mp_spi_t *spi, mp_options_t *options);
		mp_ret_t mp_spi_close(mp_spi_t *spi);
		void mp_spi_write(mp_spi_t *spi, unsigned char *input, int size);
		void mp_spi_tx(mp_spi_t *spi, unsigned char data);
		unsigned char mp_spi_rx(mp_spi_t *spi);
	#endif

#endif
