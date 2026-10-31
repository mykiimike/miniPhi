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

#if defined(MP_CRYPTO_SELFTEST_BLAKE2S) && defined(SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S)

#include <crypto/selftest.h>
#include <crypto/blake2s.h>
#include <string.h>

/* BLAKE2s-256, unkeyed, of the empty message and of "abc". */

static const uint8_t _empty[32] = {
	0x69, 0x21, 0x7a, 0x30, 0x79, 0x90, 0x80, 0x94,
	0xe1, 0x11, 0x21, 0xd0, 0x42, 0x35, 0x4a, 0x7c,
	0x1f, 0x55, 0xb6, 0x48, 0x2c, 0xa1, 0xa5, 0x1e,
	0x1b, 0x25, 0x0d, 0xfd, 0x1e, 0xd0, 0xee, 0xf9,
};

static const uint8_t _abc[32] = {
	0x50, 0x8c, 0x5e, 0x8c, 0x32, 0x7c, 0x14, 0xe2,
	0xe1, 0xa7, 0x2b, 0xa3, 0x4e, 0xeb, 0x45, 0x2f,
	0x37, 0x45, 0x8b, 0x20, 0x9e, 0xd6, 0x3a, 0x29,
	0x4d, 0x99, 0x9b, 0x4c, 0x86, 0x67, 0x59, 0x82,
};

mp_bool_t mp_crypto_selftest_blake2s(void) {
	uint8_t dig[32];
	mp_bool_t ok;

	mp_crypto_blake2s("", 0, dig);
	if(mp_crypto_selftest_equal(dig, _empty, 32) != YES) {
		memset(dig, 0, sizeof(dig));
		return(NO);
	}
	mp_crypto_blake2s("abc", 3, dig);
	ok = mp_crypto_selftest_equal(dig, _abc, 32);
	memset(dig, 0, sizeof(dig));
	return(ok);
}

#endif
