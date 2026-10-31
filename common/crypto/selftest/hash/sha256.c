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

#if defined(MP_CRYPTO_SELFTEST_SHA256) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256)

#include <crypto/selftest.h>
#include <crypto/sha256.h>
#include <string.h>

/* FIPS 180-4. Empty, "abc", and 64 bytes of 'a' so a second block is hashed.
 * mp_crypto_sha256 is the public call, so an accelerator that accepted it is checked. */

static const uint8_t _empty[32] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
	0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
	0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
	0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
};

static const uint8_t _abc[32] = {
	0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
	0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
	0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
	0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
};

static const uint8_t _aa[32] = {
	0xff, 0xe0, 0x54, 0xfe, 0x7a, 0xe0, 0xcb, 0x6d,
	0xc6, 0x5c, 0x3a, 0xf9, 0xb6, 0x1d, 0x52, 0x09,
	0xf4, 0x39, 0x85, 0x1d, 0xb4, 0x3d, 0x0b, 0xa5,
	0x99, 0x73, 0x37, 0xdf, 0x15, 0x46, 0x68, 0xeb,
};

static mp_bool_t _one(const void *msg, unsigned int len, const uint8_t *expect) {
	uint8_t dig[32];
	mp_bool_t ok;

	mp_crypto_sha256(msg, len, dig);
	ok = mp_crypto_selftest_equal(dig, expect, 32);
	memset(dig, 0, sizeof(dig));
	return(ok);
}

mp_bool_t mp_crypto_selftest_sha256(void) {
	uint8_t block[64];
	unsigned int i;

	if(_one("", 0, _empty) != YES)
		return(NO);
	if(_one("abc", 3, _abc) != YES)
		return(NO);
	for(i = 0; i < 64; i++)
		block[i] = 'a';
	return(_one(block, 64, _aa));
}

#endif
