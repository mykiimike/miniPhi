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
 * HKDF is one-shot. A shorter L must be the prefix of a longer expansion. */

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
#define SUPPORT_COMMON_CRYPTO_KDF_HKDF
#include <mp.h>
#include <crypto/sha1.h>
#include <crypto/sha256.h>
#include <crypto/sha512.h>
#include <crypto/sha384.h>
#include <crypto/hash.h>
#include <crypto/hmac.h>
#include <crypto/hkdf.h>
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
#include "../../../mac/hmac/hmac.c"
#include "../hkdf.c"

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

static uint8_t out[16400];

int main(void) {
	unsigned int i;
	uint8_t full[100];
	uint8_t part[64];
	unsigned int lens[6] = { 1, 19, 20, 21, 41, 64 };

	for(i = 0; i < HKDF_COUNT; i++) {
		unsigned int id = hkdf_at(i, 0)[0];
		const char *name = hname(id);
		unsigned int L = ((unsigned int)hkdf_at(i, 5)[0] << 8) | hkdf_at(i, 5)[1];
		unsigned int n = 0;
		int accepted = 0;
		const uint8_t *salt = hkdf_vec[i].n[2] ? hkdf_at(i, 2) : NULL;
		const uint8_t *info = hkdf_vec[i].n[3] ? hkdf_at(i, 3) : NULL;

		if(name && L <= sizeof(out)) {
			n = mp_crypto_hkdf(name, salt, hkdf_vec[i].n[2], hkdf_at(i, 1), hkdf_vec[i].n[1], info, hkdf_vec[i].n[3], out, L);
			accepted = L && n == L && hkdf_vec[i].n[4] == L && memcmp(out, hkdf_at(i, 4), L) == 0;
		}
		if(host_expect("hkdf", hkdf_vec[i].result, accepted)) {
			fprintf(stderr, "id %u L %u\n", hkdf_vec[i].id, L);
			return(1);
		}
	}
	if(mp_crypto_hkdf("SHA256", NULL, 0, "ikm", 3, "info", 4, full, sizeof(full)) != sizeof(full))
		return(host_fail("hkdf full"));
	for(i = 0; i < 6; i++) {
		if(mp_crypto_hkdf("SHA256", NULL, 0, "ikm", 3, "info", 4, part, lens[i]) != lens[i] || memcmp(full, part, lens[i]) != 0)
			return(host_fail("hkdf prefix"));
	}
	if(mp_crypto_hkdf("SHA256", NULL, 0, "ikm", 3, NULL, 0, out, 255u * 32 + 1) != 0)
		return(host_fail("hkdf limit"));
	host_report("hkdf");
	return(0);
}
