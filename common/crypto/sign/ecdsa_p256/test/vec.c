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
 * P1363 r||s, public key x||y. The message is hashed with SHA-256 inside verify. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH
#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#define SUPPORT_COMMON_CRYPTO_MAC_HMAC
#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_BN
#define SUPPORT_COMMON_CRYPTO_SIGN_ECDSA_P256
#include <mp.h>
#include <crypto/sha256.h>
#include <crypto/hash.h>
#include <crypto/hmac.h>
#include <crypto/lock.h>
#include <crypto/bn.h>
#include <crypto/ecdsa_p256.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../hash/sha256/sha256.c"
#include "../../../hash/index.c"
#include "../../../mac/hmac/hmac.c"
#include "../../../lock/lock.c"
#define _wide _bn_wide
#include "../../../bn/bn.c"
#undef _wide
#include "../ecdsa_p256.c"

/* n and p, big-endian */
static const uint8_t N[32] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51,
};
static const uint8_t P[32] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static void be32(uint8_t out[32], unsigned int v) {
	memset(out, 0, 32);
	out[31] = (uint8_t)v;
}

int main(void) {
	unsigned int i;
	uint8_t seed[32];
	uint8_t priv[32];
	uint8_t pub[64];
	uint8_t sig[64];
	uint8_t bad[64];
	uint8_t one[32];
	int flipped = 0;

	be32(one, 1);
	for(i = 0; i < EC_COUNT; i++) {
		int accepted = 0;

		if(ec_vec[i].n[0] == 64)
			accepted = mp_crypto_ecdsa_p256_verify(ec_at(i, 0), ec_at(i, 1), ec_vec[i].n[1], ec_at(i, 2), ec_vec[i].n[2]) == YES;
		else if(ec_vec[i].result == 1)
			return(host_fail("ecdsa pk"));
		if(host_expect("ecdsa", ec_vec[i].result, accepted)) {
			fprintf(stderr, "id %u sig %u\n", ec_vec[i].id, ec_vec[i].n[2]);
			return(1);
		}
		if(accepted && !flipped && ec_vec[i].n[2] == 64) {
			memcpy(bad, ec_at(i, 2), 64);
			bad[0] ^= 1;
			if(mp_crypto_ecdsa_p256_verify(ec_at(i, 0), ec_at(i, 1), ec_vec[i].n[1], bad, 64) == YES)
				return(host_fail("ecdsa sig"));
			if(ec_vec[i].n[1]) {
				uint8_t m[1100];

				memcpy(m, ec_at(i, 1), ec_vec[i].n[1]);
				m[0] ^= 1;
				if(mp_crypto_ecdsa_p256_verify(ec_at(i, 0), m, ec_vec[i].n[1], ec_at(i, 2), 64) == YES)
					return(host_fail("ecdsa msg"));
			}
			memcpy(bad, ec_at(i, 0), 64);
			bad[0] ^= 1;
			if(mp_crypto_ecdsa_p256_verify(bad, ec_at(i, 1), ec_vec[i].n[1], ec_at(i, 2), 64) == YES)
				return(host_fail("ecdsa pk flip"));
			flipped = 1;
		}
	}
	if(!flipped)
		return(host_fail("ecdsa no vector"));
	memset(sig, 0, 64);
	if(mp_crypto_ecdsa_p256_verify(ec_at(0, 0), "m", 1, sig, 64) == YES)
		return(host_fail("ecdsa r=0"));
	memcpy(sig, one, 32);
	if(mp_crypto_ecdsa_p256_verify(ec_at(0, 0), "m", 1, sig, 64) == YES)
		return(host_fail("ecdsa s=0"));
	memcpy(sig, N, 32);
	memcpy(sig + 32, one, 32);
	if(mp_crypto_ecdsa_p256_verify(ec_at(0, 0), "m", 1, sig, 64) == YES)
		return(host_fail("ecdsa r=n"));
	memcpy(sig, one, 32);
	memcpy(sig + 32, N, 32);
	if(mp_crypto_ecdsa_p256_verify(ec_at(0, 0), "m", 1, sig, 64) == YES)
		return(host_fail("ecdsa s=n"));
	memcpy(pub, one, 32);
	memcpy(pub + 32, one, 32);
	memcpy(sig, one, 32);
	memcpy(sig + 32, one, 32);
	if(mp_crypto_ecdsa_p256_verify(pub, "m", 1, sig, 64) == YES)
		return(host_fail("ecdsa curve"));
	memcpy(pub, P, 32);
	memcpy(pub + 32, one, 32);
	if(mp_crypto_ecdsa_p256_verify(pub, "m", 1, sig, 64) == YES)
		return(host_fail("ecdsa x=p"));
	memset(seed, 0x11, sizeof(seed));
	if(mp_crypto_ecdsa_p256_keypair(seed, 32, priv, pub) != TRUE || mp_crypto_ecdsa_p256_sign(priv, "miniPhi", 7, sig) != 64)
		return(host_fail("ecdsa sign"));
	if(mp_crypto_ecdsa_p256_verify(pub, "miniPhi", 7, sig, 64) != YES)
		return(host_fail("ecdsa round"));
	sig[0] ^= 1;
	if(mp_crypto_ecdsa_p256_verify(pub, "miniPhi", 7, sig, 64) == YES)
		return(host_fail("ecdsa round flip"));
	host_report("ecdsa");
	return(0);
}
