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

#if defined(MP_CRYPTO_SELFTEST_SHA384) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384)

#include <crypto/selftest.h>
#include <crypto/sha384.h>
#include <string.h>

/* FIPS 180-4, SHA-384("abc"). */

static const uint8_t _abc[48] = {
	0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
	0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
	0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
	0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
	0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
	0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7,
};

mp_bool_t mp_crypto_selftest_sha384(void) {
	uint8_t dig[48];
	mp_bool_t ok;

	mp_crypto_sha384("abc", 3, dig);
	ok = mp_crypto_selftest_equal(dig, _abc, 48);
	memset(dig, 0, sizeof(dig));
	return(ok);
}

#endif
