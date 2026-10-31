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
#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST

#ifndef _HAVE_MP_COMMON_SELFTEST_H
	#define _HAVE_MP_COMMON_SELFTEST_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonSelftest
	 * @{
	 */

	typedef struct mp_crypto_selftest_s mp_crypto_selftest_t;

	/** one known-answer check the build kept */
	struct mp_crypto_selftest_s {
		/** module name, "AES" */
		const char *name;
		const char *label;
		/** cipher, hash, mac or kem */
		const char *family;
		/** YES when the known answer matches */
		mp_bool_t (*run)(void);
	};

	/** @} */

	int mp_crypto_selftest_count(void);
	const mp_crypto_selftest_t *mp_crypto_selftest_at(int index);
	/** NULL when this build left that algorithm's self-test off, or does not carry the algorithm */
	const mp_crypto_selftest_t *mp_crypto_selftest_find(const char *name);
	/**
	 * Runs the known answer of name ("AES", "SHA256").
	 * YES when it matches. NO when it does not, or when that self-test is not in this build.
	 * Prints the name, then passed or failed and the microseconds the check took.
	 * The call is the app's: nothing here runs at boot.
	 * The check uses the public API, so an accelerator that accepted the operation is what is checked.
	 */
	mp_bool_t mp_crypto_selftest(const char *name);
	/** microseconds of the last check, the comparison only */
	unsigned long mp_crypto_selftest_last_us(void);

	/** YES when the n bytes match. The expected answer is public. */
	static inline mp_bool_t mp_crypto_selftest_equal(const void *got, const void *expect, unsigned int n) {
		const uint8_t *a = got;
		const uint8_t *b = expect;
		unsigned int i;
		uint8_t diff = 0;

		if(!a || !b)
			return(NO);
		for(i = 0; i < n; i++)
			diff |= (uint8_t)(a[i] ^ b[i]);
		return(diff == 0 ? YES : NO);
	}

	#if defined(MP_CRYPTO_SELFTEST_AES) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES)
		mp_bool_t mp_crypto_selftest_aes(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_CHACHA20) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20)
		mp_bool_t mp_crypto_selftest_chacha20(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES)
		mp_bool_t mp_crypto_selftest_xts(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_SHA1) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1)
		mp_bool_t mp_crypto_selftest_sha1(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_SHA256) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256)
		mp_bool_t mp_crypto_selftest_sha256(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_SHA384) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384)
		mp_bool_t mp_crypto_selftest_sha384(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_SHA512) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512)
		mp_bool_t mp_crypto_selftest_sha512(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_BLAKE2S) && defined(SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S)
		mp_bool_t mp_crypto_selftest_blake2s(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_SHAKE) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHAKE)
		mp_bool_t mp_crypto_selftest_shake(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_HMAC) && defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256)
		mp_bool_t mp_crypto_selftest_hmac(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_POLY1305) && defined(SUPPORT_COMMON_CRYPTO_MAC_POLY1305)
		mp_bool_t mp_crypto_selftest_poly1305(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_MLKEM) && defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM)
		mp_bool_t mp_crypto_selftest_mlkem(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_HQC) && defined(SUPPORT_COMMON_CRYPTO_KEM_HQC)
		mp_bool_t mp_crypto_selftest_hqc(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_X25519) && defined(SUPPORT_COMMON_CRYPTO_KEM_X25519)
		mp_bool_t mp_crypto_selftest_x25519(void);
	#endif
	#if defined(MP_CRYPTO_SELFTEST_RSAKEM) && defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && defined(SUPPORT_COMMON_CRYPTO_SIGN_RSA)
		mp_bool_t mp_crypto_selftest_rsakem(void);
	#endif

#endif
#endif
