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

#ifndef _HAVE_ESP32_RF_I2C_H
	#define _HAVE_ESP32_RF_I2C_H

	#include "host.h"

	typedef struct {
		unsigned long base;       /* master base: C6 0x600AF800 */
		unsigned long sel_off;    /* host select register offset: C6 0x20 */
		unsigned long sel_mask;   /* bits kept: C6 0xffffe00f */
		unsigned long sel_value;  /* bits set: C6 0x1f00 */
		unsigned long mst1_hosts; /* bit (host-0x62) set -> master 1: C6 0x223 */
		unsigned long rmask_off;  /* read mask register, 0 when absent: C6 0x1c */
	} mp_arch_rf_i2c_t;

	extern const mp_arch_rf_i2c_t mp_arch_rf_i2c;

	/**
	 * Select the analog master and report which master owns the block.
	 * @param m Series master
	 * @param block Analog block id
	 * @returns 1 when the block sits on master 1, otherwise 0
	 */
	static inline int mp_arch_rf_i2c_hostid(const mp_arch_rf_i2c_t *m, int block) {
		unsigned long addr;
		unsigned long sel;

		addr = m->base + m->sel_off;
		sel = mp_arch_rf_read(addr);
		mp_arch_rf_write(addr, (sel & m->sel_mask) | m->sel_value);
		return((block - 0x62U & 0xff) < 10 && (1 << (block - 0x62U & 0x1f) & m->mst1_hosts) != 0);
	}

	/**
	 * Read one analog register. Polls bit 25, then loads the result.
	 * @param m Series master
	 * @param block Analog block id
	 * @param rmask Read mask written inverted when the series has that register
	 * @param host Master host index
	 * @param reg Register inside the block
	 * @returns Bits 23:16 of the result word
	 */
	static inline uint32_t mp_arch_rf_i2c_read(const mp_arch_rf_i2c_t *m, uint32_t block, uint32_t rmask, int host, int reg) {
		unsigned long addr;
		unsigned long word;

		if(m->rmask_off != 0)
			mp_arch_rf_write(m->base + m->rmask_off, ~rmask);
		addr = m->base + (unsigned long)host * 4UL;
		mp_arch_rf_write(addr, (unsigned long)(reg << 8) | block | 0x04000000UL);
		do {
			word = mp_arch_rf_read(addr);
		} while((int)(word << 6) < 0);
		word = mp_arch_rf_read(addr);
		return(word >> 0x10 & 0xff);
	}

#endif
