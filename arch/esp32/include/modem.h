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

#ifndef _HAVE_ESP32_MODEM_H
	#define _HAVE_ESP32_MODEM_H

	/**
	 * Who holds the modem domain. The domain is the clocks shared by the
	 * PHY, the Wi-Fi MAC, BLE and 802.15.4. The first get runs the series
	 * power-up. The last put restores the words that power-up changed.
	 */
	typedef enum {
		MP_ARCH_MODEM_PHY = 0,
		MP_ARCH_MODEM_WIFI,
		MP_ARCH_MODEM_BLE,
		MP_ARCH_MODEM_IEEE154,
		MP_ARCH_MODEM_COUNT
	} mp_arch_modem_user_t;

	/**
	 * Take the modem domain for this user.
	 * A second get from the same user does not power the domain again.
	 * @param user One mp_arch_modem_user_t
	 * @returns FALSE when user is not one of them
	 */
	mp_ret_t mp_arch_modem_get(mp_arch_modem_user_t user);

	/**
	 * Release the domain. A user that does not hold it returns.
	 * @param user The same user passed to mp_arch_modem_get
	 */
	void mp_arch_modem_put(mp_arch_modem_user_t user);

	/** How many distinct users hold the domain. */
	int mp_arch_modem_users(void);

	/**
	 * Series power-up. modem.c calls this on the first get.
	 * The series saves every word it changes.
	 */
	void mp_arch_modem_on(void);

	/**
	 * Series power-down. modem.c calls this on the last put.
	 * The series writes back the words saved by mp_arch_modem_on.
	 */
	void mp_arch_modem_off(void);

#endif
