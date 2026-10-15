/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 * Copyright (C) 2026  VERMAN                                              *
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_MODE_CHACHA20_POLY1305

#include "chacha20_poly1305.h"
#include <string.h>

/**
@defgroup mpCommonChacha20Poly1305 ChaCha20-Poly1305

@ingroup mpCommon

@brief ChaCha20-Poly1305 AEAD (RFC 8439)

A one-shot AEAD: the one-time Poly1305 key is the first 32 bytes of the
ChaCha20 block 0 keystream, the message is encrypted from block 1, and
the tag is Poly1305 over aad || pad || ct || pad || le64(aad) || le64(ct).
Constant time: the tag compare reads every byte; the only secret that
touches a branch is the tag-mismatch flag, which is the same for every
forgery.

@{
*/

static void _le64_put(uint8_t *p, uint64_t v) {
	for(int i = 0; i < 8; i++)
		p[i] = (uint8_t)(v >> (8 * i));
}

/* feed a buffer to poly1305 then zero-pad to a 16-byte boundary */
static void _cp_poly_feed_pad(mp_poly1305_t *poly, const void *data, unsigned int len) {
	if(!len)
		return;
	mp_poly1305_update(poly, data, len);
	unsigned int pad = 16 - (len % 16);
	if(pad != 16) {
		uint8_t zero[16] = {0};
		mp_poly1305_update(poly, zero, pad);
	}
}

/* compute the AEAD tag over aad and ciphertext */
static void _cp_poly_tag(mp_poly1305_t *poly, const uint8_t *otk,
		const void *aad, unsigned int aadlen,
		const void *ct, unsigned int ctlen, uint8_t *tag) {
	uint8_t lens[16];
	mp_poly1305_init(poly, otk, MP_POLY1305_KEY);
	_cp_poly_feed_pad(poly, aad, aadlen);
	_cp_poly_feed_pad(poly, ct, ctlen);
	_le64_put(lens + 0, (uint64_t)aadlen);
	_le64_put(lens + 8, (uint64_t)ctlen);
	mp_poly1305_update(poly, lens, 16);
	mp_poly1305_final(poly, tag);
}

/**
 * @brief Encrypt and authenticate
 */
mp_ret_t mp_chacha20_poly1305_encrypt(const void *key, unsigned int keylen,
		const void *nonce, unsigned int noncelen,
		const void *aad, unsigned int aadlen,
		const void *pt, unsigned int ptlen,
		uint8_t *ct, uint8_t *tag) {
	if(!key || keylen != MP_CHACHA20_POLY1305_KEY || !nonce || noncelen != MP_CHACHA20_POLY1305_NONCE)
		return(FALSE);
	if(!tag || (ptlen && !ct) || (ptlen && !pt))
		return(FALSE);

	mp_chacha20_t cc;
	mp_poly1305_t poly;
	uint8_t otk[32];
	uint8_t zero[32] = {0};

	mp_chacha20_init(&cc, key, keylen);
	mp_chacha20_set_nonce(&cc, nonce, noncelen);
	mp_chacha20_set_counter(&cc, 0);
	/* one-time Poly1305 key = first 32 bytes of block 0 */
	mp_chacha20_update_block(&cc, zero, 32, otk);

	/* message keystream starts at block 1 */
	mp_chacha20_set_counter(&cc, 1);
	if(ptlen)
		mp_chacha20_update_block(&cc, pt, ptlen, ct);

	_cp_poly_tag(&poly, otk, aad, aadlen, ct, ptlen, tag);

	mp_chacha20_free(&cc);
	memset(otk, 0, sizeof(otk));
	return(TRUE);
}

/**
 * @brief Verify and decrypt
 */
mp_ret_t mp_chacha20_poly1305_decrypt(const void *key, unsigned int keylen,
		const void *nonce, unsigned int noncelen,
		const void *aad, unsigned int aadlen,
		const void *ct, unsigned int ctlen,
		const void *tag,
		uint8_t *pt) {
	if(!key || keylen != MP_CHACHA20_POLY1305_KEY || !nonce || noncelen != MP_CHACHA20_POLY1305_NONCE || !tag)
		return(FALSE);
	if(ctlen && (!ct || !pt))
		return(FALSE);

	mp_chacha20_t cc;
	mp_poly1305_t poly;
	uint8_t otk[32];
	uint8_t zero[32] = {0};
	uint8_t want[MP_CHACHA20_POLY1305_TAG];

	mp_chacha20_init(&cc, key, keylen);
	mp_chacha20_set_nonce(&cc, nonce, noncelen);
	mp_chacha20_set_counter(&cc, 0);
	mp_chacha20_update_block(&cc, zero, 32, otk);

	/* verify the tag before releasing any plaintext */
	_cp_poly_tag(&poly, otk, aad, aadlen, ct, ctlen, want);
	if(mp_mac_equal(want, tag, MP_CHACHA20_POLY1305_TAG) != YES) {
		mp_chacha20_free(&cc);
		memset(otk, 0, sizeof(otk));
		memset(want, 0, sizeof(want));
		if(pt && ctlen)
			memset(pt, 0, ctlen);
		return(FALSE);
	}

	mp_chacha20_set_counter(&cc, 1);
	if(ctlen)
		mp_chacha20_update_block(&cc, ct, ctlen, pt);

	mp_chacha20_free(&cc);
	memset(otk, 0, sizeof(otk));
	memset(want, 0, sizeof(want));
	return(TRUE);
}

/**@}*/

#endif
