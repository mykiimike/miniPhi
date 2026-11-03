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

#ifndef _HAVE_ESP32_RF_SERIES_H
	#define _HAVE_ESP32_RF_SERIES_H

	/* The series header defines MP_ARCH_RF_PARAM, MP_ARCH_RF_CAL,
	 * MP_ARCH_RF_UP, MP_ARCH_RF_MODE and MP_ARCH_RF_STATUS. */

	/**
	 * Power the radio clocks. The series keeps its register order.
	 * @param on TRUE brings the clocks up
	 */
	void mp_arch_rf_power(mp_bool_t on);

	/**
	 * @returns TRUE when the frequency block reports ready
	 */
	mp_ret_t mp_arch_rf_ready(void);

#endif
