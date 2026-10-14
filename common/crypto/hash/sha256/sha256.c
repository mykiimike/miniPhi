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

#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA256

/**
@defgroup mpCommonSha256 SHA-256

@ingroup mpCommon

@brief SHA-256 digest (FIPS 180-4)

The caller owns the context, so several digests can run at once.
The software version below always builds: it is the engine of an MCU
without accelerator, and the fallback of a context the arch refused.

@{
*/

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t _sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static const uint32_t _sha256_iv[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

static uint32_t _sha256_load(const uint8_t *p) {
	return(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static void _sha256_store(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)(value >> 24);
	p[1] = (uint8_t)(value >> 16);
	p[2] = (uint8_t)(value >> 8);
	p[3] = (uint8_t)value;
}

/* the schedule lives in 16 words reused in a ring, so the stack stays small on MSP430 */
static void _sha256_block(uint32_t state[8], const uint8_t *block) {
	uint32_t w[16];
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
	uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
	uint32_t t1, t2, s0, s1;
	int i;

	for(i = 0; i < 64; i++) {
		if(i < 16)
			w[i] = _sha256_load(block + i * 4);
		else {
			s0 = w[(i + 1) & 15];
			s1 = w[(i + 14) & 15];
			s0 = ROR(s0, 7) ^ ROR(s0, 18) ^ (s0 >> 3);
			s1 = ROR(s1, 17) ^ ROR(s1, 19) ^ (s1 >> 10);
			w[i & 15] += s0 + s1 + w[(i + 9) & 15];
		}
		t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + _sha256_k[i] + w[i & 15];
		t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

static void _sha256_soft_update(mp_sha256_t *ctx, const uint8_t *data, unsigned int size) {
	unsigned int take;

	ctx->length += size;
	while(size > 0) {
		take = MP_SHA256_BLOCK - ctx->used;
		if(take > size)
			take = size;
		memcpy(ctx->block + ctx->used, data, take);
		ctx->used += take;
		data += take;
		size -= take;
		if(ctx->used == MP_SHA256_BLOCK) {
			_sha256_block(ctx->state, ctx->block);
			ctx->used = 0;
		}
	}
}

static void _sha256_soft_final(mp_sha256_t *ctx, uint8_t *digest) {
	uint64_t bits = ctx->length * 8;
	int i;

	ctx->block[ctx->used++] = 0x80;
	if(ctx->used > MP_SHA256_BLOCK - 8) {
		memset(ctx->block + ctx->used, 0, MP_SHA256_BLOCK - ctx->used);
		_sha256_block(ctx->state, ctx->block);
		ctx->used = 0;
	}
	memset(ctx->block + ctx->used, 0, MP_SHA256_BLOCK - 8 - ctx->used);
	for(i = 0; i < 8; i++)
		ctx->block[MP_SHA256_BLOCK - 8 + i] = (uint8_t)(bits >> (56 - i * 8));
	_sha256_block(ctx->state, ctx->block);

	for(i = 0; i < 8; i++)
		_sha256_store(digest + i * 4, ctx->state[i]);
}

/**
 * @brief Start a digest
 *
 * @param[in] ctx Context, owned by the caller
 */
void mp_sha256_init(mp_sha256_t *ctx) {
	memcpy(ctx->state, _sha256_iv, sizeof(ctx->state));
	ctx->length = 0;
	ctx->used = 0;
#ifdef MP_ARCH_SHA256
	ctx->onHardware = mp_arch_sha256_init(&ctx->hw);
#endif
}

/**
 * @brief Hash more bytes
 *
 * @param[in] ctx Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_sha256_update(mp_sha256_t *ctx, const void *data, unsigned int size) {
#ifdef MP_ARCH_SHA256
	if(ctx->onHardware) {
		mp_arch_sha256_update(&ctx->hw, data, size);
		return;
	}
#endif
	_sha256_soft_update(ctx, data, size);
}

/**
 * @brief End a digest
 *
 * The context is wiped, start again with mp_sha256_init().
 *
 * @param[in] ctx Context
 * @param[out] digest MP_SHA256_SIZE bytes
 */
void mp_sha256_final(mp_sha256_t *ctx, uint8_t digest[MP_SHA256_SIZE]) {
#ifdef MP_ARCH_SHA256
	if(ctx->onHardware)
		mp_arch_sha256_final(&ctx->hw, digest);
	else
		_sha256_soft_final(ctx, digest);
#else
	_sha256_soft_final(ctx, digest);
#endif
	memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief Digest a buffer in one call
 *
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 * @param[out] digest MP_SHA256_SIZE bytes
 */
void mp_sha256(const void *data, unsigned int size, uint8_t digest[MP_SHA256_SIZE]) {
	mp_sha256_t ctx;

	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, data, size);
	mp_sha256_final(&ctx, digest);
}

/**@}*/

#endif
