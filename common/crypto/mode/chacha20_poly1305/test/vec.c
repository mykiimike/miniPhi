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
 * One-shot AEAD. result 4 is a nonce the API refuses.
 * The tag is checked before plaintext is released; a mismatch wipes it. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20
#define SUPPORT_COMMON_CRYPTO_MAC_POLY1305
#define SUPPORT_COMMON_CRYPTO_MAC
#define SUPPORT_COMMON_CRYPTO_MODE_CHACHA20_POLY1305
#include <mp.h>
#include <crypto/chacha20.h>
#include <crypto/poly1305.h>
#include <crypto/mac.h>
#include <crypto/chacha20_poly1305.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"
#include "vectors_rfc.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define _le32 _cc_le32
#define _le32_put _cc_le32_put
#include "../../../cipher/chacha20/chacha20.c"
#undef _le32
#undef _le32_put
#include "../../../mac/poly1305/poly1305.c"
#include "../../../mac/index.c"
#include "../chacha20_poly1305.c"

static int zeroed(const uint8_t *p, unsigned int n) {
	unsigned int i;

	for(i = 0; i < n; i++)
		if(p[i])
			return(0);
	return(1);
}

static int untouched(const uint8_t *p, unsigned int n) {
	unsigned int i;

	for(i = 0; i < n; i++)
		if(p[i] != 0xa5)
			return(0);
	return(1);
}

int main(void) {
	unsigned int i;
	unsigned int lens[7] = { 0, 1, 63, 64, 65, 129, 1009 };
	uint8_t ct[1100];
	uint8_t pt[1100];
	uint8_t tag[16];
	uint8_t key[32];
	uint8_t nonce[12];
	uint8_t msg[1100];
	int flipped = 0;

	for(i = 0; i < CP_COUNT; i++) {
		unsigned int n = cp_vec[i].n[3];
		unsigned int aadlen = cp_vec[i].n[2];
		int accepted = 0;
		const uint8_t *aad = aadlen ? cp_at(i, 2) : NULL;

		memset(pt, 0xa5, sizeof(pt));
		if(cp_vec[i].result == 1) {
			if(mp_crypto_chacha20_poly1305_encrypt(cp_at(i, 0), cp_vec[i].n[0], cp_at(i, 1), cp_vec[i].n[1], aad, aadlen, n ? cp_at(i, 3) : NULL, n, ct, tag) == TRUE && (n == 0 || memcmp(ct, cp_at(i, 4), n) == 0) && cp_vec[i].n[5] == 16 && memcmp(tag, cp_at(i, 5), 16) == 0) {
				memset(pt, 0xa5, n ? n : 1);
				if(mp_crypto_chacha20_poly1305_decrypt(cp_at(i, 0), cp_vec[i].n[0], cp_at(i, 1), cp_vec[i].n[1], aad, aadlen, n ? ct : NULL, n, tag, pt) == TRUE && (n == 0 || memcmp(pt, cp_at(i, 3), n) == 0))
					accepted = 1;
			}
		}
		else if(mp_crypto_chacha20_poly1305_decrypt(cp_at(i, 0), cp_vec[i].n[0], cp_at(i, 1), cp_vec[i].n[1], aad, aadlen, n ? cp_at(i, 4) : NULL, n, cp_at(i, 5), pt) == TRUE)
			accepted = 1;
		else if(cp_vec[i].result == 2 && n && !zeroed(pt, n)) {
			fprintf(stderr, "chacha-poly kept plaintext %u\n", cp_vec[i].id);
			return(1);
		}
		else if(cp_vec[i].result == 4 && n && !untouched(pt, n) && !zeroed(pt, n)) {
			fprintf(stderr, "chacha-poly wrote %u\n", cp_vec[i].id);
			return(1);
		}
		if(host_expect("chacha-poly", cp_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", cp_vec[i].id);
			return(1);
		}
		if(accepted && !flipped && n) {
			uint8_t bad[16];
			uint8_t badn[12];

			memcpy(bad, tag, 16);
			bad[0] ^= 1;
			memset(pt, 0xa5, n);
			if(mp_crypto_chacha20_poly1305_decrypt(cp_at(i, 0), 32, cp_at(i, 1), 12, aad, aadlen, ct, n, bad, pt) != FALSE || !zeroed(pt, n))
				return(host_fail("cp tag"));
			ct[0] ^= 1;
			memset(pt, 0xa5, n);
			if(mp_crypto_chacha20_poly1305_decrypt(cp_at(i, 0), 32, cp_at(i, 1), 12, aad, aadlen, ct, n, tag, pt) != FALSE || !zeroed(pt, n))
				return(host_fail("cp ct"));
			ct[0] ^= 1;
			memcpy(badn, cp_at(i, 1), 12);
			badn[0] ^= 1;
			memset(pt, 0xa5, n);
			if(mp_crypto_chacha20_poly1305_decrypt(cp_at(i, 0), 32, badn, 12, aad, aadlen, ct, n, tag, pt) != FALSE || !zeroed(pt, n))
				return(host_fail("cp nonce"));
			if(aadlen) {
				uint8_t bada[600];

				memcpy(bada, aad, aadlen);
				bada[0] ^= 1;
				memset(pt, 0xa5, n);
				if(mp_crypto_chacha20_poly1305_decrypt(cp_at(i, 0), 32, cp_at(i, 1), 12, bada, aadlen, ct, n, tag, pt) != FALSE || !zeroed(pt, n))
					return(host_fail("cp aad"));
			}
			flipped = 1;
		}
	}
	for(i = 0; i < CPRFC_COUNT; i++) {
		unsigned int n = cprfc_vec[i].n[3];

		if(mp_crypto_chacha20_poly1305_encrypt(cprfc_at(i, 0), 32, cprfc_at(i, 1), 12, cprfc_at(i, 2), cprfc_vec[i].n[2], cprfc_at(i, 3), n, ct, tag) != TRUE || memcmp(ct, cprfc_at(i, 4), n) != 0 || memcmp(tag, cprfc_at(i, 5), 16) != 0)
			return(host_fail("rfc 8439"));
		host_valid_n++;
	}
	if(!flipped)
		return(host_fail("cp no vector"));
	host_fill(key, sizeof(key));
	host_fill(nonce, sizeof(nonce));
	host_fill(msg, sizeof(msg));
	for(i = 0; i < 7; i++) {
		unsigned int n = lens[i];

		if(mp_crypto_chacha20_poly1305_encrypt(key, 32, nonce, 12, "aad", 3, msg, n, ct, tag) != TRUE)
			return(host_fail("cp len"));
		memset(pt, 0xa5, n ? n : 1);
		if(mp_crypto_chacha20_poly1305_decrypt(key, 32, nonce, 12, "aad", 3, n ? ct : NULL, n, tag, pt) != TRUE || (n && memcmp(pt, msg, n) != 0))
			return(host_fail("cp len"));
	}
	host_report("chacha20-poly1305");
	return(0);
}
