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
 * S >= L is not canonical and must be rejected. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH_SHA512
#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_BN
#define SUPPORT_COMMON_CRYPTO_SIGN_ED25519
#include <mp.h>
#include <crypto/sha512.h>
#include <crypto/lock.h>
#include <crypto/bn.h>
#include <crypto/ed25519.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../hash/sha512/sha512.c"
#include "../../../lock/lock.c"
#define _wide _bn_wide
#include "../../../bn/bn.c"
#undef _wide
#include "../ed25519.c"

/* little-endian order L */
static const uint8_t L[32] = {
	0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
};

int main(void) {
	unsigned int i;
	uint8_t seed[32];
	uint8_t priv[32];
	uint8_t pub[32];
	uint8_t sig[64];
	uint8_t bad[64];
	int flipped = 0;

	for(i = 0; i < ED_COUNT; i++) {
		int accepted = 0;

		if(ed_vec[i].n[0] == 32)
			accepted = mp_crypto_ed25519_verify(ed_at(i, 0), ed_at(i, 1), ed_vec[i].n[1], ed_at(i, 2), ed_vec[i].n[2]) == YES;
		else if(ed_vec[i].result == 1)
			return(host_fail("ed length"));
		if(host_expect("ed25519", ed_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", ed_vec[i].id);
			return(1);
		}
		if(accepted && !flipped) {
			memcpy(bad, ed_at(i, 2), 64);
			bad[0] ^= 1;
			if(mp_crypto_ed25519_verify(ed_at(i, 0), ed_at(i, 1), ed_vec[i].n[1], bad, 64) == YES)
				return(host_fail("ed sig"));
			memcpy(bad, ed_at(i, 2), 64);
			memcpy(bad + 32, L, 32);
			if(mp_crypto_ed25519_verify(ed_at(i, 0), ed_at(i, 1), ed_vec[i].n[1], bad, 64) == YES)
				return(host_fail("ed S=L"));
			memset(bad + 32, 0xff, 32);
			if(mp_crypto_ed25519_verify(ed_at(i, 0), ed_at(i, 1), ed_vec[i].n[1], bad, 64) == YES)
				return(host_fail("ed S"));
			if(ed_vec[i].n[1]) {
				uint8_t m[1100];

				memcpy(m, ed_at(i, 1), ed_vec[i].n[1]);
				m[0] ^= 1;
				if(mp_crypto_ed25519_verify(ed_at(i, 0), m, ed_vec[i].n[1], ed_at(i, 2), 64) == YES)
					return(host_fail("ed msg"));
			}
			memcpy(bad, ed_at(i, 0), 32);
			bad[0] ^= 1;
			if(mp_crypto_ed25519_verify(bad, ed_at(i, 1), ed_vec[i].n[1], ed_at(i, 2), 64) == YES)
				return(host_fail("ed pk"));
			flipped = 1;
		}
	}
	if(!flipped)
		return(host_fail("ed no vector"));
	memset(seed, 0x42, sizeof(seed));
	if(mp_crypto_ed25519_keypair(seed, 32, priv, pub) != TRUE || mp_crypto_ed25519_sign(priv, "miniPhi", 7, sig) != 64)
		return(host_fail("ed sign"));
	if(mp_crypto_ed25519_verify(pub, "miniPhi", 7, sig, 64) != YES)
		return(host_fail("ed round"));
	sig[63] ^= 1;
	if(mp_crypto_ed25519_verify(pub, "miniPhi", 7, sig, 64) == YES)
		return(host_fail("ed round flip"));
	host_report("ed25519");
	return(0);
}
