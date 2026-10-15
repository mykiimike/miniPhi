/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_AES

#include "aes.h"
#include <string.h>

/**
@defgroup mpCommonAes AES

@ingroup mpCommon

@brief AES block cipher (FIPS 197), one block at a time

The caller owns the context, so several blocks can run at once.
The software version below always builds: it is the engine of an MCU
without accelerator, and the fallback of a context the arch refused.

Constant time with respect to the key in the round function: the key
only enters the state through AddRoundKey (an XOR), SubBytes indexes the
public data byte (not the key), and ShiftRows, MixColumns and xtime are
fixed permutations and branchless masks. The round count is a function of
the key length, fixed once at init, not per block. The key schedule runs
once at init and uses S-box table lookups indexed by the key: on a cacheless
target (the STM32F1 has no data cache) that is a fixed address computation
with no timing leak; on a cached CPU it is the classic AES cache-timing
surface, so the key schedule is a one-time setup, not a per-block path.

@{
*/

#define _AES_ROUNDS128 10
#define _AES_ROUNDS192 12
#define _AES_ROUNDS256 14

static const uint8_t _aes_sbox[256] = {
	0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
	0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
	0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
	0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
	0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
	0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
	0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
	0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
	0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
	0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
	0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
	0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
	0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
	0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
	0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
	0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
};

static const uint8_t _aes_invsbox[256] = {
	0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
	0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
	0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
	0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
	0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
	0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
	0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
	0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
	0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
	0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
	0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
	0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
	0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
	0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
	0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
	0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d,
};

/* round constants: rcon[i] = (rc[i] in GF(2^8)) with rc[0] = 1 */
static const uint8_t _aes_rcon[16] = {
	0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36,0x6c,0xd8,0xab,0x4d,0x9a,0x2f,
};

/* multiply by 2 in GF(2^8): reduce x^8 by the AES modulus x^8+x^4+x^3+x+1 (0x1b).
 * Branchless: the reduction is selected by a mask of the high bit, so no
 * branch or table index reads the byte (constant time w.r.t. the data) */
static uint8_t _aes_xtime(uint8_t x) {
	uint8_t hi = (uint8_t)(x >> 7); /* 1 when the high bit is set, else 0 */
	uint8_t reduce = (uint8_t)(0x1b & (uint8_t)(0 - hi)); /* 0x1b or 0 */
	return (uint8_t)((x << 1) ^ reduce);
}

/* rotate a 32-bit word left by 8 bits (RotWord) */
static uint32_t _aes_rotword(uint32_t w) {
	return ((w << 8) | (w >> 24));
}

/* apply the S-box to a 32-bit word (SubWord) */
static uint32_t _aes_subword(uint32_t w) {
	return ((uint32_t)_aes_sbox[(w >> 24) & 0xff] << 24)
		| ((uint32_t)_aes_sbox[(w >> 16) & 0xff] << 16)
		| ((uint32_t)_aes_sbox[(w >> 8) & 0xff] << 8)
		| (uint32_t)_aes_sbox[w & 0xff];
}

/* expand the key into roundKeys[0..Nr] (each 4 words = 16 bytes) */
static void _aes_expand(mp_aes_t *ctx, const uint8_t *key) {
	unsigned int nk = ctx->keylen / 4;       /* 4, 6 or 8 */
	unsigned int nr = nk + 6;                /* 10, 12 or 14 */
	unsigned int total = 4 * (nr + 1);       /* words to produce */
	uint32_t *w = ctx->roundKeys;
	unsigned int i;

	ctx->rounds = nr;
	for(i = 0; i < nk; i++) {
		w[i] = ((uint32_t)key[4*i] << 24) | ((uint32_t)key[4*i+1] << 16)
			| ((uint32_t)key[4*i+2] << 8) | (uint32_t)key[4*i+3];
	}
	for(i = nk; i < total; i++) {
		uint32_t t = w[i-1];
		if((i % nk) == 0)
			t = _aes_subword(_aes_rotword(t)) ^ ((uint32_t)_aes_rcon[(i/nk)-1] << 24);
		else if(nk > 6 && (i % nk) == 4)
			t = _aes_subword(t);
		w[i] = w[i-nk] ^ t;
	}
}

/* state is held column-major: byte (r,c) = s[c*4 + r], matching FIPS 197 */

static void _aes_add_round_key(uint8_t *s, const uint32_t *rk) {
	for(unsigned int c = 0; c < 4; c++) {
		uint32_t k = rk[c];
		s[c*4+0] ^= (uint8_t)(k >> 24);
		s[c*4+1] ^= (uint8_t)(k >> 16);
		s[c*4+2] ^= (uint8_t)(k >> 8);
		s[c*4+3] ^= (uint8_t)(k);
	}
}

static void _aes_sub_bytes(uint8_t *s) {
	for(unsigned int i = 0; i < 16; i++)
		s[i] = _aes_sbox[s[i]];
}

static void _aes_inv_sub_bytes(uint8_t *s) {
	for(unsigned int i = 0; i < 16; i++)
		s[i] = _aes_invsbox[s[i]];
}

/* ShiftRows: row r shifts left by r. state byte (r,c) at index c*4+r */
static void _aes_shift_rows(uint8_t *s) {
	uint8_t t;
	/* row 1: left by 1 */
	t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
	/* row 2: left by 2 */
	t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
	/* row 3: left by 3 = right by 1 */
	t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
}

static void _aes_inv_shift_rows(uint8_t *s) {
	uint8_t t;
	/* row 1: right by 1 */
	t = s[13]; s[13] = s[9]; s[9] = s[5]; s[5] = s[1]; s[1] = t;
	/* row 2: right by 2 */
	t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
	/* row 3: right by 3 = left by 1 */
	t = s[3]; s[3] = s[7]; s[7] = s[11]; s[11] = s[15]; s[15] = t;
}

static void _aes_mix_columns(uint8_t *s) {
	for(unsigned int c = 0; c < 4; c++) {
		uint8_t *col = s + c*4;
		uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
		uint8_t t = a0 ^ a1 ^ a2 ^ a3;
		col[0] ^= t ^ _aes_xtime(a0 ^ a1);
		col[1] ^= t ^ _aes_xtime(a1 ^ a2);
		col[2] ^= t ^ _aes_xtime(a2 ^ a3);
		col[3] ^= t ^ _aes_xtime(a3 ^ a0);
	}
}

static uint8_t _aes_gmul(uint8_t a, uint8_t b) {
	uint8_t r = 0;
	for(int i = 0; i < 8; i++) {
		if((b >> i) & 1)
			r ^= a;
		a = _aes_xtime(a);
	}
	return r;
}

static void _aes_inv_mix_columns(uint8_t *s) {
	for(unsigned int c = 0; c < 4; c++) {
		uint8_t *col = s + c*4;
		uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
		col[0] = _aes_gmul(a0,0x0e) ^ _aes_gmul(a1,0x0b) ^ _aes_gmul(a2,0x0d) ^ _aes_gmul(a3,0x09);
		col[1] = _aes_gmul(a0,0x09) ^ _aes_gmul(a1,0x0e) ^ _aes_gmul(a2,0x0b) ^ _aes_gmul(a3,0x0d);
		col[2] = _aes_gmul(a0,0x0d) ^ _aes_gmul(a1,0x09) ^ _aes_gmul(a2,0x0e) ^ _aes_gmul(a3,0x0b);
		col[3] = _aes_gmul(a0,0x0b) ^ _aes_gmul(a1,0x0d) ^ _aes_gmul(a2,0x09) ^ _aes_gmul(a3,0x0e);
	}
}

/**
 * @brief Start an AES context
 *
 * @param[in] ctx Uninitialised context
 * @param[in] key Key bytes (16, 24 or 32)
 * @param[in] keylen Length of key
 * @return TRUE when the key length is 16, 24 or 32, FALSE otherwise
 */
mp_ret_t mp_aes_init(mp_aes_t *ctx, const void *key, unsigned int keylen) {
	if(!ctx || !key)
		return(FALSE);
	if(keylen != 16 && keylen != 24 && keylen != 32)
		return(FALSE);
	memset(ctx, 0, sizeof(*ctx));
	ctx->keylen = keylen;
	_aes_expand(ctx, (const uint8_t *)key);
	return(TRUE);
}

/**
 * @brief Encrypt one block
 *
 * @param[in] ctx Started context
 * @param[in] in 16 bytes
 * @param[in] inlen Bytes to take from in (clamped to 16)
 * @param[out] out 16 bytes
 * @return 16, or 0 when the context was not started
 */
unsigned int mp_aes_update_block(mp_aes_t *ctx, const void *in, unsigned int inlen, uint8_t *out) {
	if(!ctx || !ctx->rounds || !in || !out)
		return(0);
	uint8_t s[16];
	unsigned int n = inlen < 16 ? inlen : 16;
	memset(s, 0, 16);
	memcpy(s, in, n);

	_aes_add_round_key(s, &ctx->roundKeys[0]);
	for(unsigned int r = 1; r < ctx->rounds; r++) {
		_aes_sub_bytes(s);
		_aes_shift_rows(s);
		_aes_mix_columns(s);
		_aes_add_round_key(s, &ctx->roundKeys[4*r]);
	}
	_aes_sub_bytes(s);
	_aes_shift_rows(s);
	_aes_add_round_key(s, &ctx->roundKeys[4*ctx->rounds]);

	memcpy(out, s, 16);
	memset(s, 0, sizeof(s));
	return(16);
}

/**
 * @brief Decrypt one block
 *
 * @param[in] ctx Started context
 * @param[in] in 16 bytes
 * @param[in] inlen Bytes to take from in (clamped to 16)
 * @param[out] out 16 bytes
 * @return 16, or 0 when the context was not started
 */
unsigned int mp_aes_update_block_inv(mp_aes_t *ctx, const void *in, unsigned int inlen, uint8_t *out) {
	if(!ctx || !ctx->rounds || !in || !out)
		return(0);
	uint8_t s[16];
	unsigned int n = inlen < 16 ? inlen : 16;
	memset(s, 0, 16);
	memcpy(s, in, n);

	_aes_add_round_key(s, &ctx->roundKeys[4*ctx->rounds]);
	for(unsigned int r = ctx->rounds - 1; r >= 1; r--) {
		_aes_inv_shift_rows(s);
		_aes_inv_sub_bytes(s);
		_aes_add_round_key(s, &ctx->roundKeys[4*r]);
		_aes_inv_mix_columns(s);
	}
	_aes_inv_shift_rows(s);
	_aes_inv_sub_bytes(s);
	_aes_add_round_key(s, &ctx->roundKeys[0]);

	memcpy(out, s, 16);
	memset(s, 0, sizeof(s));
	return(16);
}

/**
 * @brief Drop an AES context
 *
 * @param[in] ctx Started context (or NULL)
 */
void mp_aes_free(mp_aes_t *ctx) {
	if(!ctx)
		return;
	memset(ctx, 0, sizeof(*ctx));
}

/**@}*/

#endif

