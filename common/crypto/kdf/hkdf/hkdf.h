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
#ifdef SUPPORT_COMMON_CRYPTO_KDF_HKDF

#ifndef _HAVE_MP_COMMON_HKDF_H
	#define _HAVE_MP_COMMON_HKDF_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonHkdf HKDF
	 * @{
	 */

	/** longest output: 255 digests of the largest hash */
	#define MP_CRYPTO_HKDF_SIZE_MAX (255u * MP_CRYPTO_HASH_SIZE_MAX)

	/** @} */

	/**
	 * HKDF-Extract (RFC 5869). hash is a name of HASH, "SHA256" or "SHA-512".
	 * A salt of length 0 is HashLen zero bytes.
	 * Writes the PRK and returns its length, the digest size. 0 when hash is absent.
	 */
	unsigned int mp_crypto_hkdf_extract(const char *hash, const void *salt, unsigned int saltlen, const void *ikm, unsigned int ikmlen, uint8_t *prk);
	/**
	 * HKDF-Expand. L is the output length, at most 255 digests.
	 * Returns L, or 0 when hash is absent or L does not fit.
	 */
	unsigned int mp_crypto_hkdf_expand(const char *hash, const void *prk, unsigned int prklen, const void *info, unsigned int infolen, uint8_t *out, unsigned int L);
	/**
	 * Extract, then expand. Returns L, or 0.
	 */
	unsigned int mp_crypto_hkdf(const char *hash, const void *salt, unsigned int saltlen, const void *ikm, unsigned int ikmlen, const void *info, unsigned int infolen, uint8_t *out, unsigned int L);

#endif
#endif
