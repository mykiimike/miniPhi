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

#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ECDSA_P256

/**
@defgroup mpCommonEcdsaP256 ECDSA P-256

@ingroup mpCommon

@brief ECDSA on NIST P-256, SHA-256, RFC 6979

Field elements use the NIST reduction (2^256 folds into the low half).
Points are Jacobian, so a scalar multiplication inverts only once, at the
end. The nonce is HMAC-SHA-256 of the private key and the digest, which
makes the signature deterministic.

@{
*/

#define W 8

static const uint32_t _p[W] = {
	0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0xffffffff,
};
static const uint32_t _n[W] = {
	0xfc632551, 0xf3b9cac2, 0xa7179e84, 0xbce6faad, 0xffffffff, 0xffffffff, 0x00000000, 0xffffffff,
};
static const uint32_t _b[W] = {
	0x27d2604b, 0x3bce3c3e, 0xcc53b0f6, 0x651d06b0, 0x769886bc, 0xb3ebbd55, 0xaa3a93e7, 0x5ac635d8,
};
static const uint32_t _gx[W] = {
	0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81, 0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2,
};
static const uint32_t _gy[W] = {
	0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357, 0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2,
};

typedef struct {
	uint32_t x[W], y[W], z[W];
	uint32_t inf;
} pt;

static uint32_t _f[16][W];
static pt _R, _T, _Q;
/* candidates for the constant-time mixed addition; never overlap with _R/_T/_Q */
static pt _g_add, _g_dbl, _g_inf, _g_q;

/* 2^256 ≡ 2^224 - 2^192 - 2^96 + 1, folded until the high half is gone.
 * Constant time: a fixed number of rounds (the fold is a no-op once the
 * high half is zero), the borrow runs over a fixed tail, and the final
 * subtract is selected by cmov. */
static void fe_reduce(uint32_t r[W], const uint32_t *c, int words) {
	uint32_t acc[20];
	uint32_t high[W];
	uint32_t tmp[W];
	uint64_t carry;
	int round, i, k, br;

	memset(acc, 0, sizeof(acc));
	for(i = 0; i < words && i < 20; i++)
		acc[i] = c[i];
	for(round = 0; round < 16; round++) {
		for(i = 0; i < W; i++) {
			high[i] = acc[W + i];
			acc[W + i] = 0;
		}
		carry = 0;
		for(i = 0; i < W; i++) {
			carry += (uint64_t)acc[i] + high[i];
			acc[i] = (uint32_t)carry;
			carry >>= 32;
		}
		acc[W] += (uint32_t)carry;
		/* + high<<224, - high<<192, - high<<96 */
		for(k = 0; k < 3; k++) {
			int at = (k == 0) ? 7 : (k == 1) ? 6 : 3;
			int sub = (k != 0);

			carry = 0;
			br = 0;
			for(i = 0; i < W; i++) {
				if(!sub) {
					carry += (uint64_t)acc[at + i] + high[i];
					acc[at + i] = (uint32_t)carry;
					carry >>= 32;
				}
				else {
					uint64_t d = (uint64_t)acc[at + i] - high[i] - (uint32_t)br;

					acc[at + i] = (uint32_t)d;
					br = (d >> 32) ? 1 : 0;
				}
			}
			if(!sub)
				acc[at + W] += (uint32_t)carry;
			/* borrow over a fixed tail, no br && condition */
			for(i = at + W; i < 20; i++) {
				uint64_t d = (uint64_t)acc[i] - (uint32_t)br;

				acc[i] = (uint32_t)d;
				br = (d >> 32) ? 1 : 0;
			}
		}
	}
	mp_bn_sub(tmp, acc, _p, W);
	mp_bn_cmov(acc, tmp, W, (uint32_t)(mp_bn_cmp(acc, _p, W) >= 0));
	mp_bn_copy(r, acc, W);
}

static void fe_add(uint32_t r[W], const uint32_t a[W], const uint32_t b[W]) {
	mp_bn_add_mod(r, a, b, _p, W);
}

static void fe_sub(uint32_t r[W], const uint32_t a[W], const uint32_t b[W]) {
	mp_bn_sub_mod(r, a, b, _p, W);
}

static void fe_mul(uint32_t r[W], const uint32_t a[W], const uint32_t b[W]) {
	uint32_t t[W * 2];

	mp_bn_mul(t, a, b, W);
	fe_reduce(r, t, W * 2);
}

static void fe_sqr(uint32_t r[W], const uint32_t a[W]) {
	fe_mul(r, a, a);
}

static void fe_inv(uint32_t r[W], const uint32_t a[W]) {
	uint32_t e[W];
	uint32_t one[W];

	mp_bn_zero(one, W);
	one[0] = 2;
	mp_bn_sub(e, _p, one, W);
	mp_bn_modexp(r, a, e, W, _p, W);
}

static void sc_reduce(uint32_t r[W], const uint32_t a[W]) {
	if(mp_bn_cmp(a, _n, W) >= 0)
		mp_bn_sub(r, a, _n, W);
	else
		mp_bn_copy(r, a, W);
}

static void sc_add(uint32_t r[W], const uint32_t a[W], const uint32_t b[W]) {
	mp_bn_add_mod(r, a, b, _n, W);
}

static void sc_mul(uint32_t r[W], const uint32_t a[W], const uint32_t b[W]) {
	uint32_t t[W * 2];

	mp_bn_mul(t, a, b, W);
	mp_bn_mod(r, t, W * 2, _n, W);
}

static void sc_inv(uint32_t r[W], const uint32_t a[W]) {
	uint32_t e[W];
	uint32_t one[W];

	mp_bn_zero(one, W);
	one[0] = 2;
	mp_bn_sub(e, _n, one, W);
	mp_bn_modexp(r, a, e, W, _n, W);
}

static void pt_inf(pt *p) {
	memset(p, 0, sizeof(*p));
	p->inf = 1;
}

static void pt_cmov(pt *d, const pt *s, uint32_t bit) {
	mp_bn_cmov(d->x, s->x, W, bit);
	mp_bn_cmov(d->y, s->y, W, bit);
	mp_bn_cmov(d->z, s->z, W, bit);
	d->inf ^= (d->inf ^ s->inf) & (0u - (bit & 1u));
}

static void pt_affine_G(pt *p) {
	mp_bn_copy(p->x, _gx, W);
	mp_bn_copy(p->y, _gy, W);
	mp_bn_zero(p->z, W);
	p->z[0] = 1;
	p->inf = 0;
}

static void pt_set_xy(pt *p, const uint32_t x[W], const uint32_t y[W]) {
	mp_bn_copy(p->x, x, W);
	mp_bn_copy(p->y, y, W);
	mp_bn_zero(p->z, W);
	p->z[0] = 1;
	p->inf = 0;
}

/* a = -3. Output may alias the input. Branchless: the formula on a zero
 * point (infinity) yields a zero point, so the inf flag just propagates. */
static void pt_dbl(pt *r, const pt *p) {
	pt src = *p;
	fe_sqr(_f[0], src.z);
	fe_sqr(_f[1], src.y);
	fe_mul(_f[2], src.x, _f[1]);
	fe_sub(_f[3], src.x, _f[0]);
	fe_add(_f[4], src.x, _f[0]);
	fe_mul(_f[3], _f[3], _f[4]);
	fe_add(_f[4], _f[3], _f[3]);
	fe_add(_f[3], _f[4], _f[3]);
	fe_add(_f[4], _f[2], _f[2]);
	fe_add(_f[4], _f[4], _f[4]);
	fe_add(_f[4], _f[4], _f[4]);
	fe_sqr(_f[5], _f[3]);
	fe_sub(r->x, _f[5], _f[4]);
	fe_sqr(_f[5], _f[1]);
	fe_add(_f[5], _f[5], _f[5]);
	fe_add(_f[5], _f[5], _f[5]);
	fe_add(_f[5], _f[5], _f[5]);
	/* 4*beta - X3, then * alpha, minus 8*gamma^2. beta was doubled to 8*beta in _f[4],
	 * so 4*beta is rebuilt from _f[2]. */
	fe_add(_f[6], _f[2], _f[2]);
	fe_add(_f[6], _f[6], _f[6]);
	fe_sub(_f[6], _f[6], r->x);
	fe_mul(_f[6], _f[3], _f[6]);
	fe_sub(r->y, _f[6], _f[5]);
	fe_add(_f[6], src.y, src.z);
	fe_sqr(_f[6], _f[6]);
	fe_sub(_f[6], _f[6], _f[1]);
	fe_sub(r->z, _f[6], _f[0]);
	r->inf = src.inf;
}

/* Jacobian p plus an affine q (Z = 1). Constant time: the general add, the
 * doubling (for p == q) and the inverse (for p == -q) are all computed, and
 * the right one is selected by cmov on flags. q is the base point of a scalar
 * multiplication and is never infinity. */
static void pt_add(pt *r, const pt *p, const pt *q) {
	pt src;
	uint32_t hzero, rzero, dbl_case, inv_case, pzero;

	src = *p;
	fe_sqr(_f[0], src.z);
	fe_mul(_f[1], src.z, _f[0]);
	fe_mul(_f[2], q->x, _f[0]);
	fe_mul(_f[3], q->y, _f[1]);
	fe_sub(_f[4], _f[2], src.x);
	fe_sub(_f[5], _f[3], src.y);
	hzero = (uint32_t)mp_bn_is_zero(_f[4], W);
	rzero = (uint32_t)mp_bn_is_zero(_f[5], W);
	dbl_case = hzero & rzero;
	inv_case = hzero & (rzero ^ 1u);
	pzero = (uint32_t)src.inf;

	/* general case, always computed into _g_add */
	fe_sqr(_f[6], _f[4]);
	fe_add(_f[7], _f[6], _f[6]);
	fe_add(_f[7], _f[7], _f[7]);
	fe_mul(_f[8], _f[4], _f[7]);
	fe_mul(_f[9], src.x, _f[7]);
	fe_add(_f[5], _f[5], _f[5]);
	fe_sqr(_f[10], _f[5]);
	fe_sub(_f[10], _f[10], _f[8]);
	fe_sub(_f[10], _f[10], _f[9]);
	fe_sub(_g_add.x, _f[10], _f[9]);
	fe_sub(_f[10], _f[9], _g_add.x);
	fe_mul(_f[10], _f[5], _f[10]);
	fe_mul(_f[11], src.y, _f[8]);
	fe_add(_f[11], _f[11], _f[11]);
	fe_sub(_g_add.y, _f[10], _f[11]);
	fe_add(_f[10], src.z, _f[4]);
	fe_sqr(_f[10], _f[10]);
	fe_sub(_f[10], _f[10], _f[0]);
	fe_sub(_g_add.z, _f[10], _f[6]);
	_g_add.inf = 0;

	/* doubling for the p == q case, infinity for p == -q, q for p == infinity */
	pt_dbl(&_g_dbl, &src);
	pt_inf(&_g_inf);
	pt_set_xy(&_g_q, q->x, q->y);

	/* priority: pzero (-> q) over dbl over inverse over general */
	pt_cmov(&_g_add, &_g_inf, inv_case);
	pt_cmov(&_g_add, &_g_dbl, dbl_case);
	pt_cmov(&_g_add, &_g_q, pzero);
	*r = _g_add;
}

static void pt_mul(pt *r, const uint8_t k[32], const pt *base) {
	int i;

	pt_inf(&_R);
	for(i = 0; i < 256; i++) {
		uint32_t bit = (k[i >> 3] >> (7 - (i & 7))) & 1u;

		pt_dbl(&_R, &_R);
		pt_add(&_T, &_R, base);
		pt_cmov(&_R, &_T, bit);
	}
	*r = _R;
}

static int pt_x(uint32_t x[W], const pt *p) {
	if(p->inf || mp_bn_is_zero(p->z, W))
		return(0);
	fe_inv(_f[0], p->z);
	fe_sqr(_f[1], _f[0]);
	fe_mul(x, p->x, _f[1]);
	return(1);
}

static int pt_on_curve(const uint32_t x[W], const uint32_t y[W]) {
	fe_sqr(_f[0], y);
	fe_sqr(_f[1], x);
	fe_mul(_f[1], _f[1], x);
	fe_add(_f[2], x, x);
	fe_add(_f[2], _f[2], x);
	fe_sub(_f[1], _f[1], _f[2]);
	fe_add(_f[1], _f[1], _b);
	return(mp_bn_cmp(_f[0], _f[1], W) == 0);
}

static void hash_int(uint32_t e[W], const uint8_t digest[32]) {
	mp_bn_from_be(e, W, digest, 32);
	sc_reduce(e, e);
}

/* RFC 6979 section 3.2, HMAC-SHA-256, qlen = hlen = 256 */
static void rfc6979(uint32_t k[W], const uint8_t priv[32], const uint8_t digest[32]) {
	uint8_t v[32], K[32], h1[32], block[32 + 1 + 32 + 32];
	uint32_t t[W];
	int guard;

	mp_bn_from_be(t, W, digest, 32);
	sc_reduce(t, t);
	mp_bn_to_be(h1, 32, t, W);
	memset(v, 0x01, 32);
	memset(K, 0x00, 32);
	memcpy(block, v, 32);
	block[32] = 0x00;
	memcpy(block + 33, priv, 32);
	memcpy(block + 65, h1, 32);
	mp_hmac("SHA256", K, 32, block, sizeof(block), K);
	mp_hmac("SHA256", K, 32, v, 32, v);
	memcpy(block, v, 32);
	block[32] = 0x01;
	mp_hmac("SHA256", K, 32, block, sizeof(block), K);
	mp_hmac("SHA256", K, 32, v, 32, v);
	for(guard = 0; guard < 8; guard++) {
		mp_hmac("SHA256", K, 32, v, 32, v);
		mp_bn_from_be(k, W, v, 32);
		if(!mp_bn_is_zero(k, W) && mp_bn_cmp(k, _n, W) < 0)
			break;
		memcpy(block, v, 32);
		block[32] = 0x00;
		mp_hmac("SHA256", K, 32, block, 33, K);
		mp_hmac("SHA256", K, 32, v, 32, v);
	}
	memset(v, 0, sizeof(v));
	memset(K, 0, sizeof(K));
}

static int load_priv(uint32_t d[W], const uint8_t priv[32]) {
	mp_bn_from_be(d, W, priv, 32);
	if(mp_bn_is_zero(d, W) || mp_bn_cmp(d, _n, W) >= 0)
		return(0);
	return(1);
}

/**
 * @brief Key pair from 32 seed bytes
 *
 * The seed is the private scalar when it lies in 1..n-1. Otherwise it is
 * reduced mod n, and a seed that collapses to 0 is refused.
 *
 * @return FALSE when the seed is shorter than 32 bytes or reduces to 0
 */
mp_ret_t mp_ecdsa_p256_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	uint32_t d[W];

	if(!seed || !priv || !pub || seedlen < MP_ECDSA_P256_PRIV)
		return(FALSE);
	mp_bn_from_be(d, W, seed, MP_ECDSA_P256_PRIV);
	sc_reduce(d, d);
	if(mp_bn_is_zero(d, W))
		return(FALSE);
	mp_bn_to_be(priv, 32, d, W);
	mp_ecdsa_p256_public(priv, pub);
	memset(d, 0, sizeof(d));
	return(TRUE);
}

/**
 * @brief Public key, x then y, 32 bytes each
 */
void mp_ecdsa_p256_public(const void *priv, void *pub) {
	uint32_t d[W];
	uint8_t k[32];
	pt g, r;
	uint8_t *out = pub;

	mp_bn_from_be(d, W, priv, 32);
	mp_bn_to_be(k, 32, d, W);
	pt_affine_G(&g);
	pt_mul(&r, k, &g);
	pt_x(_f[12], &r);
	/* pt_x left Z^{-2} in _f[1] and Z^{-1} in _f[0]; y = Y * Z^{-3} = Y * Z^{-1} * Z^{-2} */
	fe_mul(_f[13], r.y, _f[0]);
	fe_mul(_f[13], _f[13], _f[1]);
	mp_bn_to_be(out, 32, _f[12], W);
	mp_bn_to_be(out + 32, 32, _f[13], W);
	memset(d, 0, sizeof(d));
	memset(k, 0, sizeof(k));
}

/**
 * @brief Sign, r and s concatenated
 *
 * @return 64, or 0 when the private key is out of range
 */
unsigned int mp_ecdsa_p256_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig) {
	uint8_t digest[32], kbe[32];
	uint32_t d[W], e[W], k[W], r[W], s[W];
	pt g, point;

	if(!priv || !sig || (len && !msg) || !load_priv(d, priv))
		return(0);
	mp_sha256(msg, len, digest);
	hash_int(e, digest);
	mp_bn_to_be(kbe, 32, d, W);
	rfc6979(k, kbe, digest);
	mp_bn_to_be(kbe, 32, k, W);
	pt_affine_G(&g);
	pt_mul(&point, kbe, &g);
	if(!pt_x(r, &point))
		return(0);
	sc_reduce(r, r);
	if(mp_bn_is_zero(r, W))
		return(0);
	sc_inv(_f[14], k);
	sc_mul(_f[15], r, d);
	sc_add(_f[15], _f[15], e);
	sc_mul(s, _f[14], _f[15]);
	if(mp_bn_is_zero(s, W))
		return(0);
	mp_bn_to_be(sig, 32, r, W);
	mp_bn_to_be(sig + 32, 32, s, W);
	memset(d, 0, sizeof(d));
	memset(k, 0, sizeof(k));
	memset(kbe, 0, sizeof(kbe));
	return(MP_ECDSA_P256_SIG);
}

/**
 * @brief Check r||s against the public key x||y
 */
mp_bool_t mp_ecdsa_p256_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen) {
	const uint8_t *in = sig;
	const uint8_t *pk = pub;
	uint8_t digest[32];
	uint32_t e[W], r[W], s[W], w[W], u1[W], u2[W], x[W], y[W], v[W];
	pt g, q, p1, p2;

	if(!pub || !sig || siglen != MP_ECDSA_P256_SIG || (len && !msg))
		return(NO);
	mp_bn_from_be(x, W, pk, 32);
	mp_bn_from_be(y, W, pk + 32, 32);
	mp_bn_from_be(r, W, in, 32);
	mp_bn_from_be(s, W, in + 32, 32);
	if(mp_bn_is_zero(r, W) || mp_bn_cmp(r, _n, W) >= 0)
		return(NO);
	if(mp_bn_is_zero(s, W) || mp_bn_cmp(s, _n, W) >= 0)
		return(NO);
	if(mp_bn_cmp(x, _p, W) >= 0 || mp_bn_cmp(y, _p, W) >= 0 || !pt_on_curve(x, y))
		return(NO);
	mp_sha256(msg, len, digest);
	hash_int(e, digest);
	sc_inv(w, s);
	sc_mul(u1, e, w);
	sc_mul(u2, r, w);
	pt_affine_G(&g);
	{
		uint8_t be[32];

		mp_bn_to_be(be, 32, u1, W);
		pt_mul(&p1, be, &g);
		pt_set_xy(&q, x, y);
		mp_bn_to_be(be, 32, u2, W);
		pt_mul(&p2, be, &q);
		memset(be, 0, sizeof(be));
	}
	if(!pt_x(_f[12], &p2))
		return(NO);
	fe_mul(_f[13], p2.y, _f[0]);
	fe_mul(_f[13], _f[13], _f[1]);
	pt_set_xy(&_Q, _f[12], _f[13]);
	pt_add(&p1, &p1, &_Q);
	if(!pt_x(v, &p1))
		return(NO);
	sc_reduce(v, v);
	return(mp_bn_cmp(v, r, W) == 0 ? YES : NO);
}

/**@}*/

#endif
