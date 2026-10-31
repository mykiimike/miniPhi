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

#if defined(MP_CRYPTO_SELFTEST_SHA512) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512)

#include <crypto/selftest.h>
#include <crypto/sha512.h>
#include <string.h>

/* FIPS 180-4, SHA-512("abc"). */

static const uint8_t _abc[64] = {
	0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba,
	0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
	0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2,
	0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
	0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8,
	0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
	0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e,
	0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f,
};

mp_bool_t mp_crypto_selftest_sha512(void) {
	uint8_t dig[64];
	mp_bool_t ok;

	mp_crypto_sha512("abc", 3, dig);
	ok = mp_crypto_selftest_equal(dig, _abc, 64);
	memset(dig, 0, sizeof(dig));
	return(ok);
}

#endif
