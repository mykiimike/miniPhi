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

#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ED25519

/**
@defgroup mpCommonEd25519 Ed25519

@ingroup mpCommon

@brief Ed25519 signatures (RFC 8032)

The field is 2^255 - 19, sixteen limbs of 16 bits, and a point is kept in
extended coordinates so addition has no special case. The scalar, which is
a multiple of the group order L, is reduced with the bignum. Nothing is
allocated; the limbs below are the scratch, so one signature runs at a time.

@{
*/

typedef int64_t fe[16];

typedef struct {
	fe x, y, z, t;
} ge;

static fe _d, _d2, _sqrtm1;
static ge _base;
static int _ready;
static fe _t[8];
static fe _u[4];
static fe _powb, _powr, _powt;

/* p-2, (p+3)/8 and (p-1)/4, little-endian, for the inverse and the square root */
static const uint8_t _exp_inv[32] = {
	0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
};
static const uint8_t _exp_sqrt[32] = {
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f,
};
static const uint8_t _exp_m1[32] = {
	0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f,
};
static const uint8_t _pbytes[32] = {
	0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
};
static const uint8_t _L[32] = {
	0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
};
static const uint8_t _bx[32] = {
	0x1a, 0xd5, 0x25, 0x8f, 0x60, 0x2d, 0x56, 0xc9, 0xb2, 0xa7, 0x25, 0x95, 0x60, 0xc7, 0x2c, 0x69,
	0x5c, 0xdc, 0xd6, 0xfd, 0x31, 0xe2, 0xa4, 0xc0, 0xfe, 0x53, 0x6e, 0xcd, 0xd3, 0x36, 0x69, 0x21,
};
static const uint8_t _by[32] = {
	0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
};

static void fe_0(fe o) {
	memset(o, 0, sizeof(fe));
}

static void fe_1(fe o) {
	fe_0(o);
	o[0] = 1;
}

static void fe_carry(fe o) {
	int i;
	int64_t c;

	for(i = 0; i < 16; i++) {
		o[i] += (int64_t)1 << 16;
		c = o[i] >> 16;
		o[i] -= c << 16;
		if(i < 15)
			o[i + 1] += c - 1;
		else
			o[0] += (c - 1) * 38;
	}
}

static void fe_norm(fe o) {
	/* A fixed number of carries, no early return: the count of passes it
	 * took to settle used to leak the magnitude of a secret field element. */
	int k;

	for(k = 0; k < 8; k++)
		fe_carry(o);
}

static void fe_add(fe o, const fe a, const fe b) {
	int i;

	for(i = 0; i < 16; i++)
		o[i] = a[i] + b[i];
	fe_norm(o);
}

static void fe_sub(fe o, const fe a, const fe b) {
	int i;

	for(i = 0; i < 16; i++)
		o[i] = a[i] - b[i];
	fe_norm(o);
}

static void fe_mul(fe o, const fe a, const fe b) {
	int64_t t[31];
	int i, j;

	memset(t, 0, sizeof(t));
	for(i = 0; i < 16; i++)
		for(j = 0; j < 16; j++)
			t[i + j] += a[i] * b[j];
	for(i = 0; i < 15; i++)
		t[i] += 38 * t[i + 16];
	for(i = 0; i < 16; i++)
		o[i] = t[i];
	fe_norm(o);
}

static void fe_sq(fe o, const fe a) {
	fe_mul(o, a, a);
}

static void fe_cmov(fe d, const fe s, int bit) {
	int64_t mask = -(int64_t)(bit & 1);
	int i;

	for(i = 0; i < 16; i++)
		d[i] = (d[i] & ~mask) | (s[i] & mask);
}

static void fe_pow(fe out, const fe base, const uint8_t exp[32]) {
	int i;

	fe_1(_powr);
	memcpy(_powb, base, sizeof(fe));
	for(i = 255; i >= 0; i--) {
		fe_sq(_powr, _powr);
		fe_mul(_powt, _powr, _powb);
		fe_cmov(_powr, _powt, (exp[i >> 3] >> (i & 7)) & 1);
	}
	memcpy(out, _powr, sizeof(fe));
}

static void fe_inv(fe o, const fe a) {
	fe_pow(o, a, _exp_inv);
}

static int _bytes_ge(const uint8_t *a, const uint8_t *b) {
	int i;

	for(i = 31; i >= 0; i--) {
		if(a[i] > b[i])
			return(1);
		if(a[i] < b[i])
			return(0);
	}
	return(1);
}

static void _sub32(uint8_t *a, const uint8_t *b) {
	int br = 0;
	int i;

	for(i = 0; i < 32; i++) {
		int v = a[i] - b[i] - br;

		a[i] = (uint8_t)v;
		br = v < 0;
	}
}

static void fe_to_bytes(uint8_t out[32], const fe a) {
	fe t;
	int i, k;

	memcpy(t, a, sizeof(fe));
	fe_norm(t);
	memset(out, 0, 32);
	for(i = 0; i < 16; i++) {
		out[2 * i] = (uint8_t)t[i];
		out[2 * i + 1] = (uint8_t)((uint32_t)t[i] >> 8);
	}
	for(k = 0; k < 4 && _bytes_ge(out, _pbytes); k++)
		_sub32(out, _pbytes);
}

static void fe_from_bytes(fe o, const uint8_t s[32]) {
	uint8_t t[32];
	int i;

	memcpy(t, s, 32);
	t[31] &= 127;
	for(i = 0; i < 16; i++)
		o[i] = (int64_t)t[2 * i] + ((int64_t)t[2 * i + 1] << 8);
}

static void fe_neg(fe o, const fe a) {
	fe_0(_u[3]);
	fe_sub(o, _u[3], a);
}

static int fe_eq(const fe a, const fe b) {
	uint8_t xa[32], xb[32];

	fe_to_bytes(xa, a);
	fe_to_bytes(xb, b);
	return(memcmp(xa, xb, 32) == 0);
}

static int fe_is0(const fe a) {
	uint8_t x[32];
	int i;

	fe_to_bytes(x, a);
	for(i = 0; i < 32; i++)
		if(x[i])
			return(0);
	return(1);
}

/* RFC 8032 section 5.1.4. r may alias p or q. */
static void ge_add(ge *r, const ge *p, const ge *q) {
	fe_sub(_u[0], p->y, p->x);
	fe_sub(_u[1], q->y, q->x);
	fe_mul(_t[0], _u[0], _u[1]);
	fe_add(_u[0], p->y, p->x);
	fe_add(_u[1], q->y, q->x);
	fe_mul(_t[1], _u[0], _u[1]);
	fe_mul(_u[0], p->t, q->t);
	fe_mul(_t[2], _u[0], _d2);
	fe_mul(_u[0], p->z, q->z);
	fe_add(_t[3], _u[0], _u[0]);
	fe_sub(_t[4], _t[1], _t[0]);
	fe_sub(_t[5], _t[3], _t[2]);
	fe_add(_t[6], _t[3], _t[2]);
	fe_add(_t[7], _t[1], _t[0]);
	fe_mul(r->x, _t[4], _t[5]);
	fe_mul(r->y, _t[6], _t[7]);
	fe_mul(r->t, _t[4], _t[7]);
	fe_mul(r->z, _t[5], _t[6]);
}

static void ge_double(ge *r, const ge *p) {
	fe_sq(_t[0], p->x);
	fe_sq(_t[1], p->y);
	fe_sq(_u[0], p->z);
	fe_add(_t[2], _u[0], _u[0]);
	fe_add(_t[7], _t[0], _t[1]);
	fe_add(_u[0], p->x, p->y);
	fe_sq(_u[0], _u[0]);
	fe_sub(_t[4], _t[7], _u[0]);
	fe_sub(_t[6], _t[0], _t[1]);
	fe_add(_t[5], _t[2], _t[6]);
	fe_mul(r->x, _t[4], _t[5]);
	fe_mul(r->y, _t[6], _t[7]);
	fe_mul(r->t, _t[4], _t[7]);
	fe_mul(r->z, _t[5], _t[6]);
}

static void ge_cmov(ge *d, const ge *s, int bit) {
	fe_cmov(d->x, s->x, bit);
	fe_cmov(d->y, s->y, bit);
	fe_cmov(d->z, s->z, bit);
	fe_cmov(d->t, s->t, bit);
}

static ge _acc, _sum, _bb, _A, _R;

static void ge_scalarmult(ge *r, const uint8_t s[32], const ge *base) {
	int i;

	fe_0(_acc.x);
	fe_1(_acc.y);
	fe_1(_acc.z);
	fe_0(_acc.t);
	memcpy(&_bb, base, sizeof(ge));
	for(i = 255; i >= 0; i--) {
		ge_double(&_acc, &_acc);
		ge_add(&_sum, &_acc, &_bb);
		ge_cmov(&_acc, &_sum, (s[i >> 3] >> (i & 7)) & 1);
	}
	memcpy(r, &_acc, sizeof(ge));
}

static void ge_encode(uint8_t out[32], const ge *p) {
	uint8_t x[32];

	fe_inv(_u[0], p->z);
	fe_mul(_u[1], p->x, _u[0]);
	fe_mul(_u[2], p->y, _u[0]);
	fe_to_bytes(out, _u[2]);
	fe_to_bytes(x, _u[1]);
	out[31] |= (x[0] & 1) << 7;
}

/* x^2 = (y^2 - 1) / (d y^2 + 1). Fails on a non-point or a non-canonical y. */
static int ge_decode(ge *r, const uint8_t in[32]) {
	uint8_t yb[32];
	uint8_t xb[32];
	int x0;

	memcpy(yb, in, 32);
	x0 = yb[31] >> 7;
	yb[31] &= 127;
	if(_bytes_ge(yb, _pbytes))
		return(0);
	fe_from_bytes(r->y, yb);
	fe_1(r->z);
	fe_sq(_t[0], r->y);
	fe_mul(_t[1], _t[0], _d);
	fe_1(_u[0]);
	fe_sub(_t[0], _t[0], _u[0]);
	fe_add(_t[1], _t[1], _u[0]);
	fe_inv(_t[2], _t[1]);
	fe_mul(_t[0], _t[0], _t[2]);
	fe_pow(r->x, _t[0], _exp_sqrt);
	fe_sq(_t[2], r->x);
	if(!fe_eq(_t[2], _t[0])) {
		fe_mul(r->x, r->x, _sqrtm1);
		fe_sq(_t[2], r->x);
		if(!fe_eq(_t[2], _t[0]))
			return(0);
	}
	fe_to_bytes(xb, r->x);
	if(fe_is0(r->x) && x0)
		return(0);
	if((xb[0] & 1) != x0)
		fe_neg(r->x, r->x);
	fe_mul(r->t, r->x, r->y);
	return(1);
}

static void _ready_up(void) {
	fe two;

	if(_ready)
		return;
	fe_0(_u[0]);
	_u[0][0] = 121665;
	fe_0(_u[1]);
	_u[1][0] = 121666;
	fe_inv(_u[1], _u[1]);
	fe_mul(_d, _u[0], _u[1]);
	fe_neg(_d, _d);
	fe_add(_d2, _d, _d);
	fe_0(two);
	two[0] = 2;
	fe_pow(_sqrtm1, two, _exp_m1);
	fe_from_bytes(_base.x, _bx);
	fe_from_bytes(_base.y, _by);
	fe_1(_base.z);
	fe_mul(_base.t, _base.x, _base.y);
	_ready = 1;
}

static void sc_reduce(uint8_t out[32], const uint8_t *in, unsigned int len) {
	uint32_t x[16], l[8], r[8];

	mp_bn_from_le(x, 16, in, len > 64 ? 64 : len);
	mp_bn_from_le(l, 8, _L, 32);
	mp_bn_mod(r, x, 16, l, 8);
	mp_bn_to_le(out, 32, r, 8);
	memset(x, 0, sizeof(x));
}

static void sc_muladd(uint8_t out[32], const uint8_t *a, const uint8_t *b, const uint8_t *c) {
	uint32_t aa[8], bb[8], cc[8], prod[16], l[8], r[8];
	uint32_t carry;
	int i;

	mp_bn_from_le(aa, 8, a, 32);
	mp_bn_from_le(bb, 8, b, 32);
	mp_bn_from_le(cc, 8, c, 32);
	mp_bn_mul(prod, aa, bb, 8);
	carry = mp_bn_add(prod, prod, cc, 8);
	for(i = 8; carry && i < 16; i++) {
		uint64_t s = (uint64_t)prod[i] + carry;

		prod[i] = (uint32_t)s;
		carry = (uint32_t)(s >> 32);
	}
	mp_bn_from_le(l, 8, _L, 32);
	mp_bn_mod(r, prod, 16, l, 8);
	mp_bn_to_le(out, 32, r, 8);
	memset(aa, 0, sizeof(aa));
	memset(bb, 0, sizeof(bb));
	memset(prod, 0, sizeof(prod));
}

static int sc_lt_L(const uint8_t s[32]) {
	uint32_t a[8], l[8];

	mp_bn_from_le(a, 8, s, 32);
	mp_bn_from_le(l, 8, _L, 32);
	return(mp_bn_cmp(a, l, 8) < 0);
}

static void _clamp(uint8_t s[32]) {
	s[0] &= 248;
	s[31] &= 63;
	s[31] |= 64;
}

static void _public_from(const uint8_t priv[32], uint8_t pub[32], uint8_t prefix[32]) {
	uint8_t h[64];
	uint8_t s[32];

	_ready_up();
	mp_sha512(priv, 32, h);
	memcpy(s, h, 32);
	if(prefix)
		memcpy(prefix, h + 32, 32);
	_clamp(s);
	ge_scalarmult(&_acc, s, &_base);
	ge_encode(pub, &_acc);
	memset(h, 0, sizeof(h));
	memset(s, 0, sizeof(s));
}

/**
 * @brief Private key and its public key
 *
 * @param[in] seed 32 bytes; this is the private key
 * @param[in] seedlen Must be 32
 * @param[out] priv 32 bytes
 * @param[out] pub 32 bytes
 * @return FALSE when the seed is shorter
 */
mp_ret_t mp_ed25519_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	if(!seed || !priv || !pub || seedlen < MP_ED25519_PRIV)
		return(FALSE);
	memcpy(priv, seed, MP_ED25519_PRIV);
	mp_ed25519_public(priv, pub);
	return(TRUE);
}

/**
 * @brief Public key of a private key
 *
 * @param[in] priv 32 bytes
 * @param[out] pub 32 bytes
 */
void mp_ed25519_public(const void *priv, void *pub) {
	_public_from(priv, pub, NULL);
}

/**
 * @brief Sign
 *
 * @param[in] priv 32 bytes
 * @param[in] msg The message, may be empty
 * @param[in] len Bytes of msg
 * @param[out] sig 64 bytes
 * @return 64, or 0 when priv or sig is missing
 */
unsigned int mp_ed25519_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig) {
	uint8_t prefix[32], pub[32], h[64], r[32], k[32], s[32];
	mp_sha512_t sha;

	if(!priv || !sig || (len && !msg))
		return(0);
	_public_from(priv, pub, prefix);
	mp_sha512(priv, 32, h);
	memcpy(s, h, 32);
	_clamp(s);
	mp_sha512_init(&sha);
	mp_sha512_update(&sha, prefix, 32);
	if(len)
		mp_sha512_update(&sha, msg, len);
	mp_sha512_final(&sha, h);
	sc_reduce(r, h, 64);
	ge_scalarmult(&_acc, r, &_base);
	ge_encode(sig, &_acc);
	mp_sha512_init(&sha);
	mp_sha512_update(&sha, sig, 32);
	mp_sha512_update(&sha, pub, 32);
	if(len)
		mp_sha512_update(&sha, msg, len);
	mp_sha512_final(&sha, h);
	sc_reduce(k, h, 64);
	sc_muladd(sig + 32, k, s, r);
	memset(prefix, 0, sizeof(prefix));
	memset(h, 0, sizeof(h));
	memset(r, 0, sizeof(r));
	memset(k, 0, sizeof(k));
	memset(s, 0, sizeof(s));
	return(MP_ED25519_SIG);
}

/**
 * @brief Check a signature
 *
 * @param[in] pub 32 bytes
 * @param[in] msg The message
 * @param[in] len Bytes of msg
 * @param[in] sig 64 bytes
 * @param[in] siglen Must be 64
 * @return YES when it checks
 */
mp_bool_t mp_ed25519_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen) {
	const uint8_t *in = sig;
	uint8_t h[64], k[32], left[32], right[32];
	mp_sha512_t sha;

	if(!pub || !sig || siglen != MP_ED25519_SIG || (len && !msg))
		return(NO);
	if(!sc_lt_L(in + 32))
		return(NO);
	_ready_up();
	if(!ge_decode(&_A, pub) || !ge_decode(&_R, in))
		return(NO);
	mp_sha512_init(&sha);
	mp_sha512_update(&sha, in, 32);
	mp_sha512_update(&sha, pub, 32);
	if(len)
		mp_sha512_update(&sha, msg, len);
	mp_sha512_final(&sha, h);
	sc_reduce(k, h, 64);
	ge_scalarmult(&_acc, in + 32, &_base);
	ge_encode(left, &_acc);
	ge_scalarmult(&_acc, k, &_A);
	ge_add(&_acc, &_acc, &_R);
	ge_encode(right, &_acc);
	memset(h, 0, sizeof(h));
	memset(k, 0, sizeof(k));
	return(memcmp(left, right, 32) == 0 ? YES : NO);
}

/**@}*/

#endif
