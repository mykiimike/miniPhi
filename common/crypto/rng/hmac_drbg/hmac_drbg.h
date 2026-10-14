/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 * Copyright (C) 2026  VERMAN                                              *
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
#ifdef SUPPORT_COMMON_CRYPTO_RNG_HMAC_DRBG

#ifndef _HAVE_MP_COMMON_HMAC_DRBG_H
	#define _HAVE_MP_COMMON_HMAC_DRBG_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonHmacDrbg
	 * @{
	 */

	/** bytes one mp_hmac_drbg_generate() may return (NIST: 2^19 bits) */
	#define MP_HMAC_DRBG_MAX (1u << 16)

	/** generates before the next one asks for mp_hmac_drbg_reseed(); NIST allows 2^48 */
	#define MP_HMAC_DRBG_RESEED (1u << 20)

	typedef struct mp_hmac_drbg_s mp_hmac_drbg_t;

	struct mp_hmac_drbg_s {
		/** NULL before init and after free */
		const mp_hash_algo_t *algo;

		/** HMAC key, algo->size bytes */
		uint8_t key[MP_HASH_SIZE_MAX];

		/** chaining value, algo->size bytes */
		uint8_t v[MP_HASH_SIZE_MAX];

		/** successful generates since the last seed; 1 after init and reseed */
		uint32_t reseed;
	};

	/** @} */

	/**
	 * hash is a name of HASH. entropy is required. nonce and personal may be NULL
	 * with a length of 0: NIST wants a nonce, so pass one unless the entropy already
	 * carries it. FALSE when the build does not carry hash or entropy is empty.
	 */
	mp_ret_t mp_hmac_drbg_init(mp_hmac_drbg_t *ctx, const char *hash, const void *entropy, unsigned int entropylen, const void *nonce, unsigned int noncelen, const void *personal, unsigned int personlen);
	/** mixes new entropy in. FALSE when ctx is not started or entropy is empty; ctx is then unchanged */
	mp_ret_t mp_hmac_drbg_reseed(mp_hmac_drbg_t *ctx, const void *entropy, unsigned int entropylen, const void *additional, unsigned int addlen);
	/**
	 * Writes size bytes, at most MP_HMAC_DRBG_MAX. additional may be NULL with a length of 0.
	 * FALSE when a reseed is due, size is 0 or too big, or ctx is not started; out is then unchanged.
	 */
	mp_ret_t mp_hmac_drbg_generate(mp_hmac_drbg_t *ctx, void *out, unsigned int size, const void *additional, unsigned int addlen);
	/** wipes the context, the key with it */
	void mp_hmac_drbg_free(mp_hmac_drbg_t *ctx);

#endif
#endif
