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
#ifdef SUPPORT_COMMON_CRYPTO_HASH

#ifndef _HAVE_MP_COMMON_HASH_H
	#define _HAVE_MP_COMMON_HASH_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonHash
	 * @{
	 */

	/** the largest digest any algorithm writes (SHA-512) */
	#define MP_HASH_SIZE_MAX 64

	/** the largest block of the algorithms in the build */
	#if defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384) || defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512)
		#define MP_HASH_BLOCK_MAX 128
	#else
		#define MP_HASH_BLOCK_MAX 64
	#endif

	typedef struct mp_hash_algo_s mp_hash_algo_t;

	/** one algorithm of the build */
	struct mp_hash_algo_s {
		/** module name, "SHA256" */
		const char *name;
		/** display name, "SHA-256" */
		const char *label;
		/** digest bytes */
		unsigned int size;
		/** block bytes */
		unsigned int block;

		void (*init)(void *ctx);
		void (*update)(void *ctx, const void *data, unsigned int size);
		void (*final)(void *ctx, uint8_t *digest);
	};

	typedef struct mp_hash_s mp_hash_t;

	struct mp_hash_s {
		/** NULL before init, after digest and after free */
		const mp_hash_algo_t *algo;

		/** as large as the largest algorithm in the build */
		union {
		#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
			mp_blake2s_t blake2s;
		#endif
		#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA1
			mp_sha1_t sha1;
		#endif
		#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA256
			mp_sha256_t sha256;
		#endif
		#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA384
			mp_sha384_t sha384;
		#endif
		#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512
			mp_sha512_t sha512;
		#endif
			uint8_t none;
		} ctx;
	};

	/** @} */

	/** how many algorithms the build carries */
	int mp_hash_count(void);
	const mp_hash_algo_t *mp_hash_at(int index);
	/** "SHA256", "sha-256"...; NULL when the build does not carry it */
	const mp_hash_algo_t *mp_hash_find(const char *name);

	/** FALSE when name is not in the build; hash is then cleared */
	mp_ret_t mp_hash_init(mp_hash_t *hash, const char *name);
	void mp_hash_update(mp_hash_t *hash, const void *data, unsigned int size);
	/**
	 * Writes the digest, at most MP_HASH_SIZE_MAX bytes, and returns its size.
	 * 0 when hash was not started. The context is wiped afterwards.
	 */
	unsigned int mp_hash_digest(mp_hash_t *hash, uint8_t *digest);
	/** wipes the context; for a hash that will not reach mp_hash_digest() */
	void mp_hash_free(mp_hash_t *hash);
	/** digest bytes of a started hash, 0 otherwise */
	unsigned int mp_hash_size(const mp_hash_t *hash);

#endif
#endif
