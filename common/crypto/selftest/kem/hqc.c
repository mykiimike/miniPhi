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

#include <mp.h>

#if defined(MP_CRYPTO_SELFTEST_HQC) && defined(SUPPORT_COMMON_CRYPTO_KEM_HQC)

#include <crypto/selftest.h>
#include <string.h>

/* HQC-128. Seed is 96 bytes 0x00..0x5f (secret seed, sigma, public seed).
 * Coins are 32 bytes 0x60..0x7f (message, then salt). The digests are
 * SHA3-256 of the public key and of the ciphertext. */

static const uint8_t _pk[32] = {
	0xd7, 0x36, 0x44, 0x53, 0x5d, 0x91, 0x5e, 0x3e,
	0xbd, 0x8b, 0x1a, 0xdd, 0x37, 0x05, 0xda, 0xe6,
	0xa7, 0xb2, 0x06, 0xd6, 0xd4, 0x02, 0x78, 0xad,
	0x5b, 0xa8, 0x6a, 0x1d, 0xd7, 0x5c, 0x8d, 0xdc,
};
static const uint8_t _ct[32] = {
	0x86, 0x2b, 0x42, 0xa4, 0xc5, 0xfd, 0x47, 0x61,
	0x6a, 0xd8, 0x04, 0x82, 0x62, 0xc7, 0x43, 0x89,
	0xd3, 0x9e, 0xb6, 0x2b, 0xa7, 0x97, 0x51, 0xba,
	0x27, 0x38, 0x0f, 0x0b, 0x21, 0xed, 0x3e, 0xd4,
};
static const uint8_t _ss[64] = {
	0x73, 0xe8, 0xf2, 0x4d, 0x88, 0xed, 0xb6, 0xde,
	0x98, 0x17, 0x7a, 0x13, 0x2f, 0x86, 0xdc, 0x0d,
	0x43, 0xf3, 0x76, 0x54, 0x09, 0xfa, 0x47, 0x8b,
	0x44, 0x82, 0x92, 0x37, 0xc9, 0xf3, 0xd9, 0x71,
	0x7c, 0xd0, 0x20, 0xf7, 0x78, 0x24, 0xbc, 0xe6,
	0x98, 0x53, 0xb8, 0xd6, 0x8e, 0xc0, 0x3a, 0xfc,
	0xbd, 0x4f, 0xf3, 0x0f, 0x98, 0xb7, 0x68, 0xc5,
	0xee, 0x0f, 0x06, 0xa4, 0xfd, 0x58, 0xdf, 0xe3,
};

static mp_bool_t _sha(const void *data, unsigned int len, const uint8_t expect[32]) {
	uint8_t dig[32];
	mp_bool_t ok;

	mp_crypto_sha3_256(data, len, dig);
	ok = mp_crypto_selftest_equal(dig, expect, 32);
	memset(dig, 0, sizeof(dig));
	return(ok);
}

mp_bool_t mp_crypto_selftest_hqc(void) {
	static uint8_t priv[MP_CRYPTO_HQC_PRIV];
	static uint8_t pub[MP_CRYPTO_HQC_PUB];
	static uint8_t ct[MP_CRYPTO_HQC_CT];
	uint8_t seed[96], coins[32], ss[64], back[64];
	unsigned int i;
	mp_bool_t ok = NO;

	for(i = 0; i < 96; i++)
		seed[i] = (uint8_t)i;
	for(i = 0; i < 32; i++)
		coins[i] = (uint8_t)(96 + i);
	if(mp_crypto_hqc_keypair(seed, 96, priv, pub) != TRUE)
		goto done;
	if(_sha(pub, MP_CRYPTO_HQC_PUB, _pk) != YES)
		goto done;
	if(mp_crypto_hqc_encaps(pub, coins, 32, ss, ct) != MP_CRYPTO_HQC_CT)
		goto done;
	if(_sha(ct, MP_CRYPTO_HQC_CT, _ct) != YES)
		goto done;
	if(mp_crypto_selftest_equal(ss, _ss, 64) != YES)
		goto done;
	if(mp_crypto_hqc_decaps(priv, ct, MP_CRYPTO_HQC_CT, back) != YES)
		goto done;
	if(mp_crypto_selftest_equal(back, ss, 64) != YES)
		goto done;
	ct[40] ^= 1;
	if(mp_crypto_hqc_decaps(priv, ct, MP_CRYPTO_HQC_CT, back) != YES)
		goto done;
	if(mp_crypto_selftest_equal(back, ss, 64) == YES)
		goto done;
	ok = YES;
done:
	memset(priv, 0, sizeof(priv));
	memset(pub, 0, sizeof(pub));
	memset(ct, 0, sizeof(ct));
	memset(seed, 0, sizeof(seed));
	memset(coins, 0, sizeof(coins));
	memset(ss, 0, sizeof(ss));
	memset(back, 0, sizeof(back));
	return(ok);
}

#endif
