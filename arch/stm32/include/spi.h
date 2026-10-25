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

#ifndef _HAVE_STM32_SPI_H
	#define _HAVE_STM32_SPI_H

	#ifdef SUPPORT_COMMON_DMA
		#include <dma/dma.h>
	#endif

	typedef struct mp_spi_s mp_spi_t;

	typedef enum {
		MP_SPI_FL_TX = SPI_SR_TXE,
		MP_SPI_FL_RX = SPI_SR_RXNE,
	} mp_spi_flag_t;

	typedef enum {
		MP_SPI_IV_TX = 1,
		MP_SPI_IV_RX = 2,
	} mp_spi_iv_t;

	typedef void (*mp_spi_interrupt_t)(mp_spi_t *spi, mp_spi_iv_t iv);

	struct mp_spi_s {
		unsigned long frequency;

		mp_gate_t *gate;

		mp_gpio_port_t *simo;
		mp_gpio_port_t *somi;
		mp_gpio_port_t *clk;

		mp_list_item_t item;

		mp_spi_interrupt_t intDispatch;
		void *user;

		mp_task_t *task;

		/* RXNEIE / TXEIE as last enabled */
		unsigned long ie;

		SPI_TypeDef *regs;

		#ifdef SUPPORT_COMMON_DMA
			/** held by mp_spi_engine(), released by mp_spi_close() */
			mp_dma_engine_t *dmaRx;
			mp_dma_engine_t *dmaTx;
		#endif
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
	void mp_spi_retime(void);
	mp_bool_t mp_spi_quiet(void);

	/* blocking: sends size bytes and drops what comes back */
	void mp_spi_write(mp_spi_t *spi, unsigned char *input, int size);

	static inline void mp_spi_enable_rx(mp_spi_t *spi) {
		spi->ie |= SPI_CR2_RXNEIE;
		spi->regs->CR2 |= SPI_CR2_RXNEIE;
	}

	static inline void mp_spi_disable_rx(mp_spi_t *spi) {
		spi->ie &= ~SPI_CR2_RXNEIE;
		spi->regs->CR2 &= ~SPI_CR2_RXNEIE;
	}

	static inline void mp_spi_enable_tx(mp_spi_t *spi) {
		spi->ie |= SPI_CR2_TXEIE;
		spi->regs->CR2 |= SPI_CR2_TXEIE;
	}

	static inline void mp_spi_disable_tx(mp_spi_t *spi) {
		spi->ie &= ~SPI_CR2_TXEIE;
		spi->regs->CR2 &= ~SPI_CR2_TXEIE;
	}

	static inline void mp_spi_enable_both(mp_spi_t *spi) {
		spi->ie |= SPI_CR2_RXNEIE | SPI_CR2_TXEIE;
		spi->regs->CR2 |= SPI_CR2_RXNEIE | SPI_CR2_TXEIE;
	}

	static inline void mp_spi_disable_both(mp_spi_t *spi) {
		spi->ie &= ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE);
		spi->regs->CR2 &= ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE);
	}

	static inline void mp_spi_disable_store(mp_spi_t *spi) {
		spi->regs->CR2 &= ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE);
	}

	static inline void mp_spi_disable_restore(mp_spi_t *spi) {
		spi->regs->CR2 = (spi->regs->CR2 & ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE)) | spi->ie;
	}

	unsigned char mp_spi_rx(mp_spi_t *spi);

	/* waits for room in DR, then queues the byte */
	void mp_spi_tx(mp_spi_t *spi, unsigned char data);

	static inline void mp_spi_setInterruption(mp_spi_t *spi, mp_spi_interrupt_t cb) {
		spi->intDispatch = cb;
	}

	#ifdef SUPPORT_COMMON_DMA
		/** NULL when this SPI has no DMA line, or the line is already claimed */
		mp_dma_engine_t *mp_spi_engine(mp_spi_t *spi, mp_dma_dir_t dir);
		void mp_spi_engine_drop(mp_spi_t *spi);
	#endif

#endif
