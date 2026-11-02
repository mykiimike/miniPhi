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

	/* Single RV32IMC core. No LP core, no PLIC. The radio is outside this port. */

	#define C3_UART0            0x60000000UL
	#define C3_GPIO             0x60004000UL
	#define C3_RTC              0x60008000UL
	#define C3_EFUSE            0x60008800UL
	#define C3_IOMUX            0x60009000UL
	#define C3_I2C              0x60013000UL
	#define C3_TIMG0            0x6001F000UL
	#define C3_TIMG1            0x60020000UL
	#define C3_SYSTIMER         0x60023000UL
	#define C3_SPI2             0x60024000UL
	#define C3_USB              0x60043000UL
	#define C3_SYSTEM           0x600C0000UL
	#define C3_INTMTX           0x600C2000UL

	/* CPU interrupt controller, after the matrix map registers. */
	#define C3_INTC_ENABLE      (C3_INTMTX + 0x104UL)
	#define C3_INTC_TYPE        (C3_INTMTX + 0x108UL)
	/* write 1 to drop the latched edge for that CPU line */
	#define C3_INTC_CLEAR       (C3_INTMTX + 0x10CUL)
	#define C3_INTC_PRI         (C3_INTMTX + 0x114UL)
	#define C3_INTC_THRESH      (C3_INTMTX + 0x194UL)

	#define C3_WDT_KEY          0x50D83AA1UL
	#define C3_SWD_KEY          0x8F1D312AUL

	/* interrupt matrix sources */
	#define C3_SRC_GPIO         16
	#define C3_SRC_SPI2         19
	#define C3_SRC_UART0        21
	#define C3_SRC_USB          26
	#define C3_SRC_I2C          29
	#define C3_SRC_SYSTIMER     37

	/* CPU lines. 0 stays free. Priority must be above the threshold. */
	#define C3_CPU_USB          1
	#define C3_CPU_TICK         2
	#define C3_CPU_GPIO         3

	#define C3_PIN_COUNT        22
	#define C3_TICK_HZ          1000UL
	#define C3_SYSTIMER_HZ      16000000UL
	#define C3_XTAL_HZ          40000000UL
	#define C3_CPU_HIGH_HZ      160000000UL
	#define C3_CPU_MEDIUM_HZ    80000000UL
	/* The 480 MHz PLL divides to 160 or 80 only. LOW stays at 80 so the
	 * USB PHY, which takes 48 MHz from that PLL, keeps its clock. */
	#define C3_CPU_LOW_HZ       80000000UL

	/* GPIO 11 is the in-package flash supply. GPIO 12-17 are its SPI bus.
	 * GPIO 18/19 are USB D-/D+. */
	#define C3_PIN_FLASH_FIRST  11
	#define C3_PIN_FLASH_LAST   17
	#define C3_PIN_USB_DM       18
	#define C3_PIN_USB_DP       19

	static inline unsigned long c3_read(unsigned long addr) {
		return(*(volatile unsigned long *)addr);
	}

	static inline void c3_write(unsigned long addr, unsigned long value) {
		*(volatile unsigned long *)addr = value;
	}

	static inline void c3_set(unsigned long addr, unsigned long bits) {
		c3_write(addr, c3_read(addr) | bits);
	}

	static inline void c3_clr(unsigned long addr, unsigned long bits) {
		c3_write(addr, c3_read(addr) & ~bits);
	}

	/* one register per pad, GPIO0 at +4, then every 4 bytes */
	static inline unsigned long c3_iomux(int pin) {
		return(C3_IOMUX + (unsigned long)(pin + 1) * 4UL);
	}

	static inline unsigned long c3_gpio_pin(int pin) {
		return(C3_GPIO + 0x74UL + (unsigned long)pin * 4UL);
	}

#endif
