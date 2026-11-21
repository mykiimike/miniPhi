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
#ifdef SUPPORT_COMMON_CRYPTO_LOCK

#ifndef _HAVE_MP_COMMON_CRYPTO_LOCK_H
	#define _HAVE_MP_COMMON_CRYPTO_LOCK_H

	/**
	 * @defgroup mpCommonCryptoLock
	 * @{
	 */

	/**
	 * One holder at a time for the asymmetric scratch. Recursive: a public
	 * operation may call another, and RSA key generation calls the prime test.
	 * An interrupt must not call this. The lock spins, and it raises the task ceiling.
	 */
	typedef mp_task_lock_t mp_crypto_lock_t;

	/** @} */

	mp_crypto_lock_t mp_crypto_lock(void);
	void mp_crypto_unlock(mp_crypto_lock_t key);

#endif
#endif
