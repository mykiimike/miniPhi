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

#ifndef _HAVE_ESP32_RF_PARAM_H
	#define _HAVE_ESP32_RF_PARAM_H

	#include <string.h>

	extern uint8_t phy_param[MP_ARCH_RF_PARAM];

	/**
	 * @param off Byte offset in phy_param
	 * @returns The byte
	 */
	static inline uint8_t mp_arch_rf_u8(unsigned off) {
		uint8_t v;

		memcpy(&v, phy_param + off, sizeof(v));
		return(v);
	}

	/**
	 * @param off Byte offset in phy_param
	 * @returns The halfword stored there
	 */
	static inline uint16_t mp_arch_rf_u16(unsigned off) {
		uint16_t v;

		memcpy(&v, phy_param + off, sizeof(v));
		return(v);
	}

	/**
	 * @param off Byte offset in phy_param
	 * @returns The word stored there
	 */
	static inline uint32_t mp_arch_rf_u32(unsigned off) {
		uint32_t v;

		memcpy(&v, phy_param + off, sizeof(v));
		return(v);
	}

	/**
	 * @param off Byte offset in phy_param
	 * @param v Byte to store
	 */
	static inline void mp_arch_rf_set_u8(unsigned off, uint8_t v) {
		memcpy(phy_param + off, &v, sizeof(v));
	}

	/**
	 * @param off Byte offset in phy_param
	 * @param v Halfword to store
	 */
	static inline void mp_arch_rf_set_u16(unsigned off, uint16_t v) {
		memcpy(phy_param + off, &v, sizeof(v));
	}

	/**
	 * @param off Byte offset in phy_param
	 * @param v Word to store
	 */
	static inline void mp_arch_rf_set_u32(unsigned off, uint32_t v) {
		memcpy(phy_param + off, &v, sizeof(v));
	}

	/**
	 * @param off Byte offset in phy_param
	 * @returns Pointer to that byte
	 */
	static inline uint8_t *mp_arch_rf_addr(unsigned off) {
		return(phy_param + off);
	}

#endif
