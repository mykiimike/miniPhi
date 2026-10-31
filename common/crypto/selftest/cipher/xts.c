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

#if defined(MP_CRYPTO_SELFTEST_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES)

#include <crypto/selftest.h>
#include <crypto/xts.h>
#include <string.h>

/* IEEE 1619. Key is 00010203... (Key1 || Key2), tweak is 2a followed by
 * zeros, plaintext is a0, a1, ... AES-128 covers 32 bytes and a 17-byte
 * unit (ciphertext stealing). AES-256 covers 32 bytes. Both directions
 * go through mp_crypto_xts_init, so the AES accelerator is checked when it
 * accepted the key. The flash-line call is a burned slot and is not run. */

static const uint8_t _tweak[16] = {
	0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t _ct128[32] = {
	0xbb, 0x00, 0x2c, 0xc1, 0x06, 0x66, 0xb2, 0xce,
	0x22, 0x52, 0xe3, 0xf1, 0xd1, 0xfb, 0x07, 0x57,
	0xef, 0x8e, 0x57, 0x78, 0x9d, 0x6d, 0x23, 0x2b,
	0x32, 0x19, 0x9f, 0x4f, 0x23, 0x20, 0x97, 0x19,
};

static const uint8_t _ct17[17] = {
	0x96, 0x67, 0xa0, 0xf3, 0x25, 0x8c, 0x2d, 0x03,
	0xe0, 0x17, 0x40, 0x50, 0x4d, 0x3e, 0xea, 0x3a,
	0xbb,
};

static const uint8_t _ct256[32] = {
	0x13, 0x4e, 0xd5, 0xb3, 0x17, 0xb5, 0xfb, 0x00,
	0x27, 0xd6, 0x68, 0xff, 0xd5, 0x39, 0xf5, 0x40,
	0xe7, 0xe9, 0x10, 0x81, 0xaf, 0x15, 0xc7, 0xd9,
	0xf8, 0x33, 0xc5, 0xec, 0x3e, 0x4d, 0xe9, 0xa4,
};

static mp_bool_t _unit(unsigned int keylen, unsigned int len, const uint8_t *ct) {
	static mp_crypto_xts_t xts;
	uint8_t key[64];
	uint8_t pt[32];
	uint8_t out[32];
	uint8_t back[32];
	unsigned int i;
	mp_bool_t ok = NO;

	for(i = 0; i < keylen; i++)
		key[i] = (uint8_t)i;
	for(i = 0; i < len; i++)
		pt[i] = (uint8_t)(0xa0 + i);
	if(mp_crypto_xts_init(&xts, key, keylen) != TRUE)
		goto done;
	if(mp_crypto_xts_set_tweak(&xts, _tweak, 16) != TRUE)
		goto done;
	if(mp_crypto_xts_update(&xts, pt, len, out) != len)
		goto done;
	if(mp_crypto_selftest_equal(out, ct, len) != YES)
		goto done;
	if(mp_crypto_xts_set_tweak(&xts, _tweak, 16) != TRUE)
		goto done;
	if(mp_crypto_xts_update_inv(&xts, ct, len, back) != len)
		goto done;
	ok = mp_crypto_selftest_equal(back, pt, len);
done:
	mp_crypto_xts_free(&xts);
	memset(key, 0, sizeof(key));
	memset(pt, 0, sizeof(pt));
	memset(out, 0, sizeof(out));
	memset(back, 0, sizeof(back));
	return(ok);
}

mp_bool_t mp_crypto_selftest_xts(void) {
	if(_unit(32, 32, _ct128) != YES)
		return(NO);
	if(_unit(32, 17, _ct17) != YES)
		return(NO);
	return(_unit(64, 32, _ct256));
}

#endif
