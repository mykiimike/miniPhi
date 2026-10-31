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
#ifdef SUPPORT_COMMON_CRYPTO_KDF

#ifndef _HAVE_MP_COMMON_KDF_H
	#define _HAVE_MP_COMMON_KDF_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonKdf KDF API
	 * @{
	 */

	/** @} */

	/**
	 * One shot. name is "HKDF-" and a hash of the build, any case: "HKDF-SHA512", "hkdf-sha-256".
	 * Returns L, or 0 when the build does not carry that derivation or L does not fit.
	 */
	unsigned int mp_crypto_kdf(const char *name, const void *salt, unsigned int saltlen, const void *ikm, unsigned int ikmlen, const void *info, unsigned int infolen, uint8_t *out, unsigned int L);

#endif
#endif
