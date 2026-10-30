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

#ifndef _HAVE_ESP32_SERIES_H
	#define _HAVE_ESP32_SERIES_H

	/* HP core is RV32IMAC. The radios and the LP core are outside this port. */

	#define C6_UART0            0x60000000UL
	#define C6_I2C              0x60004000UL
	#define C6_TIMG0            0x60008000UL
	#define C6_TIMG1            0x60009000UL
	#define C6_SYSTIMER         0x6000A000UL
	#define C6_USB              0x6000F000UL
	#define C6_INTMTX           0x60010000UL
	#define C6_SPI2             0x60081000UL
	#define C6_IOMUX            0x60090000UL
	#define C6_GPIO             0x60091000UL
	#define C6_PCR              0x60096000UL
	#define C6_EFUSE            0x600B0800UL
	#define C6_LP_AON           0x600B1000UL
	#define C6_LP_WDT           0x600B1C00UL
	/* HP-core PLIC. Priority n is at +0x10 + n*4. The threshold resets to 1. */
	#define C6_PLIC             0x20001000UL

	#define C6_WDT_KEY          0x50D83AA1UL

	/* interrupt matrix sources */
	#define C6_SRC_GPIO         30
	#define C6_SRC_UART0        43
	#define C6_SRC_USB          48
	#define C6_SRC_I2C          50
	#define C6_SRC_SYSTIMER     57
	#define C6_SRC_SPI2         72

	/* CPU lines. 0 stays free. Priority must be above the threshold. */
	#define C6_CPU_USB          1
	#define C6_CPU_TICK         2
	#define C6_CPU_GPIO         3

	#define C6_PIN_COUNT        31
	#define C6_TICK_HZ          1000UL
	#define C6_SYSTIMER_HZ      16000000UL
	#define C6_XTAL_HZ          40000000UL
	#define C6_CPU_HIGH_HZ      160000000UL
	#define C6_CPU_MEDIUM_HZ    80000000UL
	#define C6_CPU_LOW_HZ       40000000UL

	/* GPIO 12/13 are USB D-/D+. GPIO 24-30 are the in-package flash. */
	#define C6_PIN_USB_DM       12
	#define C6_PIN_USB_DP       13
	#define C6_PIN_FLASH_FIRST  24

	static inline unsigned long c6_read(unsigned long addr) {
		return(*(volatile unsigned long *)addr);
	}

	static inline void c6_write(unsigned long addr, unsigned long value) {
		*(volatile unsigned long *)addr = value;
	}

	static inline void c6_set(unsigned long addr, unsigned long bits) {
		c6_write(addr, c6_read(addr) | bits);
	}

	static inline void c6_clr(unsigned long addr, unsigned long bits) {
		c6_write(addr, c6_read(addr) & ~bits);
	}

	/* one register per pad, GPIO0 at +4, then every 4 bytes */
	static inline unsigned long c6_iomux(int pin) {
		return(C6_IOMUX + (unsigned long)(pin + 1) * 4UL);
	}

	static inline unsigned long c6_gpio_pin(int pin) {
		return(C6_GPIO + 0x74UL + (unsigned long)pin * 4UL);
	}

#endif
