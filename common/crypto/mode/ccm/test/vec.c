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

/* Host vectors. Not linked into the firmware.
 * One-shot AEAD. result 4 is a nonce or tag the API refuses.
 * A failed tag wipes plaintext. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_CIPHER
#define SUPPORT_COMMON_CRYPTO_CIPHER_AES
#ifndef MP_CRYPTO_AES_BITS
#define MP_CRYPTO_AES_BITS 128
#endif
#define SUPPORT_COMMON_CRYPTO_MAC
#define SUPPORT_COMMON_CRYPTO_MODE_CCM
#include <mp.h>
#include <crypto/aes.h>
#include <crypto/cipher.h>
#include <crypto/mac.h>
#include <crypto/ccm.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../cipher/aes/aes.c"
#include "../../../cipher/index.c"

/* The MAC module's other entry points need a hash this file does not build. */
mp_bool_t mp_crypto_mac_equal(const void *a, const void *b, unsigned int size) {
	const volatile uint8_t *x = a;
	const volatile uint8_t *y = b;
	uint8_t diff = 0;
	unsigned int n;

	for(n = 0; n < size; n++)
		diff |= x[n] ^ y[n];
	return(diff == 0 ? YES : NO);
}

#include "../ccm.c"

static int zeroed(const uint8_t *p, unsigned int n) {
	unsigned int i;

	for(i = 0; i < n; i++)
		if(p[i])
			return(0);
	return(1);
}

int main(void) {
	unsigned int i;
	unsigned int lens[7] = { 0, 1, 15, 16, 17, 33, 1009 };
	uint8_t ct[1100];
	uint8_t pt[1100];
	uint8_t tag[16];
	uint8_t key[32];
	uint8_t nonce[13];
	uint8_t msg[1100];
	mp_crypto_cipher_t cipher;
	int flipped = 0;

	for(i = 0; i < CCM_COUNT; i++) {
		unsigned int klen = ccm_vec[i].n[0];
		unsigned int nlen = ccm_vec[i].n[1];
		unsigned int aadlen = ccm_vec[i].n[2];
		unsigned int n = ccm_vec[i].n[3];
		unsigned int taglen = ccm_vec[i].n[5];
		int accepted = 0;
		const uint8_t *aad = aadlen ? ccm_at(i, 2) : NULL;
		const uint8_t *plain = n ? ccm_at(i, 3) : NULL;

		if(klen != MP_CRYPTO_AES_KEY)
			continue;
		if(mp_crypto_cipher_init(&cipher, "AES", ccm_at(i, 0), klen) != TRUE)
			return(host_fail("ccm init"));
		if(ccm_vec[i].result == 1) {
			if(mp_crypto_ccm_seal(&cipher, ccm_at(i, 1), nlen, aad, aadlen, plain, n, ct, tag, taglen) == TRUE && (n == 0 || memcmp(ct, ccm_at(i, 4), n) == 0) && taglen && memcmp(tag, ccm_at(i, 5), taglen) == 0) {
				mp_crypto_cipher_free(&cipher);
				if(mp_crypto_cipher_init(&cipher, "AES", ccm_at(i, 0), klen) != TRUE)
					return(host_fail("ccm init"));
				memset(pt, 0xa5, n ? n : 1);
				if(mp_crypto_ccm_open(&cipher, ccm_at(i, 1), nlen, aad, aadlen, n ? ct : NULL, n, tag, taglen, pt) == TRUE && (n == 0 || memcmp(pt, plain, n) == 0))
					accepted = 1;
			}
		}
		else {
			memset(pt, 0xa5, sizeof(pt));
			if(mp_crypto_ccm_open(&cipher, ccm_at(i, 1), nlen, aad, aadlen, n ? ccm_at(i, 4) : NULL, n, ccm_at(i, 5), taglen, pt) == TRUE)
				accepted = 1;
			else if(ccm_vec[i].result == 2 && n && !zeroed(pt, n)) {
				fprintf(stderr, "ccm kept plaintext %u\n", ccm_vec[i].id);
				return(1);
			}
		}
		mp_crypto_cipher_free(&cipher);
		if(host_expect("ccm", ccm_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", ccm_vec[i].id);
			return(1);
		}
		if(accepted && !flipped && n) {
			uint8_t bad[16];

			memcpy(bad, tag, taglen);
			bad[0] ^= 1;
			memset(pt, 0xa5, n);
			if(mp_crypto_cipher_init(&cipher, "AES", ccm_at(i, 0), klen) != TRUE)
				return(host_fail("ccm"));
			if(mp_crypto_ccm_open(&cipher, ccm_at(i, 1), nlen, aad, aadlen, ct, n, bad, taglen, pt) != FALSE || !zeroed(pt, n))
				return(host_fail("ccm tag"));
			mp_crypto_cipher_free(&cipher);
			flipped = 1;
		}
	}
	if(!flipped)
		return(host_fail("ccm no vector"));
	host_fill(key, sizeof(key));
	host_fill(nonce, sizeof(nonce));
	host_fill(msg, sizeof(msg));
	for(i = 0; i < 7; i++) {
		unsigned int n = lens[i];

		if(mp_crypto_cipher_init(&cipher, "AES", key, MP_CRYPTO_AES_KEY) != TRUE)
			return(host_fail("ccm len"));
		if(mp_crypto_ccm_seal(&cipher, nonce, 12, "aad", 3, msg, n, ct, tag, 8) != TRUE)
			return(host_fail("ccm len"));
		mp_crypto_cipher_free(&cipher);
		memset(pt, 0xa5, n ? n : 1);
		if(mp_crypto_cipher_init(&cipher, "AES", key, MP_CRYPTO_AES_KEY) != TRUE)
			return(host_fail("ccm len"));
		if(mp_crypto_ccm_open(&cipher, nonce, 12, "aad", 3, n ? ct : NULL, n, tag, 8, pt) != TRUE || (n && memcmp(pt, msg, n) != 0))
			return(host_fail("ccm len"));
		mp_crypto_cipher_free(&cipher);
		if(n) {
			ct[0] ^= 1;
			memset(pt, 0xa5, n);
			if(mp_crypto_cipher_init(&cipher, "AES", key, MP_CRYPTO_AES_KEY) != TRUE)
				return(host_fail("ccm len"));
			if(mp_crypto_ccm_open(&cipher, nonce, 12, "aad", 3, ct, n, tag, 8, pt) != FALSE || !zeroed(pt, n))
				return(host_fail("ccm ct"));
			mp_crypto_cipher_free(&cipher);
		}
	}
	host_report("ccm");
	return(0);
}
