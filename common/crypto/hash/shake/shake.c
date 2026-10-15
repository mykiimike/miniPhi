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

#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHAKE

#include "shake.h"
#include <string.h>

/**
@defgroup mpCommonShake SHA-3 / SHAKE

@ingroup mpCommon

@brief Keccak-f[1600] sponge: SHA3-256, SHA3-512, SHAKE-128, SHAKE-256

One core permutation serves the four modes; only the rate (in bytes) and
the domain suffix differ. SHA3-256 and SHA3-512 are fixed-size digests,
SHAKE-128 and SHAKE-256 are extendable output functions the caller squeezes
to any length. ML-DSA (FIPS 204) needs all four.

Constant time with respect to any secret: the Keccak permutation is a fixed
function of the full 1600-bit state, with no data-dependent branch or
memory address. The absorb and squeeze loops depend only on lengths, which
are public. One operation at a time.

@{
*/

/* round constants, 24 rounds of Keccak-f[1600]. */
static const uint64_t _rc[24] = {
	0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
	0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
	0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
	0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
	0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
	0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

/* rotation offsets indexed by lane x + 5*y. */
static const uint8_t _rot[25] = {
	 0,  1, 62, 28, 27,
	36, 44,  6, 55, 20,
	 3, 10, 43, 25, 39,
	41, 45, 15, 21,  8,
	18,  2, 61, 56, 14,
};

#define _ROL(x, n) (((x) << (n)) | ((x) >> (64 - (n))))

static void _keccak_f1600(uint64_t *a) {
	unsigned int r, x, y;
	uint64_t c[5], d[5], t[25], p[5];
	for(r = 0; r < 24; r++) {
		/* theta */
		for(x = 0; x < 5; x++)
			c[x] = a[x] ^ a[x+5] ^ a[x+10] ^ a[x+15] ^ a[x+20];
		for(x = 0; x < 5; x++)
			d[x] = c[(x+4) % 5] ^ _ROL(c[(x+1) % 5], 1);
		for(x = 0; x < 5; x++)
			for(y = 0; y < 5; y++)
				a[x+5*y] ^= d[x];
		/* rho + pi */
		for(x = 0; x < 5; x++)
			for(y = 0; y < 5; y++)
				t[y + 5*((2*x+3*y) % 5)] = _ROL(a[x+5*y], _rot[x+5*y]);
		for(x = 0; x < 5; x++)
			for(y = 0; y < 5; y++)
				a[x+5*y] = t[x+5*y];
		/* chi */
		for(y = 0; y < 5; y++) {
			for(x = 0; x < 5; x++) p[x] = a[x+5*y];
			for(x = 0; x < 5; x++)
				a[x+5*y] = p[x] ^ ((~p[(x+1) % 5]) & p[(x+2) % 5]);
		}
		/* iota */
		a[0] ^= _rc[r];
	}
}

static void _keccak_absorb_block(mp_keccak_t *k) {
	unsigned int i;
	uint64_t *s = k->a;
	for(i = 0; i < k->rate / 8; i++) {
		uint64_t lane = 0;
		unsigned int b;
		for(b = 0; b < 8; b++)
			lane |= (uint64_t)k->buf[i*8 + b] << (8*b);
		s[i] ^= lane;
	}
	_keccak_f1600(s);
	k->idx = 0;
}

void mp_keccak_init(mp_keccak_t *k, unsigned int rate, uint8_t suffix) {
	memset(k, 0, sizeof(*k));
	k->rate = rate;
	k->suffix = suffix;
	k->idx = 0;
	k->squeezing = 0;
}

void mp_keccak_absorb(mp_keccak_t *k, const void *data, unsigned int len) {
	const uint8_t *p = data;
	while(len) {
		unsigned int n = k->rate - k->idx;
		if(n > len) n = len;
		memcpy(k->buf + k->idx, p, n);
		k->idx += n;
		p += n;
		len -= n;
		if(k->idx == k->rate)
			_keccak_absorb_block(k);
	}
}

void mp_keccak_pad(mp_keccak_t *k) {
	/* pad10*1: set the suffix, then 1, fill with zeros, set the last bit. */
	memset(k->buf + k->idx, 0, k->rate - k->idx);
	k->buf[k->idx] = k->suffix;
	k->buf[k->rate - 1] |= 0x80;
	_keccak_absorb_block(k);
	k->squeezing = 1;
	k->firstperm = 1;   /* pad already permuted; first squeeze must not re-permute */
	k->idx = k->rate;   /* state holds fresh squeezed bytes, none drained yet */
}

void mp_keccak_squeeze(mp_keccak_t *k, uint8_t *out, unsigned int len) {
	uint8_t *o = out;
	while(len) {
		if(k->idx >= k->rate) {
			if(k->firstperm) k->firstperm = 0;
			else _keccak_f1600(k->a);
			k->idx = 0;
		}
		unsigned int n = k->rate - k->idx;
		if(n > len) n = len;
		unsigned int i;
		for(i = 0; i < n; i++) {
			uint64_t lane = k->a[(k->idx + i) / 8];
			o[i] = (uint8_t)(lane >> (8 * ((k->idx + i) % 8)));
		}
		o += n;
		k->idx += n;
		len -= n;
	}
}

/* ---- SHA3-256 ---------------------------------------------------------- */

void mp_sha3_256_init(mp_sha3_256_t *c) { mp_keccak_init(c, 136, 0x06); }
void mp_sha3_256_update(mp_sha3_256_t *c, const void *data, unsigned int len) { mp_keccak_absorb(c, data, len); }
void mp_sha3_256_final(mp_sha3_256_t *c, uint8_t *digest) {
	mp_keccak_pad(c);
	memcpy(digest, c->a, 32);
}
void mp_sha3_256(const void *in, unsigned int len, uint8_t *out) {
	mp_sha3_256_t c;
	mp_sha3_256_init(&c);
	mp_sha3_256_update(&c, in, len);
	mp_sha3_256_final(&c, out);
}

/* ---- SHA3-512 ---------------------------------------------------------- */

void mp_sha3_512_init(mp_sha3_512_t *c) { mp_keccak_init(c, 72, 0x06); }
void mp_sha3_512_update(mp_sha3_512_t *c, const void *data, unsigned int len) { mp_keccak_absorb(c, data, len); }
void mp_sha3_512_final(mp_sha3_512_t *c, uint8_t *digest) {
	mp_keccak_pad(c);
	memcpy(digest, c->a, 64);
}
void mp_sha3_512(const void *in, unsigned int len, uint8_t *out) {
	mp_sha3_512_t c;
	mp_sha3_512_init(&c);
	mp_sha3_512_update(&c, in, len);
	mp_sha3_512_final(&c, out);
}

/* ---- SHAKE-128 --------------------------------------------------------- */

void mp_shake128_init(mp_shake128_t *c) { mp_keccak_init(c, 168, 0x1f); }
void mp_shake128_absorb(mp_shake128_t *c, const void *data, unsigned int len) { mp_keccak_absorb(c, data, len); }
void mp_shake128_finalize(mp_shake128_t *c) { mp_keccak_pad(c); }
void mp_shake128_squeeze(mp_shake128_t *c, uint8_t *out, unsigned int len) { mp_keccak_squeeze(c, out, len); }
void mp_shake128(uint8_t *out, unsigned int outlen, const void *in, unsigned int inlen) {
	mp_shake128_t c;
	mp_shake128_init(&c);
	mp_shake128_absorb(&c, in, inlen);
	mp_shake128_finalize(&c);
	mp_shake128_squeeze(&c, out, outlen);
}

/* ---- SHAKE-256 --------------------------------------------------------- */

void mp_shake256_init(mp_shake256_t *c) { mp_keccak_init(c, 136, 0x1f); }
void mp_shake256_absorb(mp_shake256_t *c, const void *data, unsigned int len) { mp_keccak_absorb(c, data, len); }
void mp_shake256_finalize(mp_shake256_t *c) { mp_keccak_pad(c); }
void mp_shake256_squeeze(mp_shake256_t *c, uint8_t *out, unsigned int len) { mp_keccak_squeeze(c, out, len); }
void mp_shake256(uint8_t *out, unsigned int outlen, const void *in, unsigned int inlen) {
	mp_shake256_t c;
	mp_shake256_init(&c);
	mp_shake256_absorb(&c, in, inlen);
	mp_shake256_finalize(&c);
	mp_shake256_squeeze(&c, out, outlen);
}

/**@}*/

#endif /* SUPPORT_COMMON_CRYPTO_HASH_SHAKE */
