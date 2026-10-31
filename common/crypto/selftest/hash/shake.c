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

#if defined(MP_CRYPTO_SELFTEST_SHAKE) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHAKE)

#include <crypto/selftest.h>
#include <crypto/shake.h>
#include <string.h>

/* FIPS 202, SHA3-256/512 and SHAKE128/256 of "abc". SHAKE squeezes 32 bytes. */

static const uint8_t _sha3_256[32] = {
	0x3a, 0x98, 0x5d, 0xa7, 0x4f, 0xe2, 0x25, 0xb2,
	0x04, 0x5c, 0x17, 0x2d, 0x6b, 0xd3, 0x90, 0xbd,
	0x85, 0x5f, 0x08, 0x6e, 0x3e, 0x9d, 0x52, 0x5b,
	0x46, 0xbf, 0xe2, 0x45, 0x11, 0x43, 0x15, 0x32,
};

static const uint8_t _sha3_512[64] = {
	0xb7, 0x51, 0x85, 0x0b, 0x1a, 0x57, 0x16, 0x8a,
	0x56, 0x93, 0xcd, 0x92, 0x4b, 0x6b, 0x09, 0x6e,
	0x08, 0xf6, 0x21, 0x82, 0x74, 0x44, 0xf7, 0x0d,
	0x88, 0x4f, 0x5d, 0x02, 0x40, 0xd2, 0x71, 0x2e,
	0x10, 0xe1, 0x16, 0xe9, 0x19, 0x2a, 0xf3, 0xc9,
	0x1a, 0x7e, 0xc5, 0x76, 0x47, 0xe3, 0x93, 0x40,
	0x57, 0x34, 0x0b, 0x4c, 0xf4, 0x08, 0xd5, 0xa5,
	0x65, 0x92, 0xf8, 0x27, 0x4e, 0xec, 0x53, 0xf0,
};

static const uint8_t _shake128[32] = {
	0x58, 0x81, 0x09, 0x2d, 0xd8, 0x18, 0xbf, 0x5c,
	0xf8, 0xa3, 0xdd, 0xb7, 0x93, 0xfb, 0xcb, 0xa7,
	0x40, 0x97, 0xd5, 0xc5, 0x26, 0xa6, 0xd3, 0x5f,
	0x97, 0xb8, 0x33, 0x51, 0x94, 0x0f, 0x2c, 0xc8,
};

static const uint8_t _shake256[32] = {
	0x48, 0x33, 0x66, 0x60, 0x13, 0x60, 0xa8, 0x77,
	0x1c, 0x68, 0x63, 0x08, 0x0c, 0xc4, 0x11, 0x4d,
	0x8d, 0xb4, 0x45, 0x30, 0xf8, 0xf1, 0xe1, 0xee,
	0x4f, 0x94, 0xea, 0x37, 0xe7, 0x8b, 0x57, 0x39,
};

mp_bool_t mp_crypto_selftest_shake(void) {
	uint8_t dig[64];
	uint8_t xof[32];
	mp_bool_t ok = NO;

	mp_crypto_sha3_256("abc", 3, dig);
	if(mp_crypto_selftest_equal(dig, _sha3_256, 32) != YES)
		goto done;
	mp_crypto_sha3_512("abc", 3, dig);
	if(mp_crypto_selftest_equal(dig, _sha3_512, 64) != YES)
		goto done;
	mp_crypto_shake128(xof, 32, "abc", 3);
	if(mp_crypto_selftest_equal(xof, _shake128, 32) != YES)
		goto done;
	mp_crypto_shake256(xof, 32, "abc", 3);
	ok = mp_crypto_selftest_equal(xof, _shake256, 32);
done:
	memset(dig, 0, sizeof(dig));
	memset(xof, 0, sizeof(xof));
	return(ok);
}

#endif
