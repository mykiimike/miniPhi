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

	#define S3_XTAL_HZ        40000000UL
	#define S3_SYSTIMER_HZ    16000000UL
	#define S3_TICK_HZ        1000UL
	#define S3_CPU_HIGH_HZ    240000000UL
	#define S3_CPU_MEDIUM_HZ  160000000UL
	#define S3_CPU_LOW_HZ     80000000UL

	#define S3_UART0    0x60000000UL
	#define S3_GPIO     0x60004000UL
	#define S3_EFUSE    0x60007000UL
	#define S3_RTC      0x60008000UL
	#define S3_IOMUX    0x60009000UL
	#define S3_I2C      0x60013000UL
	#define S3_TIMG0    0x6001F000UL
	#define S3_TIMG1    0x60020000UL
	#define S3_SYSTIMER 0x60023000UL
	#define S3_SPI2     0x60024000UL
	#define S3_USB      0x60038000UL
	#define S3_SYSTEM   0x600C0000UL
	#define S3_INTMTX   0x600C2000UL

	#define S3_WDT_KEY  0x50D83AA1UL
	#define S3_SWD_KEY  0x8F1D312AUL

	#define S3_PIN_COUNT 49

	/* interrupt-matrix sources */
	#define S3_SRC_GPIO      16
	#define S3_SRC_SPI2      21
	#define S3_SRC_UART0     27
	#define S3_SRC_I2C       42
	#define S3_SRC_SYSTIMER  57
	#define S3_SRC_USB       96

	/* level-1 CPU lines: 8 and 12 are level, 10 is edge */
	#define S3_CPU_USB   8
	#define S3_CPU_TICK  10
	#define S3_CPU_GPIO  12

	static inline unsigned long s3_read(unsigned long addr) {
		return(*(volatile unsigned long *)addr);
	}

	static inline void s3_write(unsigned long addr, unsigned long value) {
		*(volatile unsigned long *)addr = value;
	}

	static inline void s3_set(unsigned long addr, unsigned long bits) {
		s3_write(addr, s3_read(addr) | bits);
	}

	static inline void s3_clr(unsigned long addr, unsigned long bits) {
		s3_write(addr, s3_read(addr) & ~bits);
	}

#endif
