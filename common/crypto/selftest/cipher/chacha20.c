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

#if defined(MP_CRYPTO_SELFTEST_CHACHA20) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20)

#include <crypto/selftest.h>
#include <crypto/chacha20.h>
#include <string.h>

/* RFC 8439 section 2.4.2, the first 64 bytes (one block).
 * Key 00010203...1f, nonce 000000090000004a00000000, counter 1. */

static const uint8_t _nonce[12] = {
	0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t _ct[64] = {
	0x5c, 0x90, 0x83, 0x8d, 0xb4, 0x48, 0x79, 0x74,
	0x3e, 0x6b, 0xfd, 0x58, 0xc6, 0x4e, 0x05, 0xa8,
	0xa2, 0xbc, 0x91, 0xa9, 0x13, 0xaf, 0x0e, 0x23,
	0x70, 0x4a, 0xcf, 0xba, 0xa0, 0xb8, 0x0d, 0x3d,
	0xa1, 0xa2, 0x0b, 0x20, 0x27, 0xb8, 0x93, 0x30,
	0x2e, 0xe2, 0x9e, 0x63, 0xf9, 0xc2, 0x22, 0xc1,
	0xda, 0x67, 0xf0, 0xb5, 0xfe, 0x79, 0x28, 0xdf,
	0xae, 0xa2, 0xa3, 0x91, 0xcd, 0x25, 0x1c, 0x21,
};

mp_bool_t mp_crypto_selftest_chacha20(void) {
	static mp_crypto_chacha20_t ctx;
	static const char msg[] = "Ladies and Gentlemen of the class of '99: If I could offer you o";
	uint8_t key[32];
	uint8_t out[64];
	uint8_t back[64];
	unsigned int i;
	mp_bool_t ok = NO;

	for(i = 0; i < 32; i++)
		key[i] = (uint8_t)i;
	if(mp_crypto_chacha20_init(&ctx, key, 32) != TRUE)
		goto done;
	mp_crypto_chacha20_set_nonce(&ctx, _nonce, 12);
	mp_crypto_chacha20_set_counter(&ctx, 1);
	if(mp_crypto_chacha20_update_block(&ctx, msg, 64, out) != 64)
		goto done;
	if(mp_crypto_selftest_equal(out, _ct, 64) != YES)
		goto done;
	mp_crypto_chacha20_set_nonce(&ctx, _nonce, 12);
	mp_crypto_chacha20_set_counter(&ctx, 1);
	if(mp_crypto_chacha20_update_block_inv(&ctx, _ct, 64, back) != 64)
		goto done;
	ok = mp_crypto_selftest_equal(back, msg, 64);
done:
	mp_crypto_chacha20_free(&ctx);
	memset(key, 0, sizeof(key));
	memset(out, 0, sizeof(out));
	memset(back, 0, sizeof(back));
	return(ok);
}

#endif
