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

#ifdef SUPPORT_COMMON_CRYPTO_KEM_X448

/**
@defgroup mpCommonX448 X448 KEM

@ingroup mpCommon

@brief DHKEM(X448, HKDF-SHA512), RFC 9180 and RFC 7748

The ladder always runs from bit 447 down to bit 0 and swaps through a mask.
The swap bit is hidden from the compiler: a visible 0 or 1 becomes a branch.
The field is 2^448 - 2^224 - 1, fourteen little-endian words. A product is
folded four times with 2^448 = 2^224 + 1, which is enough for two full
448-bit factors, then one subtract of p. That bit is hidden the same way.
The shared secret is mp_crypto_dhkem() with SHA-512 and suite 0x0021.
RFC 9180 requires the Diffie-Hellman output to be rejected when it is
all zeros, which is the low-order case. Both ladders still run, and
the check is the OR of the 56 bytes.
One ladder runs at a time.

@{
*/

#define _A24 39081u

typedef struct {
	uint32_t v[14];
} _fe;

/** p = 2^448 - 2^224 - 1. Limb 7 lost the low bit of 2^224. */
static const uint32_t _p[14] = {
	0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU,
	0xffffffffU, 0xffffffffU, 0xffffffffU, 0xfffffffeU,
	0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU,
	0xffffffffU, 0xffffffffU
};

/** p-2, little-endian, for the inversion chain */
static const uint8_t _pm2[56] = {
	0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static const uint8_t _base[56] = { 5 };

static const uint8_t _suite[5] = { 'K', 'E', 'M', 0x00, 0x21 };

static void _copy(_fe *o, const _fe *a) {
	int i;

	for(i = 0; i < 14; i++)
		o->v[i] = a->v[i];
}

static void _zero(_fe *o) {
	int i;

	for(i = 0; i < 14; i++)
		o->v[i] = 0;
}

static void _set1(_fe *o) {
	_zero(o);
	o->v[0] = 1;
}

/** A 0/1 the compiler must not inspect. GCC turns a known mask into a branch. */
static uint32_t _opaque(uint32_t x) {
	__asm__ volatile("" : "+r"(x));
	return(x);
}

static void _cmov(_fe *dst, const _fe *src, uint32_t bit) {
	uint32_t mask = 0u - _opaque(bit);
	int i;

	for(i = 0; i < 14; i++)
		dst->v[i] ^= mask & (dst->v[i] ^ src->v[i]);
}

static void _cswap(_fe *a, _fe *b, uint32_t bit) {
	uint32_t mask = 0u - _opaque(bit);
	int i;

	for(i = 0; i < 14; i++) {
		uint32_t t = mask & (a->v[i] ^ b->v[i]);

		a->v[i] ^= t;
		b->v[i] ^= t;
	}
}

/**
 * o = value mod p. lo and hi are fourteen words: value = lo + hi * 2^448.
 * Four folds clear a product of two 448-bit numbers. The compare with p
 * is a carry, then a mask.
 */
static void _canon(_fe *o) {
	uint32_t t[14];
	uint32_t br = 0;
	uint32_t bit, mask;
	uint64_t c;
	int i;

	c = (uint64_t)o->v[0] + 1u;
	c >>= 32;
	for(i = 1; i < 7; i++)
		c = (c + o->v[i]) >> 32;
	c = (c + o->v[7] + 1u) >> 32;
	for(i = 8; i < 14; i++)
		c = (c + o->v[i]) >> 32;
	bit = (uint32_t)c;
	for(i = 0; i < 14; i++) {
		uint64_t s = (uint64_t)o->v[i] - _p[i] - br;

		t[i] = (uint32_t)s;
		br = (uint32_t)((s >> 32) & 1u);
	}
	mask = 0u - _opaque(bit);
	for(i = 0; i < 14; i++)
		o->v[i] ^= mask & (o->v[i] ^ t[i]);
}

static void _reduce(_fe *o, const uint32_t lo[14], const uint32_t hi[14]) {
	uint32_t cur[14], high[14];
	int round, i;

	for(i = 0; i < 14; i++) {
		cur[i] = lo[i];
		high[i] = hi[i];
	}
	for(round = 0; round < 4; round++) {
		uint64_t acc[28];
		uint64_t c = 0;

		for(i = 0; i < 28; i++)
			acc[i] = 0;
		for(i = 0; i < 14; i++) {
			acc[i] += (uint64_t)cur[i] + high[i];
			acc[i + 7] += high[i];
		}
		for(i = 0; i < 28; i++) {
			c += acc[i];
			if(i < 14)
				cur[i] = (uint32_t)c;
			else
				high[i - 14] = (uint32_t)c;
			c >>= 32;
		}
	}
	for(i = 0; i < 14; i++)
		o->v[i] = cur[i];
	_canon(o);
}

static void _add(_fe *o, const _fe *a, const _fe *b) {
	uint32_t lo[14], hi[14];
	uint64_t c = 0;
	int i;

	for(i = 0; i < 14; i++) {
		c += (uint64_t)a->v[i] + b->v[i];
		lo[i] = (uint32_t)c;
		c >>= 32;
	}
	for(i = 0; i < 14; i++)
		hi[i] = 0;
	hi[0] = (uint32_t)c;
	_reduce(o, lo, hi);
}

/**
 * o = a - b. Both are in 0..p-1, so the result stays there.
 * A borrow subtracts 2^224 + 1, which is p's complement under 2^448.
 */
static void _sub(_fe *o, const _fe *a, const _fe *b) {
	uint32_t br = 0, br2 = 0;
	int i;

	for(i = 0; i < 14; i++) {
		uint64_t s = (uint64_t)a->v[i] - b->v[i] - br;

		o->v[i] = (uint32_t)s;
		br = (uint32_t)((s >> 32) & 1u);
	}
	for(i = 0; i < 14; i++) {
		uint32_t dec = 0;
		uint64_t s;

		if(i == 0 || i == 7)
			dec = br;
		s = (uint64_t)o->v[i] - dec - br2;
		o->v[i] = (uint32_t)s;
		br2 = (uint32_t)((s >> 32) & 1u);
	}
	(void)br2;
	_canon(o);
}

static void _mul(_fe *o, const _fe *a, const _fe *b) {
	uint32_t t[28];
	uint64_t c;
	int i, j;

	for(i = 0; i < 28; i++)
		t[i] = 0;
	for(i = 0; i < 14; i++) {
		c = 0;
		for(j = 0; j < 14; j++) {
			c += (uint64_t)t[i + j] + (uint64_t)a->v[i] * b->v[j];
			t[i + j] = (uint32_t)c;
			c >>= 32;
		}
		t[i + 14] = (uint32_t)c;
	}
	_reduce(o, t, t + 14);
}

static void _sqr(_fe *o, const _fe *a) {
	_fe t;

	_copy(&t, a);
	_mul(o, &t, &t);
}

static void _mul_small(_fe *o, const _fe *a, uint32_t s) {
	uint32_t lo[14], hi[14];
	uint64_t c = 0;
	int i;

	for(i = 0; i < 14; i++) {
		c += (uint64_t)a->v[i] * s;
		lo[i] = (uint32_t)c;
		c >>= 32;
	}
	for(i = 0; i < 14; i++)
		hi[i] = 0;
	hi[0] = (uint32_t)c;
	c >>= 32;
	hi[1] = (uint32_t)c;
	_reduce(o, lo, hi);
}

static void _load(_fe *o, const uint8_t in[56]) {
	int i;

	for(i = 0; i < 14; i++) {
		o->v[i] = (uint32_t)in[i * 4]
			| ((uint32_t)in[i * 4 + 1] << 8)
			| ((uint32_t)in[i * 4 + 2] << 16)
			| ((uint32_t)in[i * 4 + 3] << 24);
	}
	_canon(o);
}

static void _store(uint8_t out[56], const _fe *a) {
	_fe t;
	int i;

	_copy(&t, a);
	_canon(&t);
	for(i = 0; i < 14; i++) {
		out[i * 4] = (uint8_t)t.v[i];
		out[i * 4 + 1] = (uint8_t)(t.v[i] >> 8);
		out[i * 4 + 2] = (uint8_t)(t.v[i] >> 16);
		out[i * 4 + 3] = (uint8_t)(t.v[i] >> 24);
	}
}

/** OR of every byte. Zero means the Diffie-Hellman output is the all-zero string. */
static uint32_t _or_bytes(const uint8_t *p, unsigned int n) {
	uint32_t acc = 0;
	unsigned int i;

	for(i = 0; i < n; i++)
		acc |= p[i];
	return(acc);
}

static void _inv(_fe *o, const _fe *a) {
	_fe r, s;
	int i;

	_set1(&r);
	for(i = 447; i >= 0; i--) {
		uint32_t bit = (_pm2[i >> 3] >> (i & 7)) & 1u;

		_sqr(&r, &r);
		_mul(&s, &r, a);
		_cmov(&r, &s, bit);
	}
	_copy(o, &r);
}

/**
 * @brief X448
 * @param[out] out 56-byte u-coordinate
 * @param[in] scalar 56-byte scalar, clamped here
 * @param[in] point 56-byte u-coordinate
 */
void mp_crypto_x448(uint8_t out[56], const uint8_t scalar[56], const uint8_t point[56]) {
	uint8_t k[56];
	_fe x1, x2, z2, x3, z3;
	_fe A, AA, B, BB, E, C, D, DA, CB, T;
	uint32_t swap = 0;
	int t;

	memcpy(k, scalar, 56);
	k[0] &= 252;
	k[55] |= 128;
	_load(&x1, point);
	_set1(&x2);
	_zero(&z2);
	_copy(&x3, &x1);
	_set1(&z3);
	for(t = 447; t >= 0; t--) {
		uint32_t kt = (k[t >> 3] >> (t & 7)) & 1u;

		swap ^= kt;
		_cswap(&x2, &x3, swap);
		_cswap(&z2, &z3, swap);
		swap = kt;
		_add(&A, &x2, &z2);
		_sqr(&AA, &A);
		_sub(&B, &x2, &z2);
		_sqr(&BB, &B);
		_sub(&E, &AA, &BB);
		_add(&C, &x3, &z3);
		_sub(&D, &x3, &z3);
		_mul(&DA, &D, &A);
		_mul(&CB, &C, &B);
		_add(&T, &DA, &CB);
		_sqr(&x3, &T);
		_sub(&T, &DA, &CB);
		_sqr(&T, &T);
		_mul(&z3, &x1, &T);
		_mul(&x2, &AA, &BB);
		_mul_small(&T, &E, _A24);
		_add(&T, &AA, &T);
		_mul(&z2, &E, &T);
	}
	_cswap(&x2, &x3, swap);
	_cswap(&z2, &z3, swap);
	_inv(&T, &z2);
	_mul(&T, &x2, &T);
	_store(out, &T);
	memset(k, 0, sizeof(k));
}

/**
 * @brief Build an X448 key pair
 * @param[in] seed 56-byte private key
 * @param[in] seedlen Length of seed
 * @param[out] priv The same 56 bytes
 * @param[out] pub The u-coordinate of the base point
 * @return FALSE when the seed is shorter than 56 bytes
 */
mp_ret_t mp_crypto_x448_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	if(!seed || !priv || !pub || seedlen < 56)
		return(FALSE);
	memcpy(priv, seed, 56);
	mp_crypto_x448(pub, priv, _base);
	return(TRUE);
}

/**
 * @brief Encapsulate with DHKEM(X448, HKDF-SHA512)
 * @param[in] pub Recipient public key, 56 bytes
 * @param[in] coins Ephemeral secret, 56 bytes
 * @param[in] coinslen Length of coins
 * @param[out] ss 64-byte shared secret
 * @param[out] ct Ephemeral public key, 56 bytes
 * @return 56, or 0 when the input is short or the Diffie-Hellman output is all zeros
 */
unsigned int mp_crypto_x448_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct) {
	uint8_t dh[56], ctx[112];
	unsigned int n;

	if(!pub || !coins || !ss || !ct || coinslen < 56)
		return(0);
	mp_crypto_x448(ct, coins, _base);
	mp_crypto_x448(dh, coins, pub);
	if(_or_bytes(dh, 56) == 0) {
		memset(dh, 0, sizeof(dh));
		memset(ct, 0, 56);
		return(0);
	}
	memcpy(ctx, ct, 56);
	memcpy(ctx + 56, pub, 56);
	n = mp_crypto_dhkem(ss, 64, "SHA512", _suite, dh, 56, ctx, 112);
	memset(dh, 0, sizeof(dh));
	memset(ctx, 0, sizeof(ctx));
	return(n == 64 ? 56 : 0);
}

/**
 * @brief Decapsulate with DHKEM(X448, HKDF-SHA512)
 * @param[in] priv Recipient private key, 56 bytes
 * @param[in] ct Ephemeral public key
 * @param[in] ctlen Length of ct
 * @param[out] ss 64-byte shared secret
 * @return YES when ctlen is 56 and the Diffie-Hellman output is not all zeros
 */
mp_bool_t mp_crypto_x448_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss) {
	uint8_t dh[56], pk[56], ctx[112];
	unsigned int n;

	if(!priv || !ct || !ss || ctlen != 56)
		return(NO);
	mp_crypto_x448(pk, priv, _base);
	mp_crypto_x448(dh, priv, ct);
	if(_or_bytes(dh, 56) == 0) {
		memset(dh, 0, sizeof(dh));
		memset(pk, 0, sizeof(pk));
		return(NO);
	}
	memcpy(ctx, ct, 56);
	memcpy(ctx + 56, pk, 56);
	n = mp_crypto_dhkem(ss, 64, "SHA512", _suite, dh, 56, ctx, 112);
	memset(dh, 0, sizeof(dh));
	memset(pk, 0, sizeof(pk));
	memset(ctx, 0, sizeof(ctx));
	return(n == 64 ? YES : NO);
}

/**
 * @brief Decapsulate with a fuse slot
 * @param[in] fuse Slot of role MP_CRYPTO_FUSE_X448
 * @param[in] wrapped Blob the arch defines
 * @param[in] wrappedlen Byte length of wrapped
 * @param[in] ct Ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss 64-byte shared secret
 * @return NO when this series has no X448 accelerator
 */
mp_bool_t mp_crypto_x448_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss) {
	static mp_bool_t said;

	if(!ss || (ctlen && !ct))
		return(NO);
#ifdef MP_ARCH_X448
	mp_bool_t ok;

#ifdef SUPPORT_COMMON_CRYPTO_FUSE
	if(mp_crypto_fuse_check(fuse, MP_CRYPTO_FUSE_X448) != YES)
		return(NO);
#endif
	ok = mp_arch_x448_decaps_fuse(fuse, wrapped, wrappedlen, ct, ctlen, ss);
	if(ok != YES && said == NO) {
		mp_printk("X448: the accelerator refused the fuse");
		said = YES;
	}
	return(ok);
#else
	(void)fuse;
	(void)wrapped;
	(void)wrappedlen;
	if(said == NO) {
		mp_printk("X448: this build has no fuse");
		said = YES;
	}
	return(NO);
#endif
}

/**@}*/

#endif
