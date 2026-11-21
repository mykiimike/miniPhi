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

/* Host vectors. Not linked into the firmware. CTR is symmetric. */

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
#define SUPPORT_COMMON_CRYPTO_MODE_CTR
#include <mp.h>
#include <crypto/aes.h>
#include <crypto/cipher.h>
#include <crypto/mode.h>
#include <hostcrypto.h>
#include "vectors_ref.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../cipher/aes/aes.c"
#include "../../../cipher/index.c"
#include "../ctr.c"
#include "../../index.c"

static uint8_t key[32];
static uint8_t iv[16];
static uint8_t msg[1100];

static int go(const uint8_t *k, unsigned int klen, const uint8_t *v, unsigned int vlen, const uint8_t *in, unsigned int len, const unsigned int *cuts, unsigned int ncuts, uint8_t *out) {
	mp_crypto_cipher_t cipher;
	mp_crypto_cipher_mode_t mode;
	unsigned int pos = 0;
	unsigned int i;
	unsigned int n;
	unsigned int one;

	if(mp_crypto_cipher_init(&cipher, "AES", k, klen) != TRUE)
		return(-1);
	if(mp_crypto_cipher_mode_init(&mode, "CTR", &cipher, v, vlen, TRUE) != TRUE) {
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
	mp_crypto_cipher_mode_final(&mode, NULL, NULL);
	mp_crypto_cipher_mode_free(&mode);
	mp_crypto_cipher_free(&cipher);
	return(0);
}

static int run(unsigned int len, const unsigned int *cuts, unsigned int ncuts, void *ud) {
	uint8_t a[1100];
	uint8_t b[1100];

	(void)ud;
	if(go(key, MP_CRYPTO_AES_KEY, iv, 16, msg, len, NULL, 0, a) || go(key, MP_CRYPTO_AES_KEY, iv, 16, msg, len, cuts, ncuts, b))
		return(host_fail("ctr"));
	if(len && memcmp(a, b, len) != 0)
		return(host_fail("ctr"));
	return(0);
}

int main(void) {
	unsigned int i;
	uint8_t ct[80];
	uint8_t pt[80];
	uint8_t k24[24];

	for(i = 0; i < CTR_COUNT; i++) {
		unsigned int n = ctr_vec[i].n[2];

		if(ctr_vec[i].n[0] != MP_CRYPTO_AES_KEY)
			continue;
		if(go(ctr_at(i, 0), ctr_vec[i].n[0], ctr_at(i, 1), 16, ctr_at(i, 2), n, NULL, 0, ct) || memcmp(ct, ctr_at(i, 3), n) != 0) {
			fprintf(stderr, "ctr %u\n", ctr_vec[i].id);
			return(1);
		}
		if(go(ctr_at(i, 0), ctr_vec[i].n[0], ctr_at(i, 1), 16, ct, n, NULL, 0, pt) || memcmp(pt, ctr_at(i, 2), n) != 0)
			return(host_fail("ctr dec"));
		host_valid_n++;
	}
	host_fill(key, sizeof(key));
	host_fill(k24, sizeof(k24));
	host_fill(iv, sizeof(iv));
	host_fill(msg, 32);
	if(MP_CRYPTO_AES_KEY == 24) {
		if(go(k24, 24, iv, 16, msg, 32, NULL, 0, ct) || go(k24, 24, iv, 16, ct, 32, NULL, 0, pt) || memcmp(pt, msg, 32) != 0)
			return(host_fail("ctr-192"));
		host_valid_n++;
	}
	if(go(key, MP_CRYPTO_AES_KEY, iv, 15, msg, 16, NULL, 0, ct) == 0)
		return(host_fail("ctr iv"));
	host_invalid_n++;
	host_fill(msg, sizeof(msg));
	if(host_sched(16, run, NULL))
		return(1);
	host_report("ctr");
	return(0);
}
