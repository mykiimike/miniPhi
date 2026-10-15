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

#ifdef SUPPORT_COMMON_CRYPTO_MAC_POLY1305

#include "poly1305.h"
#include <string.h>

/**
@defgroup mpCommonPoly1305 Poly1305

@ingroup mpCommon

@brief Poly1305 one-time MAC (RFC 7539)

A 130-bit prime field MAC. The 32-byte key is r (clamped) || s and is
one-time: a fresh key per message, never reused. The accumulator is kept
in five 26-bit limbs (the poly1305-donna layout) and the multiply uses
64-bit products, so every step is plain arithmetic with no table: the
MAC is constant time by construction. The only secret is r (s is added
at the end and never multiplies anything); r drives no branch and no
index.

@{
*/

static uint32_t _le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void _le32_put(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* absorb one 16-byte block; hibit is (1<<24) for a full block, 0 for a
 * padded final partial block (the 0x01 pad byte is already in m) */
static void _poly1305_block(mp_poly1305_t *ctx, const uint8_t *m, uint32_t hibit) {
	uint32_t t0 = _le32(m + 0);
	uint32_t t1 = _le32(m + 4);
	uint32_t t2 = _le32(m + 8);
	uint32_t t3 = _le32(m + 12);
	uint64_t d0, d1, d2, d3, d4;
	uint32_t h0, h1, h2, h3, h4;
	uint32_t r0, r1, r2, r3, r4;
	uint32_t s1, s2, s3, s4;
	uint32_t c;

	ctx->h[0] += t0 & 0x3ffffff;
	ctx->h[1] += ((t0 >> 26) | (t1 << 6)) & 0x3ffffff;
	ctx->h[2] += ((t1 >> 20) | (t2 << 12)) & 0x3ffffff;
	ctx->h[3] += ((t2 >> 14) | (t3 << 18)) & 0x3ffffff;
	ctx->h[4] += (t3 >> 8) | hibit;

	h0 = ctx->h[0]; h1 = ctx->h[1]; h2 = ctx->h[2]; h3 = ctx->h[3]; h4 = ctx->h[4];
	r0 = ctx->r[0]; r1 = ctx->r[1]; r2 = ctx->r[2]; r3 = ctx->r[3]; r4 = ctx->r[4];
	s1 = r1 * 5; s2 = r2 * 5; s3 = r3 * 5; s4 = r4 * 5;

	d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
	d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
	d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
	d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
	d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

	c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
	d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
	d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
	d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
	d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
	h0 += c * 5; c = h0 >> 26; h0 = h0 & 0x3ffffff;
	h1 += c;

	ctx->h[0] = h0; ctx->h[1] = h1; ctx->h[2] = h2; ctx->h[3] = h3; ctx->h[4] = h4;
}

/**
 * @brief Start a Poly1305 MAC
 *
 * @param[in] ctx Uninitialised context
 * @param[in] key 32 bytes (r || s), one-time
 * @param[in] keylen Must be 32
 * @return TRUE when the key length is 32, FALSE otherwise
 */
mp_ret_t mp_poly1305_init(mp_poly1305_t *ctx, const void *key, unsigned int keylen) {
	if(!ctx || !key)
		return(FALSE);
	if(keylen != MP_POLY1305_KEY)
		return(FALSE);
	const uint8_t *k = (const uint8_t *)key;
	uint32_t t0 = _le32(k + 0);
	uint32_t t1 = _le32(k + 4);
	uint32_t t2 = _le32(k + 8);
	uint32_t t3 = _le32(k + 12);

	memset(ctx, 0, sizeof(*ctx));
	/* r is clamped: clear bits 4..0 of bytes 3,7,11,15 and bits 7..2 of bytes 4,8,12 */
	ctx->r[0] = t0 & 0x3ffffff;
	ctx->r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
	ctx->r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
	ctx->r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
	ctx->r[4] = (t3 >> 8) & 0x00fffff;
	ctx->s[0] = _le32(k + 16);
	ctx->s[1] = _le32(k + 20);
	ctx->s[2] = _le32(k + 24);
	ctx->s[3] = _le32(k + 28);
	return(TRUE);
}

/**
 * @brief Absorb bytes
 *
 * @param[in] ctx Started context
 * @param[in] in Any length
 * @param[in] inlen Bytes to take from in
 */
void mp_poly1305_update(mp_poly1305_t *ctx, const void *in, unsigned int inlen) {
	if(!ctx || !in || !inlen)
		return;
	const uint8_t *p = (const uint8_t *)in;
	unsigned int n = inlen;

	/* drain the buffer first */
	if(ctx->bufLeft) {
		unsigned int want = MP_POLY1305_BLOCK - ctx->bufLeft;
		unsigned int take = want < n ? want : n;
		memcpy(ctx->buf + ctx->bufLeft, p, take);
		ctx->bufLeft += take;
		p += take; n -= take;
		if(ctx->bufLeft == MP_POLY1305_BLOCK) {
			_poly1305_block(ctx, ctx->buf, (1 << 24));
			ctx->bufLeft = 0;
		}
	}
	/* full blocks straight from the input */
	while(n >= MP_POLY1305_BLOCK) {
		_poly1305_block(ctx, p, (1 << 24));
		p += MP_POLY1305_BLOCK;
		n -= MP_POLY1305_BLOCK;
	}
	/* buffer the rest */
	if(n) {
		memcpy(ctx->buf, p, n);
		ctx->bufLeft = n;
	}
}

/**
 * @brief End the MAC
 *
 * @param[in] ctx Started context
 * @param[out] tag 16 bytes
 * @return 16, 0 when not started; wipes the context
 */
unsigned int mp_poly1305_final(mp_poly1305_t *ctx, uint8_t *tag) {
	if(!ctx || !tag)
		return(0);
	uint32_t h0, h1, h2, h3, h4, g0, g1, g2, g3, g4;
	uint32_t c, mask;

	/* final partial block: pad with 0x01 then zeros, no hibit */
	if(ctx->bufLeft) {
		ctx->buf[ctx->bufLeft++] = 1;
		while(ctx->bufLeft < MP_POLY1305_BLOCK)
			ctx->buf[ctx->bufLeft++] = 0;
		_poly1305_block(ctx, ctx->buf, 0);
	}

	h0 = ctx->h[0]; h1 = ctx->h[1]; h2 = ctx->h[2]; h3 = ctx->h[3]; h4 = ctx->h[4];

	/* full carry */
	c = h1 >> 26; h1 = h1 & 0x3ffffff;
	h2 += c; c = h2 >> 26; h2 = h2 & 0x3ffffff;
	h3 += c; c = h3 >> 26; h3 = h3 & 0x3ffffff;
	h4 += c; c = h4 >> 26; h4 = h4 & 0x3ffffff;
	h0 += c * 5; c = h0 >> 26; h0 = h0 & 0x3ffffff;
	h1 += c;

	/* second carry */
	c = h1 >> 26; h1 = h1 & 0x3ffffff;
	h2 += c; c = h2 >> 26; h2 = h2 & 0x3ffffff;
	h3 += c; c = h3 >> 26; h3 = h3 & 0x3ffffff;
	h4 += c; c = h4 >> 26; h4 = h4 & 0x3ffffff;
	h0 += c * 5; c = h0 >> 26; h0 = h0 & 0x3ffffff;
	h1 += c;

	/* compute h + -p (g = h - p); if h >= p the top bit of g4 is set */
	g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
	g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
	g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
	g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
	g4 = h4 + c - (1 << 26);

	/* select h when h < p (g4 negative -> mask all ones), else g */
	mask = (g4 >> 31) - 1;
	g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
	mask = ~mask;
	h0 = (h0 & mask) | g0;
	h1 = (h1 & mask) | g1;
	h2 = (h2 & mask) | g2;
	h3 = (h3 & mask) | g3;
	h4 = (h4 & mask) | g4;

	/* repack to four 32-bit words (drop the top 2 bits: mod 2^128) */
	h0 = (h0) | (h1 << 26);
	h1 = (h1 >> 6) | (h2 << 20);
	h2 = (h2 >> 12) | (h3 << 14);
	h3 = (h3 >> 18) | (h4 << 8);

	/* h += s */
	uint64_t f = (uint64_t)h0 + ctx->s[0]; h0 = (uint32_t)f;
	f = (uint64_t)h1 + ctx->s[1] + (f >> 32); h1 = (uint32_t)f;
	f = (uint64_t)h2 + ctx->s[2] + (f >> 32); h2 = (uint32_t)f;
	f = (uint64_t)h3 + ctx->s[3] + (f >> 32); h3 = (uint32_t)f;

	_le32_put(tag + 0, h0);
	_le32_put(tag + 4, h1);
	_le32_put(tag + 8, h2);
	_le32_put(tag + 12, h3);

	mp_poly1305_free(ctx);
	return(MP_POLY1305_SIZE);
}

/**
 * @brief Drop a Poly1305 context
 *
 * @param[in] ctx Started context (or NULL)
 */
void mp_poly1305_free(mp_poly1305_t *ctx) {
	if(!ctx)
		return;
	memset(ctx, 0, sizeof(*ctx));
}

/**@}*/

#endif


