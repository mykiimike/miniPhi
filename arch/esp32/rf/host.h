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

#ifndef _HAVE_ESP32_RF_HOST_H
	#define _HAVE_ESP32_RF_HOST_H

	typedef unsigned long (*mp_arch_rf_rom_t)();
	typedef unsigned long (*mp_arch_rf_code_t)();

	/**
	 * Read one word.
	 * @param addr Register address
	 * @returns The word at addr
	 */
	static inline unsigned long mp_arch_rf_read(unsigned long addr) {
#ifdef __XTENSA__
		__asm__ volatile("memw" ::: "memory");
#endif
		return(*(volatile unsigned long *)addr);
	}

	/**
	 * Write one word.
	 * @param addr Register address
	 * @param v Word to store
	 */
	static inline void mp_arch_rf_write(unsigned long addr, unsigned long v) {
#ifdef __XTENSA__
		__asm__ volatile("memw" ::: "memory");
#endif
		*(volatile unsigned long *)addr = v;
	}

	/**
	 * Set bits. Other bits stay.
	 * @param addr Register address
	 * @param mask Bits to set
	 */
	static inline void mp_arch_rf_set(unsigned long addr, unsigned long mask) {
		mp_arch_rf_write(addr, mp_arch_rf_read(addr) | mask);
	}

	/**
	 * Clear bits. Other bits stay.
	 * @param addr Register address
	 * @param mask Bits to clear
	 */
	static inline void mp_arch_rf_clr(unsigned long addr, unsigned long mask) {
		mp_arch_rf_write(addr, mp_arch_rf_read(addr) & ~mask);
	}

	/**
	 * Busy-wait. The archive calls this ets_delay_us().
	 * @param us Microseconds to wait. Zero returns.
	 */
	void mp_arch_rf_delay_us(uint32_t us);

	/** The archive name phy_printf() is mp_printk(). */
	#define mp_arch_rf_printf mp_printk

	/** Mask machine interrupts for the analog bus. The C6 archive calls this phy_i2c_enter_critical(). */
	void mp_arch_rf_i2c_enter_critical(void);
	/** Restore the mask taken by mp_arch_rf_i2c_enter_critical(). */
	void mp_arch_rf_i2c_exit_critical(void);

	/**
	 * Same mask as mp_arch_rf_i2c_enter_critical(). The ESP32 and the S2
	 * call this phy_enter_critical(). The S3 calls both names, so the two
	 * share one depth.
	 */
	void mp_arch_rf_enter_critical(void);
	/** Restore the mask taken by mp_arch_rf_enter_critical(). */
	void mp_arch_rf_exit_critical(void);

	/**
	 * Crystal frequency in MHz. The series defines this when its archive
	 * calls rtc_clk_xtal_freq_get(). The C6 definition reads C6_XTAL_HZ.
	 * @returns Megahertz
	 */
	int mp_arch_rf_xtal_mhz(void);

	#define ets_delay_us mp_arch_rf_delay_us
	#define phy_printf mp_arch_rf_printf
	#define phy_i2c_enter_critical mp_arch_rf_i2c_enter_critical
	#define phy_i2c_exit_critical mp_arch_rf_i2c_exit_critical
	#define phy_enter_critical mp_arch_rf_enter_critical
	#define phy_exit_critical mp_arch_rf_exit_critical
	#define rtc_clk_xtal_freq_get mp_arch_rf_xtal_mhz

#endif
