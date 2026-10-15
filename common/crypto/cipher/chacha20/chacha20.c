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

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20

#include "chacha20.h"
#include <string.h>

/**
@defgroup mpCommonChacha20 ChaCha20

@ingroup mpCommon

@brief ChaCha20 stream cipher (RFC 8439)

A 256-bit key, 96-bit nonce, 20-round ARX (add-rotate-xor) stream cipher.
No table, no S-box: every step is adds, rotates and xors on 32-bit words,
so the cipher is constant time by construction, including the key
schedule (there is none: the key is loaded directly into the state).

The keystream is buffered: mp_chacha20_update_block() XORs any length
with the running keystream and refills the 64-byte block as needed.

@{
*/

#define _ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void _chacha20_qr(uint32_t *s, int a, int b, int c, int d) {
	s[a] += s[b]; s[d] ^= s[a]; s[d] = _ROTL32(s[d], 16);
	s[c] += s[d]; s[b] ^= s[c]; s[b] = _ROTL32(s[b], 12);
	s[a] += s[b]; s[d] ^= s[a]; s[d] = _ROTL32(s[d], 8);
	s[c] += s[d]; s[b] ^= s[c]; s[b] = _ROTL32(s[b], 7);
}

/* 20 rounds (10 double rounds) on a 16-word state, then add the input */
static void _chacha20_block(const uint32_t *in, uint32_t *out) {
	uint32_t x[16];
	for(int i = 0; i < 16; i++)
		x[i] = in[i];
	for(int i = 0; i < 10; i++) {
		/* column rounds */
		_chacha20_qr(x, 0, 4, 8, 12);
		_chacha20_qr(x, 1, 5, 9, 13);
		_chacha20_qr(x, 2, 6, 10, 14);
		_chacha20_qr(x, 3, 7, 11, 15);
		/* diagonal rounds */
		_chacha20_qr(x, 0, 5, 10, 15);
		_chacha20_qr(x, 1, 6, 11, 12);
		_chacha20_qr(x, 2, 7, 8, 13);
		_chacha20_qr(x, 3, 4, 9, 14);
	}
	for(int i = 0; i < 16; i++)
		out[i] = x[i] + in[i];
	memset(x, 0, sizeof(x));
}

static uint32_t _le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void _le32_put(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* build the 16-word input state and run one block, writing 64 keystream bytes */
static void _chacha20_keystream(mp_chacha20_t *ctx, uint8_t *ks) {
	static const uint32_t c[4] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
	uint32_t in[16], out[16];
	in[0] = c[0]; in[1] = c[1]; in[2] = c[2]; in[3] = c[3];
	for(int i = 0; i < 8; i++)
		in[4 + i] = _le32(ctx->key + 4 * i);
	in[12] = ctx->counter;
	in[13] = _le32(ctx->nonce);
	in[14] = _le32(ctx->nonce + 4);
	in[15] = _le32(ctx->nonce + 8);
	_chacha20_block(in, out);
	for(int i = 0; i < 16; i++)
		_le32_put(ks + 4 * i, out[i]);
	memset(in, 0, sizeof(in));
	memset(out, 0, sizeof(out));
}

/**
 * @brief Start a ChaCha20 context
 *
 * @param[in] ctx Uninitialised context
 * @param[in] key 32 bytes
 * @param[in] keylen Must be 32
 * @return TRUE when the key length is 32, FALSE otherwise
 */
mp_ret_t mp_chacha20_init(mp_chacha20_t *ctx, const void *key, unsigned int keylen) {
	if(!ctx || !key)
		return(FALSE);
	if(keylen != MP_CHACHA20_KEY)
		return(FALSE);
	memset(ctx, 0, sizeof(*ctx));
	memcpy(ctx->key, key, MP_CHACHA20_KEY);
	ctx->counter = 0;
	ctx->ksLeft = 0;
	return(TRUE);
}

/**
 * @brief Set the 96-bit nonce
 *
 * @param[in] ctx Started context
 * @param[in] nonce 12 bytes
 * @param[in] noncelen Must be 12
 */
void mp_chacha20_set_nonce(mp_chacha20_t *ctx, const void *nonce, unsigned int noncelen) {
	if(!ctx || !nonce || noncelen != MP_CHACHA20_NONCE)
		return;
	memcpy(ctx->nonce, nonce, MP_CHACHA20_NONCE);
	ctx->ksLeft = 0;
}

/**
 * @brief Set the 32-bit block counter
 *
 * @param[in] ctx Started context
 * @param[in] counter Block counter (1 for RFC 8439 data, 0 for the AEAD key)
 */
void mp_chacha20_set_counter(mp_chacha20_t *ctx, uint32_t counter) {
	if(!ctx)
		return;
	ctx->counter = counter;
	ctx->ksLeft = 0;
}

/**
 * @brief XOR the input with the keystream (encrypt = decrypt)
 *
 * @param[in] ctx Started context
 * @param[in] in Any length
 * @param[in] inlen Bytes to take from in
 * @param[out] out At least inlen bytes
 * @return Bytes written (== inlen)
 */
unsigned int mp_chacha20_update_block(mp_chacha20_t *ctx, const void *in, unsigned int inlen, uint8_t *out) {
	if(!ctx || !in || !out)
		return(0);
	const uint8_t *p = (const uint8_t *)in;
	uint8_t *o = out;
	unsigned int n = inlen;
	unsigned int k = MP_CHACHA20_BLOCK - ctx->ksLeft;
	while(n--) {
		if(ctx->ksLeft == 0) {
			_chacha20_keystream(ctx, ctx->ks);
			ctx->counter++;
			ctx->ksLeft = MP_CHACHA20_BLOCK;
			k = 0;
		}
		*o++ = *p++ ^ ctx->ks[k++];
		ctx->ksLeft--;
	}
	return(inlen);
}

unsigned int mp_chacha20_update_block_inv(mp_chacha20_t *ctx, const void *in, unsigned int inlen, uint8_t *out) {
	return(mp_chacha20_update_block(ctx, in, inlen, out));
}

/**
 * @brief Drop a ChaCha20 context
 *
 * @param[in] ctx Started context (or NULL)
 */
void mp_chacha20_free(mp_chacha20_t *ctx) {
	if(!ctx)
		return;
	memset(ctx, 0, sizeof(*ctx));
}

/**@}*/

#endif

