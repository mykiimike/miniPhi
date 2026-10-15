/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_AES

#ifndef _HAVE_MP_COMMON_AES_H
	#define _HAVE_MP_COMMON_AES_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonAes
	 * @{
	 */

	/** AES block size (bytes) */
	#define MP_AES_BLOCK 16

	/* AES-128: 16, AES-192: 24, AES-256: 32 */
	#if MP_AES_BITS == 128
		#define MP_AES_KEY 16
	#elif MP_AES_BITS == 192
		#define MP_AES_KEY 24
	#elif MP_AES_BITS == 256
		#define MP_AES_KEY 32
	#else
		#error MP_AES_BITS must be 128, 192 or 256
	#endif

	/* the arch header defines mp_arch_aes_t, the state of one hardware cipher */
	#ifdef MP_ARCH_AES
		#include <arch/aes.h>
	#endif

	/**
	 * @brief AES context, owned by the caller
	 *
	 * The round-key array is sized for the largest key (AES-256, 14 rounds
	 * plus one), so a smaller key wastes the tail. The software version
	 * below always builds.
	 */
	typedef struct mp_aes_s mp_aes_t;

	struct mp_aes_s {
		/** number of rounds for this key (10, 12 or 14) */
		unsigned int rounds;
		/** key length in bytes (16, 24 or 32) */
		unsigned int keylen;
		/** round keys, the schedule of the chosen key */
		uint32_t roundKeys[(14 + 1) * 4];

	#ifdef MP_ARCH_AES
		mp_bool_t onHardware;
		mp_arch_aes_t hw;
	#endif
	};

	/** @} */

	/** FALSE when the key length is not 16, 24 or 32 */
	mp_ret_t mp_aes_init(mp_aes_t *ctx, const void *key, unsigned int keylen);
	/** bytes written, at most MP_AES_BLOCK, 0 when the cipher was not started */
	unsigned int mp_aes_update_block(mp_aes_t *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** bytes written, at most MP_AES_BLOCK, 0 when the cipher was not started;
	 *  the inverse cipher is used by the modes that need to decrypt (CBC, GCM) */
	unsigned int mp_aes_update_block_inv(mp_aes_t *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** wipes the context; safe on a cipher already freed */
	void mp_aes_free(mp_aes_t *ctx);

	/* an arch with an AES accelerator provides these; init returns NO when
	 * the block is busy or off, and the context then runs in software */
	#ifdef MP_ARCH_AES
		mp_bool_t mp_arch_aes_init(mp_arch_aes_t *hw, const void *key, unsigned int keylen);
		void mp_arch_aes_update_block(mp_arch_aes_t *hw, const void *in, unsigned int inlen, uint8_t *out);
	#endif

#endif
#endif
