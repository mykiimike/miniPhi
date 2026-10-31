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

#if defined(MP_CRYPTO_SELFTEST_SHA1) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1)

#include <crypto/selftest.h>
#include <crypto/sha1.h>
#include <string.h>

/* FIPS 180-4. Empty, "abc", and 64 bytes of 'a' so a second block is hashed.
 * mp_crypto_sha1 is the public call, so an accelerator that accepted it is checked. */

static const uint8_t _empty[20] = {
	0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
	0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09,
};

static const uint8_t _abc[20] = {
	0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
	0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d,
};

static const uint8_t _aa[20] = {
	0x00, 0x98, 0xba, 0x82, 0x4b, 0x5c, 0x16, 0x42, 0x7b, 0xd7,
	0xa1, 0x12, 0x2a, 0x5a, 0x44, 0x2a, 0x25, 0xec, 0x64, 0x4d,
};

static mp_bool_t _one(const void *msg, unsigned int len, const uint8_t *expect) {
	uint8_t dig[20];
	mp_bool_t ok;

	mp_crypto_sha1(msg, len, dig);
	ok = mp_crypto_selftest_equal(dig, expect, 20);
	memset(dig, 0, sizeof(dig));
	return(ok);
}

mp_bool_t mp_crypto_selftest_sha1(void) {
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
