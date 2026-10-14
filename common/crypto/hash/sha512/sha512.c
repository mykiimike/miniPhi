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

#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512

/**
@defgroup mpCommonSha512 SHA-512

@ingroup mpCommon

@brief SHA-512 digest (FIPS 180-4)

The caller owns the context. SHA-384 starts the same compression with
mp_sha512_init_iv() and a 48-byte result.

The schedule is a ring of 16 words, so the stack stays small on MSP430.

@{
*/

#define ROR(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

static const uint64_t _sha512_k[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

static const uint64_t _sha512_iv[8] = {
	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};

static uint64_t _sha512_load(const uint8_t *p) {
	return(((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
		((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | (uint64_t)p[7]);
}

static void _sha512_store(uint8_t *p, uint64_t value) {
	p[0] = (uint8_t)(value >> 56);
	p[1] = (uint8_t)(value >> 48);
	p[2] = (uint8_t)(value >> 40);
	p[3] = (uint8_t)(value >> 32);
	p[4] = (uint8_t)(value >> 24);
	p[5] = (uint8_t)(value >> 16);
	p[6] = (uint8_t)(value >> 8);
	p[7] = (uint8_t)value;
}

void mp_sha512_block(uint64_t state[8], const uint8_t block[MP_SHA512_BLOCK]) {
	uint64_t w[16];
	uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
	uint64_t e = state[4], f = state[5], g = state[6], h = state[7];
	uint64_t t1, t2, s0, s1;
	int i;

	for(i = 0; i < 80; i++) {
		if(i < 16)
			w[i] = _sha512_load(block + i * 8);
		else {
			s0 = w[(i + 1) & 15];
			s1 = w[(i + 14) & 15];
			s0 = ROR(s0, 1) ^ ROR(s0, 8) ^ (s0 >> 7);
			s1 = ROR(s1, 19) ^ ROR(s1, 61) ^ (s1 >> 6);
			w[i & 15] += s0 + s1 + w[(i + 9) & 15];
		}
		t1 = h + (ROR(e, 14) ^ ROR(e, 18) ^ ROR(e, 41)) + ((e & f) ^ (~e & g)) + _sha512_k[i] + w[i & 15];
		t2 = (ROR(a, 28) ^ ROR(a, 34) ^ ROR(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
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

static void _sha512_soft_update(mp_sha512_t *ctx, const uint8_t *data, unsigned int size) {
	unsigned int take;

	ctx->length += size;
	while(size > 0) {
		take = MP_SHA512_BLOCK - ctx->used;
		if(take > size)
			take = size;
		memcpy(ctx->block + ctx->used, data, take);
		ctx->used += take;
		data += take;
		size -= take;
		if(ctx->used == MP_SHA512_BLOCK) {
			mp_sha512_block(ctx->state, ctx->block);
			ctx->used = 0;
		}
	}
}

static void _sha512_soft_final(mp_sha512_t *ctx, uint8_t *digest) {
	uint64_t bits = ctx->length * 8;
	unsigned int words, i;

	ctx->block[ctx->used++] = 0x80;
	if(ctx->used > MP_SHA512_BLOCK - 16) {
		memset(ctx->block + ctx->used, 0, MP_SHA512_BLOCK - ctx->used);
		mp_sha512_block(ctx->state, ctx->block);
		ctx->used = 0;
	}
	memset(ctx->block + ctx->used, 0, MP_SHA512_BLOCK - ctx->used);
	for(i = 0; i < 8; i++)
		ctx->block[MP_SHA512_BLOCK - 8 + i] = (uint8_t)(bits >> (56 - i * 8));
	mp_sha512_block(ctx->state, ctx->block);

	words = ctx->outlen / 8;
	if(words > 8)
		words = 8;
	for(i = 0; i < words; i++)
		_sha512_store(digest + i * 8, ctx->state[i]);
}

/**
 * @brief Start a software digest with a chosen initial value
 *
 * SHA-384 uses this. The accelerator is left off.
 *
 * @param[in] ctx Context, owned by the caller
 * @param[in] iv Eight words
 * @param[in] outlen Bytes final will write, 64 or 48
 */
void mp_sha512_init_iv(mp_sha512_t *ctx, const uint64_t iv[8], unsigned int outlen) {
	memcpy(ctx->state, iv, sizeof(ctx->state));
	ctx->length = 0;
	ctx->used = 0;
	ctx->outlen = outlen;
#ifdef MP_ARCH_SHA512
	ctx->onHardware = NO;
#endif
}

/**
 * @brief Start a digest
 *
 * @param[in] ctx Context, owned by the caller
 */
void mp_sha512_init(mp_sha512_t *ctx) {
	mp_sha512_init_iv(ctx, _sha512_iv, MP_SHA512_SIZE);
#ifdef MP_ARCH_SHA512
	ctx->onHardware = mp_arch_sha512_init(&ctx->hw);
#endif
}

/**
 * @brief Hash more bytes
 *
 * @param[in] ctx Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_sha512_update(mp_sha512_t *ctx, const void *data, unsigned int size) {
#ifdef MP_ARCH_SHA512
	if(ctx->onHardware) {
		mp_arch_sha512_update(&ctx->hw, data, size);
		return;
	}
#endif
	_sha512_soft_update(ctx, data, size);
}

/**
 * @brief End a digest
 *
 * Writes ctx->outlen bytes. The context is wiped.
 *
 * @param[in] ctx Context
 * @param[out] digest ctx->outlen bytes
 */
void mp_sha512_final(mp_sha512_t *ctx, uint8_t *digest) {
#ifdef MP_ARCH_SHA512
	if(ctx->onHardware)
		mp_arch_sha512_final(&ctx->hw, digest);
	else
		_sha512_soft_final(ctx, digest);
#else
	_sha512_soft_final(ctx, digest);
#endif
	memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief Digest a buffer in one call
 *
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 * @param[out] digest MP_SHA512_SIZE bytes
 */
void mp_sha512(const void *data, unsigned int size, uint8_t digest[MP_SHA512_SIZE]) {
	mp_sha512_t ctx;

	mp_sha512_init(&ctx);
	mp_sha512_update(&ctx, data, size);
	mp_sha512_final(&ctx, digest);
}

/**@}*/

#endif
