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

#ifdef SUPPORT_COMMON_CRYPTO_KEM_X25519

/**
@defgroup mpCommonX25519 X25519 KEM

@ingroup mpCommon

@brief DHKEM(X25519, HKDF-SHA256), RFC 9180 and RFC 7748

The ladder always runs from bit 254 down to bit 0 and swaps through a mask.
The swap bit is hidden from the compiler: a visible 0 or 1 becomes a branch.
The shared secret is mp_crypto_dhkem() with SHA-256 and suite 0x0020.
A field element is eight little-endian words. Reduction is a fixed sequence
of adds, so a secret coordinate never selects a branch or a table entry.
One ladder runs at a time.

@{
*/

#define _A24 121665u

typedef struct {
	uint32_t v[8];
} _fe;

/** p = 2^255 - 19 */
static const uint32_t _p[8] = {
	0xffffffedU, 0xffffffffU, 0xffffffffU, 0xffffffffU,
	0xffffffffU, 0xffffffffU, 0xffffffffU, 0x7fffffffU
};

/** p-2, little-endian, for the inversion chain */
static const uint8_t _pm2[32] = {
	0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

static const uint8_t _base[32] = { 9 };

static const uint8_t _suite[5] = { 'K', 'E', 'M', 0x00, 0x20 };

static void _copy(_fe *o, const _fe *a) {
	int i;

	for(i = 0; i < 8; i++)
		o->v[i] = a->v[i];
}

static void _zero(_fe *o) {
	int i;

	for(i = 0; i < 8; i++)
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

	for(i = 0; i < 8; i++)
		dst->v[i] ^= mask & (dst->v[i] ^ src->v[i]);
}

static void _cswap(_fe *a, _fe *b, uint32_t bit) {
	uint32_t mask = 0u - _opaque(bit);
	int i;

	for(i = 0; i < 8; i++) {
		uint32_t t = mask & (a->v[i] ^ b->v[i]);

		a->v[i] ^= t;
		b->v[i] ^= t;
	}
}

/** Fold a carry that stands for a multiple of 2^256, and 2^256 ≡ 38. */
static void _fold(_fe *o, uint64_t c) {
	int n, i;

	for(n = 0; n < 3; n++) {
		c *= 38;
		for(i = 0; i < 8; i++) {
			c += o->v[i];
			o->v[i] = (uint32_t)c;
			c >>= 32;
		}
	}
}

static void _add(_fe *o, const _fe *a, const _fe *b) {
	uint64_t c = 0;
	int i;

	for(i = 0; i < 8; i++) {
		c += (uint64_t)a->v[i] + b->v[i];
		o->v[i] = (uint32_t)c;
		c >>= 32;
	}
	_fold(o, c);
}

/**
 * o = a - b. The representative stays in 256 bits and congruent to the
 * difference modulo p. The borrow is a mask, not a branch.
 */
static void _sub(_fe *o, const _fe *a, const _fe *b) {
	uint32_t br = 0, br2 = 0;
	_fe t, u;
	int i;

	for(i = 0; i < 8; i++) {
		uint64_t s = (uint64_t)a->v[i] - b->v[i] - br;

		o->v[i] = (uint32_t)s;
		br = (uint32_t)((s >> 32) & 1u);
	}
	for(i = 0; i < 8; i++) {
		uint32_t dec = (i == 0) ? 38u * br : 0u;
		uint64_t s = (uint64_t)o->v[i] - dec - br2;

		t.v[i] = (uint32_t)s;
		br2 = (uint32_t)((s >> 32) & 1u);
	}
	/* when the subtraction of 38 borrowed, add p back: that is t - (2^255 + 19) */
	{
		uint32_t br3 = 0;

		for(i = 0; i < 8; i++) {
			uint32_t dec = 0;

			if(i == 0)
				dec = 19;
			if(i == 7)
				dec = 0x80000000u;
			{
				uint64_t s = (uint64_t)t.v[i] - dec - br3;

				u.v[i] = (uint32_t)s;
				br3 = (uint32_t)((s >> 32) & 1u);
			}
		}
		(void)br3;
	}
	_copy(o, &t);
	_cmov(o, &u, br2);
}

static void _mul(_fe *o, const _fe *a, const _fe *b) {
	uint32_t t[16];
	uint64_t c;
	int i, j;

	for(i = 0; i < 16; i++)
		t[i] = 0;
	for(i = 0; i < 8; i++) {
		c = 0;
		for(j = 0; j < 8; j++) {
			c += (uint64_t)t[i + j] + (uint64_t)a->v[i] * b->v[j];
			t[i + j] = (uint32_t)c;
			c >>= 32;
		}
		t[i + 8] = (uint32_t)c;
	}
	c = 0;
	for(i = 0; i < 8; i++) {
		c += t[i] + (uint64_t)38 * t[i + 8];
		o->v[i] = (uint32_t)c;
		c >>= 32;
	}
	_fold(o, c);
}

static void _sqr(_fe *o, const _fe *a) {
	_fe t;

	_copy(&t, a);
	_mul(o, &t, &t);
}

static void _mul_small(_fe *o, const _fe *a, uint32_t s) {
	uint64_t c = 0;
	int i;

	for(i = 0; i < 8; i++) {
		c += (uint64_t)a->v[i] * s;
		o->v[i] = (uint32_t)c;
		c >>= 32;
	}
	_fold(o, c);
}

static void _canon(_fe *o) {
	int n, i;

	for(n = 0; n < 2; n++) {
		uint32_t t[8];
		uint32_t br = 0;
		uint32_t mask;

		for(i = 0; i < 8; i++) {
			uint64_t s = (uint64_t)o->v[i] - _p[i] - br;

			t[i] = (uint32_t)s;
			br = (uint32_t)((s >> 32) & 1u);
		}
		mask = 0u - (1u - br);
		for(i = 0; i < 8; i++)
			o->v[i] ^= mask & (o->v[i] ^ t[i]);
	}
}

static void _load(_fe *o, const uint8_t in[32]) {
	int i;

	for(i = 0; i < 8; i++) {
		o->v[i] = (uint32_t)in[i * 4]
			| ((uint32_t)in[i * 4 + 1] << 8)
			| ((uint32_t)in[i * 4 + 2] << 16)
			| ((uint32_t)in[i * 4 + 3] << 24);
	}
	o->v[7] &= 0x7fffffffu;
}

static void _store(uint8_t out[32], const _fe *a) {
	_fe t;
	int i;

	_copy(&t, a);
	_canon(&t);
	for(i = 0; i < 8; i++) {
		out[i * 4] = (uint8_t)t.v[i];
		out[i * 4 + 1] = (uint8_t)(t.v[i] >> 8);
		out[i * 4 + 2] = (uint8_t)(t.v[i] >> 16);
		out[i * 4 + 3] = (uint8_t)(t.v[i] >> 24);
	}
}

static void _inv(_fe *o, const _fe *a) {
	_fe r, s;
	int i;

	_set1(&r);
	for(i = 254; i >= 0; i--) {
		uint32_t bit = (_pm2[i >> 3] >> (i & 7)) & 1u;

		_sqr(&r, &r);
		_mul(&s, &r, a);
		_cmov(&r, &s, bit);
	}
	_copy(o, &r);
}

/**
 * @brief X25519
 * @param[out] out 32-byte u-coordinate
 * @param[in] scalar 32-byte scalar, clamped here
 * @param[in] point 32-byte u-coordinate
 */
void mp_crypto_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) {
	uint8_t k[32];
	_fe x1, x2, z2, x3, z3;
	_fe A, AA, B, BB, E, C, D, DA, CB, T;
	uint32_t swap = 0;
	int t;

	memcpy(k, scalar, 32);
	k[0] &= 248;
	k[31] &= 127;
	k[31] |= 64;
	_load(&x1, point);
	_set1(&x2);
	_zero(&z2);
	_copy(&x3, &x1);
	_set1(&z3);
	for(t = 254; t >= 0; t--) {
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
 * @brief Build an X25519 key pair
 * @param[in] seed 32-byte private key
 * @param[in] seedlen Length of seed
 * @param[out] priv The same 32 bytes
 * @param[out] pub The u-coordinate of the base point
 * @return FALSE when the seed is shorter than 32 bytes
 */
mp_ret_t mp_crypto_x25519_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	if(!seed || !priv || !pub || seedlen < 32)
		return(FALSE);
	memcpy(priv, seed, 32);
	mp_crypto_x25519(pub, priv, _base);
	return(TRUE);
}

/**
 * @brief Encapsulate with DHKEM(X25519, HKDF-SHA256)
 * @param[in] pub Recipient public key, 32 bytes
 * @param[in] coins Ephemeral secret, 32 bytes
 * @param[in] coinslen Length of coins
 * @param[out] ss 32-byte shared secret
 * @param[out] ct Ephemeral public key, 32 bytes
 * @return 32, or 0
 */
unsigned int mp_crypto_x25519_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct) {
	uint8_t dh[32], ctx[64];
	unsigned int n;

	if(!pub || !coins || !ss || !ct || coinslen < 32)
		return(0);
	mp_crypto_x25519(ct, coins, _base);
	mp_crypto_x25519(dh, coins, pub);
	memcpy(ctx, ct, 32);
	memcpy(ctx + 32, pub, 32);
	n = mp_crypto_dhkem(ss, 32, "SHA256", _suite, dh, 32, ctx, 64);
	memset(dh, 0, sizeof(dh));
	memset(ctx, 0, sizeof(ctx));
	return(n);
}

/**
 * @brief Decapsulate with DHKEM(X25519, HKDF-SHA256)
 * @param[in] priv Recipient private key, 32 bytes
 * @param[in] ct Ephemeral public key
 * @param[in] ctlen Length of ct
 * @param[out] ss 32-byte shared secret
 * @return YES when ctlen is 32
 */
mp_bool_t mp_crypto_x25519_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss) {
	uint8_t dh[32], pk[32], ctx[64];
	unsigned int n;

	if(!priv || !ct || !ss || ctlen != 32)
		return(NO);
	mp_crypto_x25519(pk, priv, _base);
	mp_crypto_x25519(dh, priv, ct);
	memcpy(ctx, ct, 32);
	memcpy(ctx + 32, pk, 32);
	n = mp_crypto_dhkem(ss, 32, "SHA256", _suite, dh, 32, ctx, 64);
	memset(dh, 0, sizeof(dh));
	memset(pk, 0, sizeof(pk));
	memset(ctx, 0, sizeof(ctx));
	return(n == 32 ? YES : NO);
}

/**
 * @brief Decapsulate with a fuse slot
 * @param[in] fuse Slot of role MP_CRYPTO_FUSE_X25519
 * @param[in] wrapped Blob the arch defines
 * @param[in] wrappedlen Byte length of wrapped
 * @param[in] ct Ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss 32-byte shared secret
 * @return NO when this series has no X25519 accelerator
 */
mp_bool_t mp_crypto_x25519_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss) {
	static mp_bool_t said;

	if(!ss || (ctlen && !ct))
		return(NO);
#ifdef MP_ARCH_X25519
	mp_bool_t ok;

#ifdef SUPPORT_COMMON_CRYPTO_FUSE
	if(mp_crypto_fuse_check(fuse, MP_CRYPTO_FUSE_X25519) != YES)
		return(NO);
#endif
	ok = mp_arch_x25519_decaps_fuse(fuse, wrapped, wrappedlen, ct, ctlen, ss);
	if(ok != YES && said == NO) {
		mp_printk("X25519: the accelerator refused the fuse");
		said = YES;
	}
	return(ok);
#else
	(void)fuse;
	(void)wrapped;
	(void)wrappedlen;
	if(said == NO) {
		mp_printk("X25519: this build has no fuse");
		said = YES;
	}
	return(NO);
#endif
}

/**@}*/

#endif
