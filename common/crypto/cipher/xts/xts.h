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
#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_XTS

#ifndef _HAVE_MP_COMMON_XTS_H
	#define _HAVE_MP_COMMON_XTS_H

	#include <crypto/aes.h>

	/**
	 * @defgroup mpCommonXts
	 * @{
	 */

	/** XTS block, and the tweak (bytes) */
	#define MP_CRYPTO_XTS_BLOCK 16

	/** @} */

	/**
	 * @brief XTS-AES context, owned by the caller
	 *
	 * Two AES schedules: Key1 encrypts the data, Key2 encrypts the tweak.
	 * The software schedules always fit. When the build has an AES
	 * accelerator, each schedule uses it if the arch accepts that key.
	 */
	typedef struct mp_crypto_xts_s mp_crypto_xts_t;

	struct mp_crypto_xts_s {
		mp_crypto_aes_t data;
		mp_crypto_aes_t tweak;
		/** AES-enc(Key2, tweak), the start of a data unit */
		uint8_t T[MP_CRYPTO_XTS_BLOCK];
		/** YES after mp_crypto_xts_set_tweak() */
		mp_bool_t armed;
	};

	/**
	 * FALSE when the key is not 32 or 64 bytes. 32 is two AES-128 keys,
	 * 64 is two AES-256 keys, Key1 then Key2.
	 */
	mp_ret_t mp_crypto_xts_init(mp_crypto_xts_t *ctx, const void *key, unsigned int keylen);
	/** 16-byte tweak, the little-endian data-unit number. FALSE when len is not 16. */
	mp_ret_t mp_crypto_xts_set_tweak(mp_crypto_xts_t *ctx, const void *tweak, unsigned int len);
	/**
	 * Encrypts one data unit. len is at least 16. A length that is not a
	 * multiple of 16 uses ciphertext stealing. Returns len, or 0 when the
	 * context has no tweak or len is under 16. in and out may be the same buffer.
	 */
	unsigned int mp_crypto_xts_update(mp_crypto_xts_t *ctx, const void *in, unsigned int len, uint8_t *out);
	/** Decrypts one data unit. Same length rules as mp_crypto_xts_update(). */
	unsigned int mp_crypto_xts_update_inv(mp_crypto_xts_t *ctx, const void *in, unsigned int len, uint8_t *out);
	/** wipes the context; safe on a cipher already freed */
	void mp_crypto_xts_free(mp_crypto_xts_t *ctx);

	#if defined(MP_ARCH_XTS) && defined(SUPPORT_COMMON_CRYPTO_FUSE)
		#include <crypto/fuse.h>
		/**
		 * Encrypts one flash line on the XTS accelerator. len is 16, 32 or 64
		 * and address is aligned to it. fuse is a slot of role MP_CRYPTO_FUSE_XTS.
		 * Returns len, or 0 when the slot or the block refuses; out is unchanged then.
		 */
		unsigned int mp_crypto_xts_crypt(const mp_crypto_fuse_t *fuse, unsigned long address, const void *in, unsigned int len, void *out);
		unsigned int mp_arch_xts_crypt(unsigned int slot, unsigned long address, const void *in, unsigned int len, void *out);
	#endif

#endif
#endif
