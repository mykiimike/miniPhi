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
 * A truncated Wycheproof tag is the prefix of the full digest. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH
#define SUPPORT_COMMON_CRYPTO_HASH_SHA1
#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#define SUPPORT_COMMON_CRYPTO_HASH_SHA512
#define SUPPORT_COMMON_CRYPTO_HASH_SHA384
#define SUPPORT_COMMON_CRYPTO_MAC_HMAC
#include <mp.h>
#include <crypto/sha1.h>
#include <crypto/sha256.h>
#include <crypto/sha512.h>
#include <crypto/sha384.h>
#include <crypto/hash.h>
#include <crypto/hmac.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../hash/sha1/sha1.c"
#include "../../../hash/sha256/sha256.c"
#undef ROR
#include "../../../hash/sha512/sha512.c"
#undef ROR
#include "../../../hash/sha384/sha384.c"
#include "../../../hash/index.c"
#include "../hmac.c"

static const char *hname(unsigned int id) {
	if(id == 1)
		return("SHA1");
	if(id == 2)
		return("SHA256");
	if(id == 3)
		return("SHA384");
	if(id == 4)
		return("SHA512");
	return(NULL);
}

static unsigned int hsize(unsigned int id) {
	if(id == 1)
		return(20);
	if(id == 2)
		return(32);
	if(id == 3)
		return(48);
	if(id == 4)
		return(64);
	return(0);
}

static unsigned int hblock(unsigned int id) {
	if(id == 3 || id == 4)
		return(128);
	return(64);
}

static const char *chunk_hash;

static void init(void *c) {
	mp_crypto_hmac_init(c, chunk_hash, "key", 3);
}

static void update(void *c, const void *p, unsigned int n) {
	mp_crypto_hmac_update(c, p, n);
}

static void final(void *c, uint8_t *d) {
	mp_crypto_hmac_final(c, d);
}

static void shot(const void *p, unsigned int n, uint8_t *d) {
	mp_crypto_hmac(chunk_hash, "key", 3, p, n, d);
}

int main(void) {
	unsigned int i;
	uint8_t tag[64];
	uint8_t flipped[64];
	mp_crypto_hmac_t ctx;
	int saw = 0;

	for(i = 0; i < HMAC_COUNT; i++) {
		unsigned int id = hmac_at(i, 0)[0];
		unsigned int dlen = hsize(id);
		unsigned int taglen = hmac_vec[i].n[3];
		const char *name = hname(id);
		unsigned int n = 0;
		int accepted = 0;

		if(name && mp_crypto_hmac_init(&ctx, name, hmac_at(i, 1), hmac_vec[i].n[1]) == TRUE) {
			mp_crypto_hmac_update(&ctx, hmac_at(i, 2), hmac_vec[i].n[2]);
			n = mp_crypto_hmac_final(&ctx, tag);
			accepted = n == dlen && taglen && taglen <= n && memcmp(tag, hmac_at(i, 3), taglen) == 0;
		}
		if(accepted && !saw && taglen) {
			memcpy(flipped, hmac_at(i, 3), taglen);
			flipped[0] ^= 1;
			if(memcmp(tag, flipped, taglen) == 0)
				return(host_fail("hmac flip"));
			saw = 1;
		}
		if(host_expect("hmac", hmac_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", hmac_vec[i].id);
			return(1);
		}
	}
	if(!saw)
		return(host_fail("hmac no valid"));
	for(i = 1; i <= 4; i++) {
		chunk_hash = hname(i);
		if(host_chunk(chunk_hash, hblock(i), hsize(i), &ctx, init, update, final, shot))
			return(1);
	}
	host_report("hmac");
	return(0);
}
