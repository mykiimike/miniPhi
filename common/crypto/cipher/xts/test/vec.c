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
 * One data unit per call. A length that is not a multiple of 16 is ciphertext stealing, not a stream. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_CIPHER_AES
#define MP_CRYPTO_AES_BITS 256
#define SUPPORT_COMMON_CRYPTO_CIPHER_XTS
#include <mp.h>
#include <crypto/aes.h>
#include <crypto/xts.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../aes/aes.c"
#include "../xts.c"

static int unit(const uint8_t *key, unsigned int keylen, const uint8_t *tw, unsigned int twlen, const uint8_t *in, unsigned int len, uint8_t *out, int enc) {
	mp_crypto_xts_t ctx;
	unsigned int n;

	if(mp_crypto_xts_init(&ctx, key, keylen) != TRUE)
		return(0);
	if(mp_crypto_xts_set_tweak(&ctx, tw, twlen) != TRUE) {
		mp_crypto_xts_free(&ctx);
		return(0);
	}
	n = enc ? mp_crypto_xts_update(&ctx, in, len, out) : mp_crypto_xts_update_inv(&ctx, in, len, out);
	mp_crypto_xts_free(&ctx);
	return(n == len);
}

int main(void) {
	unsigned int i;
	unsigned int lens[6] = { 16, 17, 31, 32, 33, 48 };
	uint8_t key[64];
	uint8_t tw[16];
	uint8_t msg[64];
	uint8_t ct[64];
	uint8_t pt[64];
	uint8_t big[1100];

	for(i = 0; i < XTS_COUNT; i++) {
		unsigned int nct = xts_vec[i].n[3];
		int accepted = 0;

		if(nct && nct <= sizeof(big) && unit(xts_at(i, 0), xts_vec[i].n[0], xts_at(i, 1), xts_vec[i].n[1], xts_at(i, 2), xts_vec[i].n[2], big, 1))
			accepted = memcmp(big, xts_at(i, 3), nct) == 0;
		if(host_expect("xts", xts_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", xts_vec[i].id);
			return(1);
		}
	}
	host_fill(key, sizeof(key));
	host_fill(tw, sizeof(tw));
	host_fill(msg, sizeof(msg));
	for(i = 0; i < 6; i++) {
		if(!unit(key, 32, tw, 16, msg, lens[i], ct, 1) || !unit(key, 32, tw, 16, ct, lens[i], pt, 0) || memcmp(pt, msg, lens[i]) != 0)
			return(host_fail("xts cts"));
		if(!unit(key, 64, tw, 16, msg, lens[i], ct, 1) || !unit(key, 64, tw, 16, ct, lens[i], pt, 0) || memcmp(pt, msg, lens[i]) != 0)
			return(host_fail("xts-256 cts"));
	}
	if(unit(key, 48, tw, 16, msg, 16, ct, 1) || unit(key, 32, tw, 15, msg, 16, ct, 1) || unit(key, 32, tw, 16, msg, 15, ct, 1))
		return(host_fail("xts refuse"));
	host_report("xts");
	return(0);
}
