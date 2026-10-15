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
#ifdef SUPPORT_COMMON_CRYPTO_MODE

#ifndef _HAVE_MP_COMMON_MODE_H
	#define _HAVE_MP_COMMON_MODE_H

	#include <stdint.h>

	/* cipher.h (mp_cipher_t) is included by mp.h before this header */

	/** the largest block any mode handles (matches the cipher) */
	#define MP_MODE_BLOCK_MAX 16

	/** the largest IV/nonce any mode takes (GCM = 12) */
	#define MP_MODE_IV_MAX 16

	/** the largest AEAD tag any mode writes (GCM = 16) */
	#define MP_MODE_TAG_MAX 16

	typedef struct mp_cipher_mode_algo_s mp_cipher_mode_algo_t;
	typedef struct mp_cipher_mode_s mp_cipher_mode_t;

	/** one mode of the build; the caller picks a started cipher to wrap */
	struct mp_cipher_mode_algo_s {
		/** module name, "CBC" */
		const char *name;
		/** display name, "CBC" */
		const char *label;
		/** IV bytes the mode expects (CBC/CTR = 16, GCM = 12) */
		unsigned int ivSize;
		/** AEAD tag bytes, 0 for a mode without authentication */
		unsigned int tagSize;

		/** FALSE when the IV is the wrong length or the cipher is not started;
		 *  encrypt is TRUE for encryption, FALSE for decryption (CBC needs it) */
		mp_ret_t (*init)(mp_cipher_mode_t *m, mp_cipher_t *cipher, const void *iv, unsigned int ivlen, mp_bool_t encrypt);
		/** bytes written to out */
		unsigned int (*update)(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out);
		/** bytes of associated data absorbed (AEAD only; 0 on a non-AEAD mode) */
		unsigned int (*update_aad)(mp_cipher_mode_t *m, const void *aad, unsigned int len);
		/** bytes written to out (padding, last block), and the tag to tag if AEAD */
		unsigned int (*final)(mp_cipher_mode_t *m, uint8_t *out, uint8_t *tag);
		/** wipes the mode state; safe on a mode already freed */
		void (*free)(mp_cipher_mode_t *m);
	};

	/**
	 * @brief Mode state, owned by the caller
	 *
	 * The fields are shared by the modes: each uses the ones it needs.
	 * buf holds a partial block; iv is the chain value (CBC) or the
	 * counter (CTR/GCM); auth is the GHASH accumulator (GCM) or the
	 * keystream block (CTR); tag holds the GCM tag or the hash subkey H.
	 */
	struct mp_cipher_mode_s {
		/** the chosen mode, NULL before init */
		const mp_cipher_mode_algo_t *algo;
		/** the caller's started cipher, used block by block */
		mp_cipher_t *cipher;
		/** bytes currently in buf */
		unsigned int leftover;
		/** a partial block (CBC padding, CTR/GCM tail) */
		uint8_t buf[MP_MODE_BLOCK_MAX];
		/** chain value (CBC) or counter (CTR/GCM) */
		uint8_t iv[MP_MODE_BLOCK_MAX];
		/** GHASH accumulator (GCM) or keystream (CTR) */
		uint8_t auth[MP_MODE_BLOCK_MAX];
		/** GCM tag, or the hash subkey H */
		uint8_t tag[MP_MODE_BLOCK_MAX];
		/** GCM precomputed tag mask E(J0) */
		uint8_t j0[MP_MODE_BLOCK_MAX];
		/** GCM keystream block (CTR over the data) */
		uint8_t ks[MP_MODE_BLOCK_MAX];
		/** GCM keystream bytes left in ks */
		unsigned int ksLeft;
		/** AAD length in bits (GCM) */
		unsigned long aadLen;
		/** plaintext/ciphertext length in bits (GCM) */
		unsigned long ctrLen;
		/** TRUE for encryption, FALSE for decryption (CBC needs it) */
		mp_bool_t encrypt;
		/** GCM phase: 0 = AAD, 1 = data, 2 = done */
		unsigned int phase;
	};

	/** @} */

	/** how many modes the build carries */
	int mp_cipher_mode_count(void);
	const mp_cipher_mode_algo_t *mp_cipher_mode_at(int index);
	/** "CBC", "cbc"...; NULL when the build does not carry it */
	const mp_cipher_mode_algo_t *mp_cipher_mode_find(const char *name);

	/** FALSE when name is not in the build, the IV is wrong or the cipher is
	 *  not started; the mode then wraps the cipher the caller started.
	 *  encrypt is TRUE for encryption, FALSE for decryption (CBC needs it) */
	mp_ret_t mp_cipher_mode_init(mp_cipher_mode_t *m, const char *name, mp_cipher_t *cipher, const void *iv, unsigned int ivlen, mp_bool_t encrypt);
	/** bytes written to out; processes whole blocks, buffers the tail */
	unsigned int mp_cipher_mode_update(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out);
	/** bytes of associated data absorbed; 0 on a mode without authentication */
	unsigned int mp_cipher_mode_aad(mp_cipher_mode_t *m, const void *aad, unsigned int len);
	/** bytes written to out (last block), and the tag to tag when AEAD */
	unsigned int mp_cipher_mode_final(mp_cipher_mode_t *m, uint8_t *out, uint8_t *tag);
	/** wipes the mode state; safe on a mode already freed */
	void mp_cipher_mode_free(mp_cipher_mode_t *m);

#endif
#endif
