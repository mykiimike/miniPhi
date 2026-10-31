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

#if defined(MP_CRYPTO_SELFTEST_AES) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES)

#include <crypto/selftest.h>
#include <crypto/aes.h>
#include <string.h>

/* FIPS 197 C.1 / C.2 / C.3. Plaintext 00112233445566778899aabbccddeeff.
 * The key is 00010203... of 16, 24 or 32 bytes. Encrypt and decrypt both
 * go through mp_crypto_aes_init, so an accelerator that accepted the key is checked. */

static const uint8_t _pt[16] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static const uint8_t _ct128[16] = {
	0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
	0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a,
};

static const uint8_t _ct192[16] = {
	0xdd, 0xa9, 0x7c, 0xa4, 0x86, 0x4c, 0xdf, 0xe0,
	0x6e, 0xaf, 0x70, 0xa0, 0xec, 0x0d, 0x71, 0x91,
};

static const uint8_t _ct256[16] = {
	0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf,
	0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60, 0x89,
};

static mp_bool_t _block(unsigned int keylen, const uint8_t *ct) {
	static mp_crypto_aes_t aes;
	uint8_t key[32];
	uint8_t out[16];
	uint8_t back[16];
	unsigned int i;
	mp_bool_t ok = NO;

	for(i = 0; i < keylen; i++)
		key[i] = (uint8_t)i;
	if(mp_crypto_aes_init(&aes, key, keylen) != TRUE)
		goto done;
	if(mp_crypto_aes_update_block(&aes, _pt, 16, out) != 16)
		goto done;
	if(mp_crypto_selftest_equal(out, ct, 16) != YES)
		goto done;
	if(mp_crypto_aes_update_block_inv(&aes, ct, 16, back) != 16)
		goto done;
	ok = mp_crypto_selftest_equal(back, _pt, 16);
done:
	mp_crypto_aes_free(&aes);
	memset(key, 0, sizeof(key));
	memset(out, 0, sizeof(out));
	memset(back, 0, sizeof(back));
	return(ok);
}

mp_bool_t mp_crypto_selftest_aes(void) {
	if(_block(16, _ct128) != YES)
		return(NO);
	if(_block(24, _ct192) != YES)
		return(NO);
	return(_block(32, _ct256));
}

#endif
