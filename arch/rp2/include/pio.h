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
@file pio.h
@brief PIO0 register macros for the WS2812 pixel driver.

Include after mp.h. The pixel driver claims GPIO22 itself. The board
reservation on pa22 has to be removed first; this header does not clear it.
*/

#ifndef _HAVE_RP2350_PIO_H
	#define _HAVE_RP2350_PIO_H

	/* RP2350 PIO blocks. The pixel driver uses PIO0 only. */
	#define RP2350_PIO0_BASE 0x50200000UL
	#define RP2350_PIO1_BASE 0x50300000UL
	#define RP2350_PIO2_BASE 0x50400000UL

	/* RESETS.RESET bits. A set bit holds the block in reset. */
	#define RESETS_PIO0 (1UL << 11)
	#define RESETS_PIO1 (1UL << 12)
	#define RESETS_PIO2 (1UL << 13)

	/* IO_BANK0 FUNCSEL. 6 is PIO0, next to SIO at 5. */
	#define IO_GPIO_FUNCSEL_PIO0 6UL

	#define PIO0_REG(off) RP2350_REG(RP2350_PIO0_BASE, off)

	#define PIO0_CTRL           PIO0_REG(0x000)
	#define PIO0_FSTAT          PIO0_REG(0x004)
	#define PIO0_FDEBUG         PIO0_REG(0x008)
	#define PIO0_TXF0           PIO0_REG(0x010)
	#define PIO0_INSTR_MEM(n)   PIO0_REG(0x048 + 4 * (n))
	#define PIO0_SM0_CLKDIV     PIO0_REG(0x0c8)
	#define PIO0_SM0_EXECCTRL   PIO0_REG(0x0cc)
	#define PIO0_SM0_SHIFTCTRL  PIO0_REG(0x0d0)
	#define PIO0_SM0_INSTR      PIO0_REG(0x0d8)
	#define PIO0_SM0_PINCTRL    PIO0_REG(0x0dc)

	/* CTRL: enable is RW. Restart bits are write-1 and clear themselves. */
	#define PIO_CTRL_SM0_ENABLE         (1UL << 0)
	#define PIO_CTRL_SM0_RESTART        (1UL << 4)
	#define PIO_CTRL_SM0_CLKDIV_RESTART (1UL << 8)

	/* FSTAT bit 24 is SM0 TX FIFO empty. FDEBUG bit 24 sticks while it is stalled. */
	#define PIO_FSTAT_TXEMPTY0  (1UL << 24)
	#define PIO_FDEBUG_TXSTALL0 (1UL << 24)

	/* SM0_EXECCTRL bit 31: a forced instruction has not finished. */
	#define PIO_SM_EXECCTRL_EXEC_STALLED (1UL << 31)

	/* SM0_CLKDIV is 16.8: integer in bits 31:16, fraction in bits 15:8. */
	#define PIO_SM_CLKDIV_INT_LSB  16
	#define PIO_SM_CLKDIV_FRAC_LSB 8

	#define PIO_SM_EXECCTRL_WRAP_TOP_LSB    12
	#define PIO_SM_EXECCTRL_WRAP_BOTTOM_LSB 7

	/* Shift left (MSB first) leaves OUT_SHIFTDIR at 0. Threshold 0 means 32. */
	#define PIO_SM_SHIFTCTRL_PULL_THRESH_LSB 25
	#define PIO_SM_SHIFTCTRL_IN_SHIFTDIR     (1UL << 18)
	#define PIO_SM_SHIFTCTRL_AUTOPULL        (1UL << 17)

	#define PIO_SM_PINCTRL_SIDESET_COUNT_LSB 29
	#define PIO_SM_PINCTRL_SET_COUNT_LSB     26
	#define PIO_SM_PINCTRL_SIDESET_BASE_LSB  10
	#define PIO_SM_PINCTRL_SET_BASE_LSB      5

	/**
	 * Reprogram PIO0 SM0 from the current clk_sys. commit() calls this
	 * before every frame, so arch/rp2/rp2350/clock.c does not have to.
	 * A call next to mp_timer_retime() is optional.
	 */
	void mp_pio_retime(void);

	void mp_rp2350_pixel(mp_kernel_t *kernel);
	void mp_rp2350_pixel_fini(mp_kernel_t *kernel);

#endif
