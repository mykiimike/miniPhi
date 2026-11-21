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
 * NIST SP 800-90A section 10.1.2, no additional input. */

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
#define SUPPORT_COMMON_CRYPTO_RNG_HMAC_DRBG
#include <mp.h>
#include <crypto/sha1.h>
#include <crypto/sha256.h>
#include <crypto/sha512.h>
#include <crypto/sha384.h>
#include <crypto/hash.h>
#include <crypto/hmac.h>
#include <crypto/hmac_drbg.h>
#include <hostcrypto.h>
#include "vectors_ref.h"

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
#include "../hmac_drbg.c"

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

static int draw(const char *name, unsigned int n, uint8_t *out) {
	mp_crypto_hmac_drbg_t ctx;
	uint8_t entropy[32];
	uint8_t nonce[16];
	uint8_t personal[8];

	memset(entropy, 0x01, sizeof(entropy));
	memset(nonce, 0x02, sizeof(nonce));
	memset(personal, 0x03, sizeof(personal));
	if(mp_crypto_hmac_drbg_init(&ctx, name, entropy, sizeof(entropy), nonce, sizeof(nonce), personal, sizeof(personal)) != TRUE)
		return(0);
	if(mp_crypto_hmac_drbg_generate(&ctx, out, n, NULL, 0) != TRUE) {
		mp_crypto_hmac_drbg_free(&ctx);
		return(0);
	}
	mp_crypto_hmac_drbg_free(&ctx);
	return(1);
}

int main(void) {
	unsigned int i;
	uint8_t out[64];
	uint8_t one[80];
	uint8_t split[80];
	uint8_t keep[8];
	mp_crypto_hmac_drbg_t ctx;
	unsigned int lens[5];
	unsigned int n;

	for(i = 0; i < DRBG_COUNT; i++) {
		const char *name = hname(drbg_at(i, 0)[0]);

		if(!name || !draw(name, 64, out) || memcmp(out, drbg_at(i, 4), 64) != 0) {
			fprintf(stderr, "drbg %u\n", drbg_vec[i].id);
			return(1);
		}
		host_valid_n++;
	}
	memset(keep, 0xa5, sizeof(keep));
	if(mp_crypto_hmac_drbg_init(&ctx, "SHA256", NULL, 0, NULL, 0, NULL, 0) != FALSE)
		return(host_fail("drbg hsm"));
	if(!draw("SHA256", 1, out))
		return(host_fail("drbg seed"));
	memcpy(keep, out, 1);
	if(mp_crypto_hmac_drbg_init(&ctx, "SHA256", "entropy-entropy-entropy-entropy", 32, "nonce-nonce-nonc", 16, NULL, 0) != TRUE)
		return(host_fail("drbg init"));
	if(mp_crypto_hmac_drbg_generate(&ctx, keep, 0, NULL, 0) != FALSE || keep[0] != out[0])
		return(host_fail("drbg zero"));
	if(mp_crypto_hmac_drbg_generate(&ctx, keep, MP_CRYPTO_HMAC_DRBG_MAX + 1, NULL, 0) != FALSE)
		return(host_fail("drbg max"));
	mp_crypto_hmac_drbg_free(&ctx);
	host_invalid_n += 3;
	/* one Generate is a stream of HMAC blocks; a shorter first request is that prefix.
	 * a second Generate is a new NIST call and is not a continuation of the bytes. */
	n = hsize(2);
	lens[0] = 1;
	lens[1] = n - 1;
	lens[2] = n;
	lens[3] = n + 1;
	lens[4] = n * 2 + 1;
	if(!draw("SHA256", 80, one))
		return(host_fail("drbg one"));
	for(i = 0; i < 5; i++) {
		if(!draw("SHA256", lens[i], split) || memcmp(one, split, lens[i]) != 0)
			return(host_fail("drbg size"));
	}
	host_report("drbg");
	return(0);
}
