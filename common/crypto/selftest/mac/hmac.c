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

#if defined(MP_CRYPTO_SELFTEST_HMAC) && defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256)

#include <crypto/selftest.h>
#include <crypto/hmac.h>
#include <string.h>

/* HMAC-SHA-256, key "key", message "miniPhi". The key is in RAM.
 * A fuse slot is not part of this check. */

static const uint8_t _tag[32] = {
	0x3c, 0x4b, 0xbd, 0x84, 0x42, 0x4f, 0x7a, 0x31,
	0x43, 0x8d, 0xed, 0xb5, 0x2e, 0x47, 0x9e, 0x0d,
	0xdf, 0x3c, 0xb2, 0xeb, 0x17, 0x3b, 0xb6, 0x4b,
	0x6d, 0x8f, 0xa4, 0x2b, 0xe1, 0x01, 0x57, 0xfd,
};

mp_bool_t mp_crypto_selftest_hmac(void) {
	uint8_t tag[32];
	unsigned int n;
	mp_bool_t ok;

	n = mp_crypto_hmac("SHA256", "key", 3, "miniPhi", 7, tag);
	ok = (n == 32 && mp_crypto_selftest_equal(tag, _tag, 32) == YES) ? YES : NO;
	memset(tag, 0, sizeof(tag));
	return(ok);
}

#endif
