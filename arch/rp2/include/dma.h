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

#ifndef _HAVE_RP2350_DMA_H
	#define _HAVE_RP2350_DMA_H

	#ifdef SUPPORT_COMMON_DMA

	#include <dma/dma.h>

	/**
	 * One request line, held until release.
	 * request is "spi0.rx" style. arm is the peripheral's DMA enable,
	 * called for the life of the claim. prep runs at each start.
	 * width is 1, 2 or 4. The engine accepts one segment and refuses cyclic.
	 * NULL when the request is missing or its channel is already claimed.
	 */
	mp_dma_engine_t *mp_dma_claim(
		const char *request,
		mp_dma_dir_t dir,
		volatile void *periph,
		unsigned char width,
		void (*arm)(void *user, mp_bool_t on),
		void (*prep)(void *user),
		void *user,
		char *who
	);
	void mp_dma_release(mp_dma_engine_t *engine);

	/** YES when no claimed channel is shifting */
	mp_bool_t mp_dma_quiet(void);

	/*
	 * RP2350 DMA, base 0x50000000. Sixteen channels, stride 0x40.
	 * CTRL bit positions are the RP2350 ones (TREQ_SEL at bit 17,
	 * CHAIN_TO at bit 13, INCR_WRITE at bit 6). They are not the RP2040
	 * positions. TRANS_COUNT is 28 bits; bits 31:28 are the mode.
	 * RESETS bit 2 is the DMA block. rp2350.h does not name it yet.
	 */
	#define RESETS_DMA (1UL << 2)

	#define RP2350_DMA_BASE 0x50000000UL
	#define DMA_CH_STRIDE   0x40UL
	#define DMA_CHANNELS    16

	typedef struct {
		__IO uint32_t READ_ADDR;
		__IO uint32_t WRITE_ADDR;
		__IO uint32_t TRANS_COUNT;
		__IO uint32_t CTRL_TRIG;
		__IO uint32_t AL1_CTRL;
		__IO uint32_t AL1_READ_ADDR;
		__IO uint32_t AL1_WRITE_ADDR;
		__IO uint32_t AL1_TRANS_COUNT_TRIG;
		__IO uint32_t AL2_CTRL;
		__IO uint32_t AL2_TRANS_COUNT;
		__IO uint32_t AL2_READ_ADDR;
		__IO uint32_t AL2_WRITE_ADDR_TRIG;
		__IO uint32_t AL3_CTRL;
		__IO uint32_t AL3_WRITE_ADDR;
		__IO uint32_t AL3_TRANS_COUNT;
		__IO uint32_t AL3_READ_ADDR_TRIG;
	} DMA_Channel_t;

	#define DMA_CH(n) ((DMA_Channel_t *)(RP2350_DMA_BASE + (unsigned long)(n) * DMA_CH_STRIDE))

	/* After the channels: raw status, IRQ 0, then IRQ 1..3 with a pad each.
	 * Abort sits at 0x464. */
	#define DMA_INTR       RP2350_REG(RP2350_DMA_BASE, 0x400)
	#define DMA_INTE0      RP2350_REG(RP2350_DMA_BASE, 0x404)
	#define DMA_INTF0      RP2350_REG(RP2350_DMA_BASE, 0x408)
	#define DMA_INTS0      RP2350_REG(RP2350_DMA_BASE, 0x40C)
	#define DMA_CHAN_ABORT RP2350_REG(RP2350_DMA_BASE, 0x464)

	#define DMA_TRANS_MAX 0x0fffffffUL

	#define DMA_CTRL_EN            (1UL << 0)
	#define DMA_CTRL_HIGH_PRIORITY (1UL << 1)
	#define DMA_CTRL_SIZE_LSB      2
	#define DMA_CTRL_INCR_READ     (1UL << 4)
	#define DMA_CTRL_INCR_WRITE    (1UL << 6)
	#define DMA_CTRL_CHAIN_LSB     13
	#define DMA_CTRL_TREQ_LSB      17
	#define DMA_CTRL_IRQ_QUIET     (1UL << 23)
	#define DMA_CTRL_BUSY          (1UL << 26)
	#define DMA_CTRL_WRITE_ERROR   (1UL << 29)
	#define DMA_CTRL_READ_ERROR    (1UL << 30)

	#endif

#endif
