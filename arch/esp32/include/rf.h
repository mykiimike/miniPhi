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

#ifndef _HAVE_ESP32_RF_H
	#define _HAVE_ESP32_RF_H

	/**
	 * Bring the radio clocks and the channel synthesizer up.
	 * arch/esp32/rf/rf.c defines this. Boot does not call it.
	 * @param kernel Running kernel
	 * @returns TRUE when the frequency block reports ready
	 */
	mp_ret_t mp_arch_rf_open(mp_kernel_t *kernel);

	/**
	 * @param mhz Channel frequency in MHz
	 * @returns TRUE when the synthesizer accepted the frequency
	 */
	mp_ret_t mp_arch_rf_channel(unsigned long mhz);

	/** Power the radio down. A stopped radio returns. */
	void mp_arch_rf_close(void);

#endif
