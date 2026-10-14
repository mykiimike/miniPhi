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

#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA1

/**
@defgroup mpCommonSha1 SHA-1

@ingroup mpCommon

@brief SHA-1 digest (FIPS 180-4)

SHA-1 is broken for signatures and certificates. Keep it for the
protocols that still name it.

The caller owns the context, so several digests can run at once.
The software version below always builds: it is the engine of an MCU
without accelerator, and the fallback of a context the arch refused.

@{
*/

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static const uint32_t _sha1_iv[5] = {
	0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0,
};

static uint32_t _sha1_load(const uint8_t *p) {
	return(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static void _sha1_store(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)(value >> 24);
	p[1] = (uint8_t)(value >> 16);
	p[2] = (uint8_t)(value >> 8);
	p[3] = (uint8_t)value;
}

/* the schedule lives in 16 words reused in a ring, so the stack stays small on MSP430 */
static void _sha1_block(uint32_t state[5], const uint8_t *block) {
	uint32_t w[16];
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
	uint32_t f, k, t;
	int i;

	for(i = 0; i < 80; i++) {
		if(i < 16)
			w[i] = _sha1_load(block + i * 4);
		else {
			t = w[(i + 13) & 15] ^ w[(i + 8) & 15] ^ w[(i + 2) & 15] ^ w[i & 15];
			w[i & 15] = ROL(t, 1);
		}

		if(i < 20) {
			f = (b & c) | (~b & d);
			k = 0x5a827999;
		}
		else if(i < 40) {
			f = b ^ c ^ d;
			k = 0x6ed9eba1;
		}
		else if(i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdc;
		}
		else {
			f = b ^ c ^ d;
			k = 0xca62c1d6;
		}

		t = ROL(a, 5) + f + e + k + w[i & 15];
		e = d;
		d = c;
		c = ROL(b, 30);
		b = a;
		a = t;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

static void _sha1_soft_update(mp_sha1_t *ctx, const uint8_t *data, unsigned int size) {
	unsigned int take;

	ctx->length += size;
	while(size > 0) {
		take = MP_SHA1_BLOCK - ctx->used;
		if(take > size)
			take = size;
		memcpy(ctx->block + ctx->used, data, take);
		ctx->used += take;
		data += take;
		size -= take;
		if(ctx->used == MP_SHA1_BLOCK) {
			_sha1_block(ctx->state, ctx->block);
			ctx->used = 0;
		}
	}
}

static void _sha1_soft_final(mp_sha1_t *ctx, uint8_t *digest) {
	uint64_t bits = ctx->length * 8;
	int i;

	ctx->block[ctx->used++] = 0x80;
	if(ctx->used > MP_SHA1_BLOCK - 8) {
		memset(ctx->block + ctx->used, 0, MP_SHA1_BLOCK - ctx->used);
		_sha1_block(ctx->state, ctx->block);
		ctx->used = 0;
	}
	memset(ctx->block + ctx->used, 0, MP_SHA1_BLOCK - 8 - ctx->used);
	for(i = 0; i < 8; i++)
		ctx->block[MP_SHA1_BLOCK - 8 + i] = (uint8_t)(bits >> (56 - i * 8));
	_sha1_block(ctx->state, ctx->block);

	for(i = 0; i < 5; i++)
		_sha1_store(digest + i * 4, ctx->state[i]);
}

/**
 * @brief Start a digest
 *
 * @param[in] ctx Context, owned by the caller
 */
void mp_sha1_init(mp_sha1_t *ctx) {
	memcpy(ctx->state, _sha1_iv, sizeof(ctx->state));
	ctx->length = 0;
	ctx->used = 0;
#ifdef MP_ARCH_SHA1
	ctx->onHardware = mp_arch_sha1_init(&ctx->hw);
#endif
}

/**
 * @brief Hash more bytes
 *
 * @param[in] ctx Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_sha1_update(mp_sha1_t *ctx, const void *data, unsigned int size) {
#ifdef MP_ARCH_SHA1
	if(ctx->onHardware) {
		mp_arch_sha1_update(&ctx->hw, data, size);
		return;
	}
#endif
	_sha1_soft_update(ctx, data, size);
}

/**
 * @brief End a digest
 *
 * The context is wiped, start again with mp_sha1_init().
 *
 * @param[in] ctx Context
 * @param[out] digest MP_SHA1_SIZE bytes
 */
void mp_sha1_final(mp_sha1_t *ctx, uint8_t digest[MP_SHA1_SIZE]) {
#ifdef MP_ARCH_SHA1
	if(ctx->onHardware)
		mp_arch_sha1_final(&ctx->hw, digest);
	else
		_sha1_soft_final(ctx, digest);
#else
	_sha1_soft_final(ctx, digest);
#endif
	memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief Digest a buffer in one call
 *
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 * @param[out] digest MP_SHA1_SIZE bytes
 */
void mp_sha1(const void *data, unsigned int size, uint8_t digest[MP_SHA1_SIZE]) {
	mp_sha1_t ctx;

	mp_sha1_init(&ctx);
	mp_sha1_update(&ctx, data, size);
	mp_sha1_final(&ctx, digest);
}

/**@}*/

#endif
