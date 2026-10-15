/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER

#ifndef _HAVE_MP_COMMON_CIPHER_H
	#define _HAVE_MP_COMMON_CIPHER_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonCipher
	 * @{
	 */

	/** the largest block any cipher accepts (AES = 16) */
	#define MP_CIPHER_BLOCK_MAX 16

	/** the largest key any cipher accepts (AES-256 = 32) */
	#define MP_CIPHER_KEY_MAX 32

	typedef struct mp_cipher_algo_s mp_cipher_algo_t;

	/** one cipher of the build */
	struct mp_cipher_algo_s {
		/** module name, "AES" */
		const char *name;
		/** display name, "AES" */
		const char *label;
		/** block bytes (AES = 16) */
		unsigned int block;
		/** key bytes (AES-128 = 16, AES-256 = 32) */
		unsigned int keySize;

		/** FALSE when name is unknown or the key is the wrong length */
		mp_ret_t (*init)(void *ctx, const void *key, unsigned int keylen);
	/** bytes written, at most the block, 0 when the key was not started */
	unsigned int (*update_block)(void *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** bytes written, at most the block, 0 when the key was not started;
	 *  the inverse cipher is used by the modes that need to decrypt (CBC, GCM) */
	unsigned int (*update_block_inv)(void *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** wipes the context; safe on a cipher that will not reach update_block again */
	void (*free)(void *ctx);
	};

	/** @} */

	/* the cipher context types the build carries are included by mp.h before
	 * this header, so the union below can hold any of them */

	/**
	 * @defgroup mpCipherCtx
	 * @{
	 */

	/** user-facing cipher context: the chosen algorithm plus room for any
	 *  cipher context the build carries. Owned by the caller. */
	typedef struct mp_cipher_s mp_cipher_t;

	struct mp_cipher_s {
		/** the chosen algorithm, NULL before init */
		const mp_cipher_algo_t *algo;
		/** room for any cipher context of the build */
		union {
	#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_AES
			mp_aes_t aes;
	#endif
			uint8_t raw[MP_CIPHER_KEY_MAX * 4];
		} ctx;
	};

	/** @} */

	/** how many ciphers the build carries */
	int mp_cipher_count(void);
	const mp_cipher_algo_t *mp_cipher_at(int index);
	/** "AES", "aes"...; NULL when the build does not carry it */
	const mp_cipher_algo_t *mp_cipher_find(const char *name);

	/** FALSE when name is not in the build or the key length is wrong */
	mp_ret_t mp_cipher_init(mp_cipher_t *ctx, const char *name, const void *key, unsigned int keylen);
	/** bytes written, at most the block, 0 when the cipher was not started */
	unsigned int mp_cipher_update_block(mp_cipher_t *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** bytes written, at most the block, 0 when the cipher was not started;
	 *  the inverse cipher is used by the modes that need to decrypt (CBC, GCM) */
	unsigned int mp_cipher_update_block_inv(mp_cipher_t *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** wipes the context; safe on a cipher that will not be used again */
	void mp_cipher_free(mp_cipher_t *ctx);

#endif
#endif
