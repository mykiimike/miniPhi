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
 * The mode API takes one build key size. IV is 12 bytes.
 * final() writes the tag and does not wipe plaintext; the caller compares tags. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_CIPHER
#define SUPPORT_COMMON_CRYPTO_CIPHER_AES
#ifndef MP_CRYPTO_AES_BITS
#define MP_CRYPTO_AES_BITS 128
#endif
#define SUPPORT_COMMON_CRYPTO_MODE
#define SUPPORT_COMMON_CRYPTO_MODE_GCM
#include <mp.h>
#include <crypto/aes.h>
#include <crypto/cipher.h>
#include <crypto/mode.h>
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
#include "../gcm.c"
#include "../../index.c"

static uint8_t key[32];
static uint8_t iv[12];
static uint8_t msg[1100];
static const uint8_t aad_fixed[7] = { 1, 2, 3, 4, 5, 6, 7 };

static int gcm(int enc, const uint8_t *k, unsigned int klen, const uint8_t *v, unsigned int vlen, const uint8_t *aad, unsigned int aadlen, const uint8_t *in, unsigned int len, const unsigned int *cuts, unsigned int ncuts, uint8_t *out, uint8_t *tag) {
	mp_crypto_cipher_t cipher;
	mp_crypto_cipher_mode_t mode;
	unsigned int pos = 0;
	unsigned int i;
	unsigned int n;
	unsigned int one;

	if(mp_crypto_cipher_init(&cipher, "AES", k, klen) != TRUE)
		return(-1);
	if(mp_crypto_cipher_mode_init(&mode, "GCM", &cipher, v, vlen, enc ? TRUE : FALSE) != TRUE) {
		mp_crypto_cipher_free(&cipher);
		return(-1);
	}
	if(aadlen && mp_crypto_cipher_mode_aad(&mode, aad, aadlen) != aadlen) {
		mp_crypto_cipher_mode_free(&mode);
		mp_crypto_cipher_free(&cipher);
		return(-1);
	}
	if(!ncuts) {
		one = len;
		cuts = &one;
		ncuts = 1;
	}
	for(i = 0; i < ncuts; i++) {
		n = mp_crypto_cipher_mode_update(&mode, in + pos, cuts[i], out + pos);
		if(n != cuts[i]) {
			mp_crypto_cipher_mode_free(&mode);
			mp_crypto_cipher_free(&cipher);
			return(-1);
		}
		pos += cuts[i];
	}
	mp_crypto_cipher_mode_final(&mode, NULL, tag);
	mp_crypto_cipher_mode_free(&mode);
	mp_crypto_cipher_free(&cipher);
	return(0);
}

static int run(unsigned int len, const unsigned int *cuts, unsigned int ncuts, void *ud) {
	uint8_t a[1100];
	uint8_t b[1100];
	uint8_t ta[16];
	uint8_t tb[16];

	(void)ud;
	if(gcm(1, key, MP_CRYPTO_AES_KEY, iv, 12, aad_fixed, sizeof(aad_fixed), msg, len, NULL, 0, a, ta))
		return(host_fail("gcm"));
	if(gcm(1, key, MP_CRYPTO_AES_KEY, iv, 12, aad_fixed, sizeof(aad_fixed), msg, len, cuts, ncuts, b, tb))
		return(host_fail("gcm"));
	if((len && memcmp(a, b, len) != 0) || memcmp(ta, tb, 16) != 0)
		return(host_fail("gcm"));
	return(0);
}

int main(void) {
	unsigned int i;
	unsigned int off;
	uint8_t ct[600];
	uint8_t pt[600];
	uint8_t tag[16];
	uint8_t tag2[16];
	uint8_t aad[40];
	int flipped = 0;

	for(i = 0; i < GCM_COUNT; i++) {
		unsigned int klen = gcm_vec[i].n[0];
		unsigned int n = gcm_vec[i].n[3];
		int accepted = 0;

		if(klen != MP_CRYPTO_AES_KEY)
			continue;
		if(gcm_vec[i].n[1] != 12 || gcm_vec[i].n[5] != 16)
			return(host_fail("gcm shape"));
		if(gcm(1, gcm_at(i, 0), klen, gcm_at(i, 1), 12, gcm_at(i, 2), gcm_vec[i].n[2], gcm_at(i, 3), n, NULL, 0, ct, tag) == 0)
			accepted = (n == 0 || memcmp(ct, gcm_at(i, 4), n) == 0) && memcmp(tag, gcm_at(i, 5), 16) == 0;
		if(host_expect("gcm", gcm_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", gcm_vec[i].id);
			return(1);
		}
		if(accepted && !flipped && n) {
			uint8_t bad[600];
			uint8_t badiv[12];
			uint8_t badaad[600];

			memcpy(bad, ct, n);
			bad[0] ^= 1;
			if(gcm(0, gcm_at(i, 0), klen, gcm_at(i, 1), 12, gcm_at(i, 2), gcm_vec[i].n[2], bad, n, NULL, 0, pt, tag2) != 0 || memcmp(tag2, gcm_at(i, 5), 16) == 0)
				return(host_fail("gcm ct"));
			memcpy(bad, tag, 16);
			bad[0] ^= 1;
			if(gcm(0, gcm_at(i, 0), klen, gcm_at(i, 1), 12, gcm_at(i, 2), gcm_vec[i].n[2], ct, n, NULL, 0, pt, tag2) != 0 || memcmp(tag2, bad, 16) == 0)
				return(host_fail("gcm tag"));
			memcpy(badiv, gcm_at(i, 1), 12);
			badiv[0] ^= 1;
			if(gcm(0, gcm_at(i, 0), klen, badiv, 12, gcm_at(i, 2), gcm_vec[i].n[2], ct, n, NULL, 0, pt, tag2) != 0 || memcmp(tag2, tag, 16) == 0)
				return(host_fail("gcm iv"));
			if(gcm_vec[i].n[2]) {
				memcpy(badaad, gcm_at(i, 2), gcm_vec[i].n[2]);
				badaad[0] ^= 1;
				if(gcm(0, gcm_at(i, 0), klen, gcm_at(i, 1), 12, badaad, gcm_vec[i].n[2], ct, n, NULL, 0, pt, tag2) != 0 || memcmp(tag2, tag, 16) == 0)
					return(host_fail("gcm aad"));
			}
			flipped = 1;
		}
	}
	if(!flipped)
		return(host_fail("gcm no vector"));
	host_fill(key, MP_CRYPTO_AES_KEY);
	host_fill(iv, sizeof(iv));
	host_fill(msg, sizeof(msg));
	host_fill(aad, sizeof(aad));
	if(gcm(1, key, MP_CRYPTO_AES_KEY, iv, 8, NULL, 0, msg, 16, NULL, 0, ct, tag) == 0)
		return(host_fail("gcm short iv"));
	host_invalid_n++;
	if(gcm(1, key, MP_CRYPTO_AES_KEY, iv, 12, aad, sizeof(aad), msg, 16, NULL, 0, ct, tag))
		return(host_fail("gcm aad one"));
	for(off = 0; off <= sizeof(aad); off++) {
		mp_crypto_cipher_t cipher;
		mp_crypto_cipher_mode_t mode;

		if(mp_crypto_cipher_init(&cipher, "AES", key, MP_CRYPTO_AES_KEY) != TRUE)
			return(host_fail("gcm aad"));
		if(mp_crypto_cipher_mode_init(&mode, "GCM", &cipher, iv, 12, TRUE) != TRUE)
			return(host_fail("gcm aad"));
		if(mp_crypto_cipher_mode_aad(&mode, aad, off) != off || mp_crypto_cipher_mode_aad(&mode, aad + off, sizeof(aad) - off) != sizeof(aad) - off)
			return(host_fail("gcm aad"));
		if(mp_crypto_cipher_mode_update(&mode, msg, 16, pt) != 16)
			return(host_fail("gcm aad"));
		mp_crypto_cipher_mode_final(&mode, NULL, tag2);
		mp_crypto_cipher_mode_free(&mode);
		mp_crypto_cipher_free(&cipher);
		if(memcmp(pt, ct, 16) != 0 || memcmp(tag2, tag, 16) != 0)
			return(host_fail("gcm aad"));
	}
	if(host_sched(16, run, NULL))
		return(1);
	host_report("gcm");
	return(0);
}
