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

/* Host vectors. Not linked into the firmware. XChaCha20 is not this cipher. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20
#include <mp.h>
#include <crypto/chacha20.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"
#include "vectors_rfc.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../chacha20.c"

static uint8_t out[1100];

static int crypt(const uint8_t *key, unsigned int keylen, const uint8_t *iv, unsigned int ivlen, uint32_t counter, const uint8_t *in, const unsigned int *cuts, unsigned int ncuts) {
	mp_crypto_chacha20_t ctx;
	unsigned int pos = 0;
	unsigned int i;
	unsigned int n;

	if(mp_crypto_chacha20_init(&ctx, key, keylen) != TRUE)
		return(-1);
	mp_crypto_chacha20_set_nonce(&ctx, iv, ivlen);
	mp_crypto_chacha20_set_counter(&ctx, counter);
	for(i = 0; i < ncuts; i++) {
		n = mp_crypto_chacha20_update_block(&ctx, in + pos, cuts[i], out + pos);
		if(n != cuts[i]) {
			mp_crypto_chacha20_free(&ctx);
			return(-1);
		}
		pos += cuts[i];
	}
	mp_crypto_chacha20_free(&ctx);
	return(0);
}

static int chunk(unsigned int len, const unsigned int *cuts, unsigned int ncuts, void *ud) {
	uint8_t *msg = ud;
	uint8_t one[1100];
	uint8_t spl[1100];
	unsigned int once = len;

	if(crypt(msg, 32, msg + 32, 12, 1, msg + 48, &once, 1) != 0)
		return(host_fail("chacha"));
	memcpy(one, out, len);
	if(crypt(msg, 32, msg + 32, 12, 1, msg + 48, cuts, ncuts) != 0)
		return(host_fail("chacha"));
	memcpy(spl, out, len);
	if(len && memcmp(one, spl, len) != 0)
		return(host_fail("chacha"));
	return(0);
}

int main(void) {
	unsigned int i;
	uint8_t key[32];
	uint8_t msg[1100];
	uint8_t ct[300];
	unsigned int n;
	mp_crypto_chacha20_t ctx;

	for(i = 0; i < CC_COUNT; i++) {
		unsigned int ptlen = cc_vec[i].n[3];
		int accepted;

		memset(out, 0xa5, sizeof(out));
		if(mp_crypto_chacha20_init(&ctx, cc_at(i, 0), cc_vec[i].n[0]) != TRUE)
			return(host_fail("chacha init"));
		mp_crypto_chacha20_set_nonce(&ctx, cc_at(i, 1), cc_vec[i].n[1]);
		mp_crypto_chacha20_set_counter(&ctx, host_le32(cc_at(i, 2)));
		n = mp_crypto_chacha20_update_block(&ctx, cc_at(i, 3), ptlen, out);
		mp_crypto_chacha20_free(&ctx);
		accepted = n == ptlen && (ptlen == 0 || memcmp(out, cc_at(i, 4), ptlen) == 0);
		if(cc_vec[i].result == 2 && (n != 0 || (ptlen && out[0] != 0xa5))) {
			fprintf(stderr, "chacha wrote on wrap %u\n", cc_vec[i].id);
			return(1);
		}
		if(host_expect("chacha", cc_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", cc_vec[i].id);
			return(1);
		}
	}
	for(i = 0; i < CCRFC_COUNT; i++) {
		unsigned int ptlen = ccrfc_vec[i].n[2];

		if(mp_crypto_chacha20_init(&ctx, ccrfc_at(i, 0), 32) != TRUE)
			return(host_fail("rfc"));
		mp_crypto_chacha20_set_nonce(&ctx, ccrfc_at(i, 1), 12);
		mp_crypto_chacha20_set_counter(&ctx, 1);
		n = mp_crypto_chacha20_update_block(&ctx, ccrfc_at(i, 2), ptlen, ct);
		mp_crypto_chacha20_free(&ctx);
		if(n != ptlen || memcmp(ct, ccrfc_at(i, 3), ptlen) != 0)
			return(host_fail("rfc 8439"));
		host_valid_n++;
	}
	if(mp_crypto_chacha20_init(&ctx, key, 16) != FALSE)
		return(host_fail("chacha key"));
	host_invalid_n++;
	host_fill(msg, sizeof(msg));
	if(host_sched(64, chunk, msg))
		return(1);
	host_report("chacha20");
	return(0);
}
