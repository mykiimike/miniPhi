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

#if defined(MP_CRYPTO_SELFTEST_POLY1305) && defined(SUPPORT_COMMON_CRYPTO_MAC_POLY1305)

#include <crypto/selftest.h>
#include <crypto/poly1305.h>
#include <string.h>

/* RFC 8439 section 2.5.2. Message "Cryptographic Forum Research Group". */

static const uint8_t _key[32] = {
	0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33,
	0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8,
	0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd,
	0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b,
};

static const uint8_t _tag[16] = {
	0xa8, 0x06, 0x1d, 0xc1, 0x30, 0x51, 0x36, 0xc6,
	0xc2, 0x2b, 0x8b, 0xaf, 0x0c, 0x01, 0x27, 0xa9,
};

mp_bool_t mp_crypto_selftest_poly1305(void) {
	static mp_crypto_poly1305_t ctx;
	static const char msg[] = "Cryptographic Forum Research Group";
	uint8_t tag[16];
	mp_bool_t ok = NO;

	if(mp_crypto_poly1305_init(&ctx, _key, 32) != TRUE)
		goto done;
	mp_crypto_poly1305_update(&ctx, msg, sizeof(msg) - 1);
	if(mp_crypto_poly1305_final(&ctx, tag) != 16)
		goto done;
	ok = mp_crypto_selftest_equal(tag, _tag, 16);
done:
	mp_crypto_poly1305_free(&ctx);
	memset(tag, 0, sizeof(tag));
	return(ok);
}

#endif
