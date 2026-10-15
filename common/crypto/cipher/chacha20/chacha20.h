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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20

#ifndef _HAVE_MP_COMMON_CHACHA20_H
	#define _HAVE_MP_COMMON_CHACHA20_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonChacha20 ChaCha20
	 * @ingroup mpCommon
	 * @brief ChaCha20 stream cipher (RFC 8439)
	 * @{
	 */

	/** ChaCha20 keystream block (bytes) */
	#define MP_CHACHA20_BLOCK 64

	/** ChaCha20 key (bytes) */
	#define MP_CHACHA20_KEY 32

	/** ChaCha20 nonce (bytes), the RFC 8439 96-bit nonce */
	#define MP_CHACHA20_NONCE 12

	typedef struct mp_chacha20_s mp_chacha20_t;

	/** ChaCha20 context, owned by the caller. A stream cipher: the
	 *  keystream is buffered so any length can be XORed. */
	struct mp_chacha20_s {
		/** 256-bit key */
		uint8_t key[MP_CHACHA20_KEY];
		/** 96-bit nonce */
		uint8_t nonce[MP_CHACHA20_NONCE];
		/** 32-bit block counter */
		uint32_t counter;
		/** buffered keystream block */
		uint8_t ks[MP_CHACHA20_BLOCK];
		/** bytes left in ks */
		unsigned int ksLeft;
	};

	/** FALSE when the key length is not 32 */
	mp_ret_t mp_chacha20_init(mp_chacha20_t *ctx, const void *key, unsigned int keylen);
	/** set the 96-bit nonce (call before the first update_block) */
	void mp_chacha20_set_nonce(mp_chacha20_t *ctx, const void *nonce, unsigned int noncelen);
	/** set the 32-bit block counter (1 for RFC 8439 data, 0 for the AEAD key block) */
	void mp_chacha20_set_counter(mp_chacha20_t *ctx, uint32_t counter);
	/** bytes written, at most inlen; XORs in with the keystream (encrypt = decrypt) */
	unsigned int mp_chacha20_update_block(mp_chacha20_t *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** bytes written, at most inlen; the same as update_block (stream cipher) */
	unsigned int mp_chacha20_update_block_inv(mp_chacha20_t *ctx, const void *in, unsigned int inlen, uint8_t *out);
	/** wipes the context; safe on a cipher already freed */
	void mp_chacha20_free(mp_chacha20_t *ctx);

	/** @} */

#endif
#endif
