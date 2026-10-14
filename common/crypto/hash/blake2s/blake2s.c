/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S

/**
@defgroup mpCommonBlake2s BLAKE2s

@ingroup mpCommon

@brief BLAKE2s-256 digest (RFC 7693)

The caller owns the context. A full block stays buffered until another
byte arrives or the digest ends, so the last block can carry the final flag.

@{
*/

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t _blake2s_iv[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

static const uint8_t _blake2s_sigma[10][16] = {
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
	{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
	{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
	{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
	{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
	{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
	{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
	{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
	{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
};

static uint32_t _blake2s_load(const uint8_t *p) {
	return((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void _blake2s_store(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}

#define G(a, b, c, d, x, y) do { \
	v[a] = v[a] + v[b] + m[x]; \
	v[d] = ROR(v[d] ^ v[a], 16); \
	v[c] = v[c] + v[d]; \
	v[b] = ROR(v[b] ^ v[c], 12); \
	v[a] = v[a] + v[b] + m[y]; \
	v[d] = ROR(v[d] ^ v[a], 8); \
	v[c] = v[c] + v[d]; \
	v[b] = ROR(v[b] ^ v[c], 7); \
} while(0)

static void _blake2s_compress(mp_blake2s_t *ctx, const uint8_t *block, int last) {
	uint32_t m[16];
	uint32_t v[16];
	uint32_t t0, t1;
	int i, round;

	ctx->count += ctx->used;
	t0 = (uint32_t)ctx->count;
	t1 = (uint32_t)(ctx->count >> 32);

	for(i = 0; i < 16; i++)
		m[i] = _blake2s_load(block + i * 4);
	for(i = 0; i < 8; i++) {
		v[i] = ctx->state[i];
		v[i + 8] = _blake2s_iv[i];
	}
	v[12] ^= t0;
	v[13] ^= t1;
	if(last)
		v[14] ^= 0xffffffffUL;

	for(round = 0; round < 10; round++) {
		const uint8_t *s = _blake2s_sigma[round];

		G(0, 4,  8, 12, s[ 0], s[ 1]);
		G(1, 5,  9, 13, s[ 2], s[ 3]);
		G(2, 6, 10, 14, s[ 4], s[ 5]);
		G(3, 7, 11, 15, s[ 6], s[ 7]);
		G(0, 5, 10, 15, s[ 8], s[ 9]);
		G(1, 6, 11, 12, s[10], s[11]);
		G(2, 7,  8, 13, s[12], s[13]);
		G(3, 4,  9, 14, s[14], s[15]);
	}

	for(i = 0; i < 8; i++)
		ctx->state[i] ^= v[i] ^ v[i + 8];
	ctx->used = 0;
}

#undef G

static void _blake2s_soft_update(mp_blake2s_t *ctx, const uint8_t *data, unsigned int size) {
	unsigned int fill;

	while(size > 0) {
		fill = MP_BLAKE2S_BLOCK - ctx->used;
		if(size > fill) {
			memcpy(ctx->block + ctx->used, data, fill);
			ctx->used = MP_BLAKE2S_BLOCK;
			_blake2s_compress(ctx, ctx->block, 0);
			data += fill;
			size -= fill;
		}
		else {
			memcpy(ctx->block + ctx->used, data, size);
			ctx->used += size;
			return;
		}
	}
}

static void _blake2s_soft_init(mp_blake2s_t *ctx, const uint8_t *key, unsigned int keylen) {
	memcpy(ctx->state, _blake2s_iv, sizeof(ctx->state));
	ctx->state[0] ^= 0x01010000UL | ((uint32_t)keylen << 8) | MP_BLAKE2S_SIZE;
	ctx->count = 0;
	ctx->used = 0;
	if(keylen > 0) {
		memset(ctx->block, 0, MP_BLAKE2S_BLOCK);
		memcpy(ctx->block, key, keylen);
		ctx->used = MP_BLAKE2S_BLOCK;
	}
}

/**
 * @brief Start an unkeyed digest
 *
 * @param[in] ctx Context, owned by the caller
 */
void mp_blake2s_init(mp_blake2s_t *ctx) {
	_blake2s_soft_init(ctx, NULL, 0);
#ifdef MP_ARCH_BLAKE2S
	ctx->onHardware = mp_arch_blake2s_init(&ctx->hw);
#endif
}

/**
 * @brief Start a keyed digest
 *
 * A key makes this a MAC. The result differs from the unkeyed digest of the same bytes.
 *
 * @param[in] ctx Context, owned by the caller
 * @param[in] key Key bytes, ignored when keylen is 0
 * @param[in] keylen Key length, cut at MP_BLAKE2S_KEY
 */
void mp_blake2s_init_key(mp_blake2s_t *ctx, const void *key, unsigned int keylen) {
	if(keylen > MP_BLAKE2S_KEY)
		keylen = MP_BLAKE2S_KEY;
	if(!key)
		keylen = 0;
	_blake2s_soft_init(ctx, key, keylen);
#ifdef MP_ARCH_BLAKE2S
	ctx->onHardware = NO;
	if(keylen == 0)
		ctx->onHardware = mp_arch_blake2s_init(&ctx->hw);
#endif
}

/**
 * @brief Hash more bytes
 *
 * @param[in] ctx Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_blake2s_update(mp_blake2s_t *ctx, const void *data, unsigned int size) {
#ifdef MP_ARCH_BLAKE2S
	if(ctx->onHardware) {
		mp_arch_blake2s_update(&ctx->hw, data, size);
		return;
	}
#endif
	_blake2s_soft_update(ctx, data, size);
}

/**
 * @brief End a digest
 *
 * The context is wiped, start again with mp_blake2s_init().
 *
 * @param[in] ctx Context
 * @param[out] digest MP_BLAKE2S_SIZE bytes
 */
void mp_blake2s_final(mp_blake2s_t *ctx, uint8_t digest[MP_BLAKE2S_SIZE]) {
	int i;

#ifdef MP_ARCH_BLAKE2S
	if(ctx->onHardware)
		mp_arch_blake2s_final(&ctx->hw, digest);
	else
#endif
	{
		memset(ctx->block + ctx->used, 0, MP_BLAKE2S_BLOCK - ctx->used);
		_blake2s_compress(ctx, ctx->block, 1);
		for(i = 0; i < 8; i++)
			_blake2s_store(digest + i * 4, ctx->state[i]);
	}
	memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief Digest a buffer in one call
 *
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 * @param[out] digest MP_BLAKE2S_SIZE bytes
 */
void mp_blake2s(const void *data, unsigned int size, uint8_t digest[MP_BLAKE2S_SIZE]) {
	mp_blake2s_t ctx;

	mp_blake2s_init(&ctx);
	mp_blake2s_update(&ctx, data, size);
	mp_blake2s_final(&ctx, digest);
}

/**@}*/

#endif
