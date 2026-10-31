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

#if defined(MP_CRYPTO_SELFTEST_X25519) && defined(SUPPORT_COMMON_CRYPTO_KEM_X25519)

#include <crypto/selftest.h>
#include <string.h>

/* RFC 7748 section 6.1, then DHKEM(X25519, HKDF-SHA256).
 * The recipient seed is 32 bytes 0x00..0x1f. The ephemeral coins are
 * 32 bytes 0x20..0x3f. */

static const uint8_t _scalar[32] = {
	0xa5, 0x46, 0xe3, 0x6b, 0xf0, 0x52, 0x7c, 0x9d,
	0x3b, 0x16, 0x15, 0x4b, 0x82, 0x46, 0x5e, 0xdd,
	0x62, 0x14, 0x4c, 0x0a, 0xc1, 0xfc, 0x5a, 0x18,
	0x50, 0x6a, 0x22, 0x44, 0xba, 0x44, 0x9a, 0xc4,
};
static const uint8_t _point[32] = {
	0xe6, 0xdb, 0x68, 0x67, 0x58, 0x30, 0x30, 0xdb,
	0x35, 0x94, 0xc1, 0xa4, 0x24, 0xb1, 0x5f, 0x7c,
	0x72, 0x66, 0x24, 0xec, 0x26, 0xb3, 0x35, 0x3b,
	0x10, 0xa9, 0x03, 0xa6, 0xd0, 0xab, 0x1c, 0x4c,
};
static const uint8_t _product[32] = {
	0xc3, 0xda, 0x55, 0x37, 0x9d, 0xe9, 0xc6, 0x90,
	0x8e, 0x94, 0xea, 0x4d, 0xf2, 0x8d, 0x08, 0x4f,
	0x32, 0xec, 0xcf, 0x03, 0x49, 0x1c, 0x71, 0xf7,
	0x54, 0xb4, 0x07, 0x55, 0x77, 0xa2, 0x85, 0x52,
};
static const uint8_t _pk[32] = {
	0x8f, 0x40, 0xc5, 0xad, 0xb6, 0x8f, 0x25, 0x62,
	0x4a, 0xe5, 0xb2, 0x14, 0xea, 0x76, 0x7a, 0x6e,
	0xc9, 0x4d, 0x82, 0x9d, 0x3d, 0x7b, 0x5e, 0x1a,
	0xd1, 0xba, 0x6f, 0x3e, 0x21, 0x38, 0x28, 0x5f,
};
static const uint8_t _enc[32] = {
	0x35, 0x80, 0x72, 0xd6, 0x36, 0x58, 0x80, 0xd1,
	0xae, 0xea, 0x32, 0x9a, 0xdf, 0x91, 0x21, 0x38,
	0x38, 0x51, 0xed, 0x21, 0xa2, 0x8e, 0x3b, 0x75,
	0xe9, 0x65, 0xd0, 0xd2, 0xcd, 0x16, 0x62, 0x54,
};
static const uint8_t _ss[32] = {
	0x20, 0xa5, 0x85, 0x93, 0xfe, 0x00, 0xbc, 0xc5,
	0x4b, 0x0c, 0x1e, 0x45, 0x2c, 0x52, 0x74, 0x7d,
	0x1f, 0x7a, 0x2e, 0x30, 0x1b, 0x9d, 0x6c, 0x2b,
	0xb7, 0x17, 0x58, 0xde, 0xd5, 0xe6, 0x14, 0xf0,
};

mp_bool_t mp_crypto_selftest_x25519(void) {
	uint8_t out[32], seed[32], coins[32];
	uint8_t priv[32], pub[32], ct[32], ss[32], back[32];
	unsigned int i;
	mp_bool_t ok = NO;

	mp_crypto_x25519(out, _scalar, _point);
	if(mp_crypto_selftest_equal(out, _product, 32) != YES)
		goto done;
	for(i = 0; i < 32; i++)
		seed[i] = (uint8_t)i;
	if(mp_crypto_x25519_keypair(seed, 32, priv, pub) != TRUE)
		goto done;
	if(mp_crypto_selftest_equal(pub, _pk, 32) != YES)
		goto done;
	for(i = 0; i < 32; i++)
		coins[i] = (uint8_t)(32 + i);
	if(mp_crypto_x25519_encaps(pub, coins, 32, ss, ct) != 32)
		goto done;
	if(mp_crypto_selftest_equal(ct, _enc, 32) != YES)
		goto done;
	if(mp_crypto_selftest_equal(ss, _ss, 32) != YES)
		goto done;
	if(mp_crypto_x25519_decaps(priv, ct, 32, back) != YES)
		goto done;
	if(mp_crypto_selftest_equal(back, ss, 32) != YES)
		goto done;
	ct[0] ^= 1;
	if(mp_crypto_x25519_decaps(priv, ct, 32, back) != YES)
		goto done;
	if(mp_crypto_selftest_equal(back, ss, 32) == YES)
		goto done;
	ok = YES;
done:
	memset(out, 0, sizeof(out));
	memset(seed, 0, sizeof(seed));
	memset(coins, 0, sizeof(coins));
	memset(priv, 0, sizeof(priv));
	memset(pub, 0, sizeof(pub));
	memset(ct, 0, sizeof(ct));
	memset(ss, 0, sizeof(ss));
	memset(back, 0, sizeof(back));
	return(ok);
}

#endif
