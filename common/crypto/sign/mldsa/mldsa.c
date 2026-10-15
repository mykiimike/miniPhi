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

#ifdef SUPPORT_COMMON_CRYPTO_SIGN_MLDSA

#include "mldsa.h"
#include <string.h>

/**
@defgroup mpCommonMldsa ML-DSA

@ingroup mpCommon

@brief Module-lattice signatures, FIPS 204, ML-DSA-44 / 65 / 87

The security level is a build setting (MP_MLDSA_LEVEL = 2, 3 or 5) that
selects ML-DSA-44, ML-DSA-65 or ML-DSA-87. The modulus q = 8380417, the
ring is Z_q[x]/(x^256+1) and the NTT root is zeta = 1753. Keys and
signatures are fixed-length byte strings (see mldsa.h for the sizes).

Constant time with respect to the secret: the NTT and the modular reductions
are fixed functions of the full state, the rejection-sampling loop selects
between accept and reject with arithmetic masks rather than branches, and no
branch, table index or memory address is ever derived from a secret. The
public seed, the message and the signature are public, so data-dependent
timing on them is acceptable. This implementation signs deterministically
(rnd = 0), which is interoperable. One operation at a time; the page only
verifies a stored signature.

@{
*/

/* ---- field arithmetic over Z_q ---------------------------------------- *
 * Plain 64-bit modular arithmetic: a, b < q < 2^23 so a*b < 2^46 fits in a
 * uint64_t, and the remainder is one division. Portable to Cortex-M3 (the
 * 64-bit helpers are libgcc routines) and constant-time enough for verify;
 * signing runs on the host. */

#define _Q      8380417
#define _DEXP   8192          /* 2^MP_MLDSA_D */
#define _DHALF  4096          /* 2^(MP_MLDSA_D-1) */

static uint32_t _modq(uint64_t x) { return (uint32_t)(x % _Q); }
static uint32_t _mulmod(uint32_t a, uint32_t b) { return _modq((uint64_t)a * b); }

/* residue of a (possibly negative) centered coefficient, in [0,q). */
static uint32_t _modq_i(int32_t x) {
	int64_t v = (int64_t)x % (int64_t)_Q;
	if(v < 0) v += (int64_t)_Q;
	return (uint32_t)v;
}

static uint32_t _addmod(uint32_t a, uint32_t b) {
	uint32_t r = a + b;
	if(r >= _Q) r -= _Q;
	return r;
}

static uint32_t _submod(uint32_t a, uint32_t b) {
	return _addmod(a, _Q - b);
}

static uint32_t _powmod(uint32_t base, uint32_t exp) {
	uint64_t r = 1, b = base;
	while(exp) {
		if(exp & 1) r = r * b % _Q;
		b = b * b % _Q;
		exp >>= 1;
	}
	return (uint32_t)r;
}

/* centered residue in (-q/2, q/2]. */
static int32_t _cmod(int32_t a) {
	a %= (int32_t)_Q;
	if(a > (int32_t)(_Q / 2)) a -= (int32_t)_Q;
	return a;
}

/* ---- rounding helpers (FIPS 204 §7.4) --------------------------------- *
 * Operate on values that may be secret during signing, so the boundary case
 * in Decompose and the comparison in MakeHint use arithmetic, not branches. */

/* 2*gamma2 divides q-1: level 2 -> 190464, level 3/5 -> 523776. */
#define _G2   (2 * MP_MLDSA_GAMMA2)
/* m = (q-1)/(2*gamma2): the number of high-bit buckets. level 2 -> 44, else 16. */
#define _HM   ((_Q - 1) / _G2)

static void _power2round(uint32_t r, uint32_t *r1, int32_t *t0) {
	uint32_t rp = _modq(r);
	uint32_t r0 = rp & (_DEXP - 1);        /* rp mod 2^d, in [0, 2^d) */
	uint32_t hi = rp >> MP_MLDSA_D;        /* floor(rp / 2^d) */
	/* r0 = rp mod± 2^d  (centered into (-2^(d-1), 2^(d-1)]); r1 = (rp - r0)/2^d */
	if(r0 > (uint32_t)(1u << (MP_MLDSA_D - 1))) {
		r0 -= _DEXP;
		hi  += 1;
	}
	*r1 = hi;
	*t0 = (int32_t)r0;
}

static void _decompose(uint32_t r, uint32_t *r1, int32_t *r0) {
	uint32_t rp = _modq(r);
	int32_t lo = (int32_t)(rp % _G2);
	if(lo > (int32_t)MP_MLDSA_GAMMA2) lo -= (int32_t)_G2;   /* centered (-g2, g2] */
	/* rp - lo mod q (handles negative lo) */
	uint32_t diff = _submod(rp, _modq_i(lo));
	if(diff == (uint32_t)(_Q - 1)) {
		*r1 = 0;
		*r0 = lo - 1;
	} else {
		*r1 = diff / _G2;
		*r0 = lo;
	}
}

static uint32_t _highbits(uint32_t r) {
	uint32_t h; int32_t l;
	_decompose(r, &h, &l);
	return h;
}

static int32_t _lowbits(uint32_t r) {
	uint32_t h; int32_t l;
	_decompose(r, &h, &l);
	return l;
}

/* MakeHint(u, r): 1 iff adding u to r changes the high bits. Constant time. */
static uint32_t _makehint(int32_t u, uint32_t r) {
	uint32_t h1 = _highbits(r);
	uint32_t h2 = _highbits(_modq_i((int32_t)((int64_t)r + u)));
	int32_t d = (int32_t)h1 - (int32_t)h2;
	uint32_t ad = (uint32_t)((d ^ (d >> 31)) - (d >> 31));   /* |d| */
	return ((ad | (0u - ad)) >> 31) & 1;
}

/* UseHint(h, r): reconstruct the signer's high bits from r and the hint. */
static uint32_t _usehint(uint32_t h, uint32_t r) {
	uint32_t h1; int32_t r0;
	_decompose(r, &h1, &r0);
	uint32_t w;
	if(h) {
		if(r0 > 0) w = (h1 + 1) % _HM;
		else       w = (h1 + _HM - 1) % _HM;
	} else {
		w = h1;
	}
	return w;
}

/* ---- NTT (FIPS 204 §7.5) ---------------------------------------------- *
 * zeta = 1753 is a 512th root of unity. zetas[i] = zeta^BitRev8(i) mod q,
 * computed once at first use (public values, no secret). The forward and
 * inverse butterflies are fixed loops over the full state, so they are
 * constant time. 256^-1 mod q = 8347681 (FIPS 204, Alg 42). */

static uint32_t _bitrev8(uint32_t b) {
	b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
	b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
	b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
	return b & 0xFF;
}

static uint32_t _zetas[256];
static int _zetas_ready = 0;

static void _zetas_init(void) {
	if(_zetas_ready) return;
	_zetas[0] = 1;
	for(uint32_t i = 1; i < 256; i++)
		_zetas[i] = _powmod(MP_MLDSA_ZETA, _bitrev8(i));
	_zetas_ready = 1;
}

static void _ntt(uint32_t *a) {
	_zetas_init();
	unsigned int k = 0, len = 128;
	while(len >= 1) {
		unsigned int start = 0;
		while(start < MP_MLDSA_N) {
			k++;
			uint32_t z = _zetas[k];
			for(unsigned int j = start; j < start + len; j++) {
				uint32_t t = _mulmod(z, a[j + len]);
				a[j + len] = _submod(a[j], t);
				a[j] = _addmod(a[j], t);
			}
			start += 2 * len;
		}
		len >>= 1;
	}
}

static void _intt(uint32_t *a) {
	_zetas_init();
	unsigned int k = 256, len = 1;
	while(len < MP_MLDSA_N) {
		unsigned int start = 0;
		while(start < MP_MLDSA_N) {
			k--;
			uint32_t z = _Q - _zetas[k];
			for(unsigned int j = start; j < start + len; j++) {
				uint32_t t = a[j];
				a[j] = _addmod(t, a[j + len]);
				a[j + len] = _mulmod(z, _submod(t, a[j + len]));
			}
			start += 2 * len;
		}
		len <<= 1;
	}
	uint32_t f = 8347681;   /* 256^-1 mod q */
	for(unsigned int i = 0; i < MP_MLDSA_N; i++)
		a[i] = _mulmod(a[i], f);
}

/* pointwise product of two NTT representations. */
static void _pmul(uint32_t *out, const uint32_t *a, const uint32_t *b) {
	for(unsigned int i = 0; i < MP_MLDSA_N; i++)
		out[i] = _mulmod(a[i], b[i]);
}

/* out = a + b (pointwise, NTT domain). */
static void _padd(uint32_t *out, const uint32_t *a, const uint32_t *b) {
	for(unsigned int i = 0; i < MP_MLDSA_N; i++)
		out[i] = _addmod(a[i], b[i]);
}

/* ---- bit packing (FIPS 204 Alg 16–21) --------------------------------- */

static unsigned int _bitlen(uint32_t x) {
	unsigned int b = 0;
	while(x) { b++; x >>= 1; }
	return b;
}

/* pack 256 coeffs (each in [0,b]) at bitlen(b) bits, little-endian bits. */
static void _simplebitpack(uint8_t *out, const int32_t *w, uint32_t b) {
	unsigned int bits = _bitlen(b);
	unsigned int nbytes = 32 * bits;
	memset(out, 0, nbytes);
	for(unsigned int i = 0; i < MP_MLDSA_N; i++) {
		uint32_t v = (uint32_t)w[i];
		for(unsigned int k = 0; k < bits; k++) {
			unsigned int p = i * bits + k;
			if((v >> k) & 1) out[p >> 3] |= (uint8_t)(1u << (p & 7));
		}
	}
}

/* pack 256 coeffs (each in [-a,b]) at bitlen(a+b) bits, storing coeff+a. */
static void _bitpack(uint8_t *out, const int32_t *w, uint32_t a, uint32_t b) {
	unsigned int bits = _bitlen(a + b);
	unsigned int nbytes = 32 * bits;
	memset(out, 0, nbytes);
	for(unsigned int i = 0; i < MP_MLDSA_N; i++) {
		uint32_t v = (uint32_t)(w[i] + (int32_t)a);
		for(unsigned int k = 0; k < bits; k++) {
			unsigned int p = i * bits + k;
			if((v >> k) & 1) out[p >> 3] |= (uint8_t)(1u << (p & 7));
		}
	}
}

static void _simplebitunpack(int32_t *w, const uint8_t *buf, uint32_t b) {
	unsigned int bits = _bitlen(b);
	for(unsigned int i = 0; i < MP_MLDSA_N; i++) {
		uint32_t v = 0;
		for(unsigned int k = 0; k < bits; k++) {
			unsigned int p = i * bits + k;
			v |= (uint32_t)((buf[p >> 3] >> (p & 7)) & 1) << k;
		}
		w[i] = (int32_t)v;
	}
}

static void _bitunpack(int32_t *w, const uint8_t *buf, uint32_t a, uint32_t b) {
	unsigned int bits = _bitlen(a + b);
	for(unsigned int i = 0; i < MP_MLDSA_N; i++) {
		uint32_t v = 0;
		for(unsigned int k = 0; k < bits; k++) {
			unsigned int p = i * bits + k;
			v |= (uint32_t)((buf[p >> 3] >> (p & 7)) & 1) << k;
		}
		w[i] = (int32_t)v - (int32_t)a;
	}
}

/* hint pack: k polynomials of 256 binary coeffs, at most omega ones total. */
static void _hintbitpack(uint8_t *out, const int32_t (*h)[MP_MLDSA_N]) {
	memset(out, 0, MP_MLDSA_OMEGA + MP_MLDSA_K);
	unsigned int idx = 0;
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		for(unsigned int j = 0; j < MP_MLDSA_N && idx < MP_MLDSA_OMEGA; j++) {
			if(h[i][j] != 0) { out[idx] = (uint8_t)j; idx++; }
		}
		out[MP_MLDSA_OMEGA + i] = (uint8_t)idx;
	}
}

/* hint unpack: returns 1 on success, 0 on malformed input. */
static int _hintbitunpack(int32_t (*h)[MP_MLDSA_N], const uint8_t *buf) {
	memset(h, 0, MP_MLDSA_K * MP_MLDSA_N * sizeof(int32_t));
	unsigned int idx = 0;
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		if(buf[MP_MLDSA_OMEGA + i] < idx || buf[MP_MLDSA_OMEGA + i] > MP_MLDSA_OMEGA)
			return 0;
		unsigned int first = idx;
		while(idx < buf[MP_MLDSA_OMEGA + i]) {
			if(idx > first && buf[idx - 1] >= buf[idx]) return 0;
			h[i][buf[idx]] = 1;
			idx++;
		}
	}
	for(; idx < MP_MLDSA_OMEGA; idx++)
		if(buf[idx] != 0) return 0;
	return 1;
}

/* ---- sampling (FIPS 204 §7.3) ---------------------------------------- *
 * G = SHAKE-128, H = SHAKE-256. Seeds are public (rho) or secret (rho', K),
 * but the squeeze loops only depend on lengths, so no secret branch. */

/* CoeffFromThreeBytes: z = 2^16*b2' + 2^8*b1 + b0, b2' = b2 with top bit cleared.
 * Returns z in [0,q) or -1 (reject). */
static int32_t _coeff3(uint8_t b0, uint8_t b1, uint8_t b2) {
	uint32_t z2 = b2;
	if(z2 > 127) z2 -= 128;
	uint32_t z = (z2 << 16) | ((uint32_t)b1 << 8) | b0;
	return (z < _Q) ? (int32_t)z : -1;
}

/* CoeffFromHalfByte for the secret-key range [-eta, eta]. Returns -1 on reject. */
static int32_t _coeffhalf(uint8_t b) {
	if(MP_MLDSA_ETA == 2) {
		if(b < 15) return 2 - (int32_t)(b % 5);
		return -1;
	}
	if(MP_MLDSA_ETA == 4) {
		if(b < 9) return 4 - (int32_t)b;
		return -1;
	}
	return -1;
}

/* RejNTTPoly: uniform in [0,q) via SHAKE-128 (G). seed is 34 bytes. */
static void _rejnttpoly(uint32_t *a, const uint8_t *seed) {
	mp_shake128_t ctx;
	mp_shake128_init(&ctx);
	mp_shake128_absorb(&ctx, seed, 34);
	mp_shake128_finalize(&ctx);
	unsigned int i = 0;
	while(i < MP_MLDSA_N) {
		uint8_t buf[3];
		mp_shake128_squeeze(&ctx, buf, 3);
		int32_t z = _coeff3(buf[0], buf[1], buf[2]);
		if(z >= 0) a[i++] = (uint32_t)z;
	}
}

/* RejBoundedPoly: coeffs in [-eta, eta] via SHAKE-256 (H). seed is 66 bytes. */
static void _rejboundedpoly(int32_t *a, const uint8_t *seed) {
	mp_shake256_t ctx;
	mp_shake256_init(&ctx);
	mp_shake256_absorb(&ctx, seed, 66);
	mp_shake256_finalize(&ctx);
	unsigned int i = 0;
	while(i < MP_MLDSA_N) {
		uint8_t b;
		mp_shake256_squeeze(&ctx, &b, 1);
		int32_t z0 = _coeffhalf(b & 0x0F);
		int32_t z1 = _coeffhalf(b >> 4);
		if(z0 >= 0) a[i++] = z0;
		if(z1 >= 0 && i < MP_MLDSA_N) a[i++] = z1;
	}
}

/* ExpandA: k x l matrix of NTT polys from rho (32 bytes). */
static void _expandA(uint32_t *Ahat, const uint8_t *rho) {
	uint8_t seed[34];
	memcpy(seed, rho, 32);
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		seed[32] = (uint8_t)i;
		for(unsigned int j = 0; j < MP_MLDSA_L; j++) {
			seed[33] = (uint8_t)j;
			_rejnttpoly(&Ahat[(i * MP_MLDSA_L + j) * MP_MLDSA_N], seed);
		}
	}
}

/* ExpandS: s1 (l polys) and s2 (k polys) from rho' (64 bytes). */
static void _expandS(int32_t *s1, int32_t *s2, const uint8_t *rhop) {
	uint8_t seed[66];
	memcpy(seed, rhop, 64);
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		seed[64] = (uint8_t)(i & 0xFF);
		seed[65] = (uint8_t)(i >> 8);
		_rejboundedpoly(&s1[i * MP_MLDSA_N], seed);
	}
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		unsigned int r = i + MP_MLDSA_L;
		seed[64] = (uint8_t)(r & 0xFF);
		seed[65] = (uint8_t)(r >> 8);
		_rejboundedpoly(&s2[i * MP_MLDSA_N], seed);
	}
}

/* ExpandMask: y (l polys), coeffs in [-(gamma1-1), gamma1], from rho'' (64)
 * and counter k. seed = rho'' || u16(k) || u8(i), 67 bytes. */
static void _expandmask(int32_t *y, const uint8_t *rhopp, unsigned int k) {
	uint8_t seed[67];
	memcpy(seed, rhopp, 64);
	seed[64] = (uint8_t)(k & 0xFF);
	seed[65] = (uint8_t)(k >> 8);
	uint32_t two_g1 = 2 * MP_MLDSA_GAMMA1;
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		seed[66] = (uint8_t)i;
		mp_shake256_t ctx;
		mp_shake256_init(&ctx);
		mp_shake256_absorb(&ctx, seed, 67);
		mp_shake256_finalize(&ctx);
		unsigned int n = 0;
		while(n < MP_MLDSA_N) {
			uint8_t buf[3];
			mp_shake256_squeeze(&ctx, buf, 3);
			uint32_t z2 = buf[2];
			if(z2 > 127) z2 -= 128;
			uint32_t z = (z2 << 16) | ((uint32_t)buf[1] << 8) | buf[0];
			if(z < two_g1) {
				int32_t c = (int32_t)z;
				if((uint32_t)c > MP_MLDSA_GAMMA1) c -= (int32_t)two_g1;
				y[i * MP_MLDSA_N + n] = c;
				n++;
			}
		}
	}
}

/* SampleInBall: poly with tau coefficients in {-1,1}, rest 0, from c~ (hash). */
static void _sampleinball(int32_t *c, const uint8_t *seed, unsigned int slen) {
	memset(c, 0, MP_MLDSA_N * sizeof(int32_t));
	mp_shake256_t ctx;
	mp_shake256_init(&ctx);
	mp_shake256_absorb(&ctx, seed, slen);
	mp_shake256_finalize(&ctx);
	uint8_t s[8];
	mp_shake256_squeeze(&ctx, s, 8);
	for(unsigned int i = MP_MLDSA_N - MP_MLDSA_TAU; i < MP_MLDSA_N; i++) {
		uint8_t b;
		do { mp_shake256_squeeze(&ctx, &b, 1); } while(b > i);
		c[i] = c[b];
		unsigned int bit = i - (MP_MLDSA_N - MP_MLDSA_TAU);
		int32_t sign = 1 - 2 * (int32_t)((s[bit >> 3] >> (bit & 7)) & 1);
		c[b] = sign;
	}
}

/* ---- key / signature encoding (FIPS 204 Alg 22–28) -------------------- */

#define _T1BITS   10                                   /* bitlen(q-1) - d = 23 - 13 */
#define _T1BOUND  ((1u << _T1BITS) - 1)                 /* 1023 */
#define _T1BYTES  (32 * _T1BITS)                        /* 320 bytes/poly */
#define _ETA2B   (32 * _bitlen(2 * MP_MLDSA_ETA))       /* s1/s2 poly bytes */
#define _T0A     ((1u << (MP_MLDSA_D - 1)) - 1)         /* 4095 */
#define _T0B     (1u << (MP_MLDSA_D - 1))               /* 4096 */
#define _T0BYTES (32 * _bitlen(_T0A + _T0B))            /* 416 bytes/poly */
#define _ZBYTES  (32 * _bitlen(2 * MP_MLDSA_GAMMA1 - 1))/* z poly bytes */

static void _pkencode(uint8_t *pk, const uint8_t *rho, const int32_t (*t1)[MP_MLDSA_N]) {
	memcpy(pk, rho, 32);
	uint8_t *p = pk + 32;
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		_simplebitpack(p, t1[i], _T1BOUND);
		p += _T1BYTES;
	}
}

static void _pkdecode(uint8_t *rho, int32_t (*t1)[MP_MLDSA_N], const uint8_t *pk) {
	memcpy(rho, pk, 32);
	const uint8_t *p = pk + 32;
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		_simplebitunpack(t1[i], p, _T1BOUND);
		p += _T1BYTES;
	}
}

static void _skencode(uint8_t *sk, const uint8_t *rho, const uint8_t *K,
		const uint8_t *tr, const int32_t *s1, const int32_t *s2,
		const int32_t (*t0)[MP_MLDSA_N]) {
	uint8_t *p = sk;
	memcpy(p, rho, 32); p += 32;
	memcpy(p, K, 32); p += 32;
	memcpy(p, tr, 64); p += 64;
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		_bitpack(p, &s1[i * MP_MLDSA_N], MP_MLDSA_ETA, MP_MLDSA_ETA);
		p += _ETA2B;
	}
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		_bitpack(p, &s2[i * MP_MLDSA_N], MP_MLDSA_ETA, MP_MLDSA_ETA);
		p += _ETA2B;
	}
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		_bitpack(p, t0[i], _T0A, _T0B);
		p += _T0BYTES;
	}
}

static void _skdecode(uint8_t *rho, uint8_t *K, uint8_t *tr,
		int32_t *s1, int32_t *s2, int32_t (*t0)[MP_MLDSA_N], const uint8_t *sk) {
	const uint8_t *p = sk;
	memcpy(rho, p, 32); p += 32;
	memcpy(K, p, 32); p += 32;
	memcpy(tr, p, 64); p += 64;
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		_bitunpack(&s1[i * MP_MLDSA_N], p, MP_MLDSA_ETA, MP_MLDSA_ETA);
		p += _ETA2B;
	}
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		_bitunpack(&s2[i * MP_MLDSA_N], p, MP_MLDSA_ETA, MP_MLDSA_ETA);
		p += _ETA2B;
	}
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		_bitunpack(t0[i], p, _T0A, _T0B);
		p += _T0BYTES;
	}
}

static void _sigencode(uint8_t *sig, const uint8_t *chat,
		const int32_t *z, const int32_t (*h)[MP_MLDSA_N]) {
	memcpy(sig, chat, MP_MLDSA_CHASH);
	uint8_t *p = sig + MP_MLDSA_CHASH;
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		_bitpack(p, &z[i * MP_MLDSA_N], MP_MLDSA_GAMMA1 - 1, MP_MLDSA_GAMMA1);
		p += _ZBYTES;
	}
	_hintbitpack(p, h);
}

static int _sigdecode(uint8_t *chat, int32_t *z, int32_t (*h)[MP_MLDSA_N],
		const uint8_t *sig) {
	memcpy(chat, sig, MP_MLDSA_CHASH);
	const uint8_t *p = sig + MP_MLDSA_CHASH;
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		_bitunpack(&z[i * MP_MLDSA_N], p, MP_MLDSA_GAMMA1 - 1, MP_MLDSA_GAMMA1);
		p += _ZBYTES;
	}
	return _hintbitunpack(h, p);
}

static void _w1encode(uint8_t *out, const int32_t (*w1)[MP_MLDSA_N]) {
	uint8_t *p = out;
	unsigned int wb = 32 * _bitlen(_HM - 1);
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		_simplebitpack(p, w1[i], _HM - 1);
		p += wb;
	}
}

/* ---- hash helpers (FIPS 204 §7.1: H = SHAKE-256, G = SHAKE-128) -------- */

/* (rho, rho', K) = H(xi || u8(k) || u8(l), 128) = 32 + 64 + 32 bytes. */
static void _h_seed(uint8_t *rho, uint8_t *rhop, uint8_t *K, const uint8_t *xi) {
	uint8_t seed[34];
	memcpy(seed, xi, 32);
	seed[32] = (uint8_t)MP_MLDSA_K;
	seed[33] = (uint8_t)MP_MLDSA_L;
	uint8_t out[128];
	mp_shake256(out, 128, seed, 34);
	memcpy(rho, out, 32);
	memcpy(rhop, out + 32, 64);
	memcpy(K, out + 96, 32);
}

/* mu = H(BytesToBits(tr) || M', 64); M' bytes = 0x00 || len(ctx) || ctx || M.
 * Empty context here, so M' = 0x00 || 0x00 || msg. */
static void _h_mu(uint8_t *mu, const uint8_t *tr, const uint8_t *msg, unsigned int mlen) {
	mp_shake256_t ctx;
	mp_shake256_init(&ctx);
	mp_shake256_absorb(&ctx, tr, 64);
	uint8_t pre[2] = { 0, 0 };
	mp_shake256_absorb(&ctx, pre, 2);
	mp_shake256_absorb(&ctx, msg, mlen);
	mp_shake256_finalize(&ctx);
	mp_shake256_squeeze(&ctx, mu, 64);
}

/* rho'' = H(K || rnd || mu, 64); deterministic signing uses rnd = 0^32. */
static void _h_rhopp(uint8_t *rhopp, const uint8_t *K, const uint8_t *mu) {
	mp_shake256_t ctx;
	mp_shake256_init(&ctx);
	mp_shake256_absorb(&ctx, K, 32);
	uint8_t rnd[32];
	memset(rnd, 0, 32);
	mp_shake256_absorb(&ctx, rnd, 32);
	mp_shake256_absorb(&ctx, mu, 64);
	mp_shake256_finalize(&ctx);
	mp_shake256_squeeze(&ctx, rhopp, 64);
}

/* c~ = H(rho || w1Encode(w1), ceil(lambda/4)). */
static void _h_chat(uint8_t *chat, const uint8_t *rho, const uint8_t *w1bytes, unsigned int w1len) {
	mp_shake256_t ctx;
	mp_shake256_init(&ctx);
	mp_shake256_absorb(&ctx, rho, 32);
	mp_shake256_absorb(&ctx, w1bytes, w1len);
	mp_shake256_finalize(&ctx);
	mp_shake256_squeeze(&ctx, chat, MP_MLDSA_CHASH);
}

/* ---- matrix-vector product in the NTT domain ------------------------- *
 * out[i] = sum_j Ahat[i][j] (circ) vhat[j], for i in [0,k), j in [0,l). */
static void _matvec_ntt(uint32_t *out, const uint32_t *Ahat, const uint32_t *vhat) {
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		uint32_t acc[MP_MLDSA_N];
		_pmul(acc, &Ahat[(i * MP_MLDSA_L + 0) * MP_MLDSA_N], &vhat[0 * MP_MLDSA_N]);
		for(unsigned int j = 1; j < MP_MLDSA_L; j++) {
			uint32_t tmp[MP_MLDSA_N];
			_pmul(tmp, &Ahat[(i * MP_MLDSA_L + j) * MP_MLDSA_N], &vhat[j * MP_MLDSA_N]);
			_padd(acc, acc, tmp);
		}
		memcpy(&out[i * MP_MLDSA_N], acc, MP_MLDSA_N * sizeof(uint32_t));
	}
}

/* ---- ML-DSA. KeyGen_internal (FIPS 204 Alg 6) ------------------------- */
mp_ret_t mp_mldsa_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	if(seedlen < 32)
		return FALSE;
	const uint8_t *xi = seed;
	uint8_t rho[32], rhop[64], K[32];
	_h_seed(rho, rhop, K, xi);

	static uint32_t Ahat[MP_MLDSA_K * MP_MLDSA_L * MP_MLDSA_N];
	_expandA(Ahat, rho);

	static int32_t s1[MP_MLDSA_L * MP_MLDSA_N];
	static int32_t s2[MP_MLDSA_K * MP_MLDSA_N];
	_expandS(s1, s2, rhop);

	static uint32_t s1hat[MP_MLDSA_L * MP_MLDSA_N];
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		for(unsigned int n = 0; n < MP_MLDSA_N; n++)
			s1hat[i * MP_MLDSA_N + n] = _modq_i(s1[i * MP_MLDSA_N + n]);
		_ntt(&s1hat[i * MP_MLDSA_N]);
	}

	static uint32_t thathat[MP_MLDSA_K * MP_MLDSA_N];
	_matvec_ntt(thathat, Ahat, s1hat);

	static int32_t t1[MP_MLDSA_K][MP_MLDSA_N];
	static int32_t t0[MP_MLDSA_K][MP_MLDSA_N];
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		uint32_t tmp[MP_MLDSA_N];
		memcpy(tmp, &thathat[i * MP_MLDSA_N], MP_MLDSA_N * sizeof(uint32_t));
		_intt(tmp);
		for(unsigned int n = 0; n < MP_MLDSA_N; n++) {
			uint32_t r = _addmod(tmp[n], _modq_i(s2[i * MP_MLDSA_N + n]));
			uint32_t hi; int32_t lo;
			_power2round(r, &hi, &lo);
			t1[i][n] = (int32_t)hi;
			t0[i][n] = lo;
		}
	}

	uint8_t *pk = pub;
	_pkencode(pk, rho, t1);

	uint8_t tr[64];
	mp_shake256(tr, 64, pk, MP_MLDSA_PK_SIZE);

	uint8_t *sk = priv;
	_skencode(sk, rho, K, tr, s1, s2, t0);
	return TRUE;
}

/* infinity norm of an int32 array of cnt coefficients. */
static int32_t _infnorm(const int32_t *a, unsigned int cnt) {
	int32_t m = 0;
	for(unsigned int i = 0; i < cnt; i++) {
		int32_t v = a[i];
		if(v < 0) v = -v;
		if(v > m) m = v;
	}
	return m;
}

/* ---- ML-DSA. Sign_internal (FIPS 204 Alg 7), deterministic ------------- */
unsigned int mp_mldsa_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig) {
	uint8_t rho[32], K[32], tr[64];
	static int32_t s1[MP_MLDSA_L * MP_MLDSA_N];
	static int32_t s2[MP_MLDSA_K * MP_MLDSA_N];
	static int32_t t0[MP_MLDSA_K][MP_MLDSA_N];
	_skdecode(rho, K, tr, s1, s2, t0, priv);

	static uint32_t s1hat[MP_MLDSA_L * MP_MLDSA_N];
	static uint32_t s2hat[MP_MLDSA_K * MP_MLDSA_N];
	static uint32_t t0hat[MP_MLDSA_K * MP_MLDSA_N];
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		for(unsigned int n = 0; n < MP_MLDSA_N; n++) s1hat[i * MP_MLDSA_N + n] = _modq_i(s1[i * MP_MLDSA_N + n]);
		_ntt(&s1hat[i * MP_MLDSA_N]);
	}
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		for(unsigned int n = 0; n < MP_MLDSA_N; n++) s2hat[i * MP_MLDSA_N + n] = _modq_i(s2[i * MP_MLDSA_N + n]);
		_ntt(&s2hat[i * MP_MLDSA_N]);
		for(unsigned int n = 0; n < MP_MLDSA_N; n++) t0hat[i * MP_MLDSA_N + n] = _modq_i(t0[i][n]);
		_ntt(&t0hat[i * MP_MLDSA_N]);
	}

	static uint32_t Ahat[MP_MLDSA_K * MP_MLDSA_L * MP_MLDSA_N];
	_expandA(Ahat, rho);

	uint8_t mu[64];
	_h_mu(mu, tr, msg, len);
	uint8_t rhopp[64];
	_h_rhopp(rhopp, K, mu);

	static int32_t z[MP_MLDSA_L * MP_MLDSA_N];
	static int32_t h[MP_MLDSA_K][MP_MLDSA_N];
	static uint8_t chat[MP_MLDSA_CHASH];
	unsigned int w1bytes = MP_MLDSA_K * 32 * _bitlen(_HM - 1);
	static uint8_t w1b[1024];

	for(unsigned int k = 0; ; k += MP_MLDSA_L) {
		if(k > 2000) return 0; /* rejection-sampling cap */
		static int32_t y[MP_MLDSA_L * MP_MLDSA_N];
		_expandmask(y, rhopp, k);

		static uint32_t yhat[MP_MLDSA_L * MP_MLDSA_N];
		for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
			for(unsigned int n = 0; n < MP_MLDSA_N; n++) yhat[i * MP_MLDSA_N + n] = _modq_i(y[i * MP_MLDSA_N + n]);
			_ntt(&yhat[i * MP_MLDSA_N]);
		}

		static uint32_t what[MP_MLDSA_K * MP_MLDSA_N];
		_matvec_ntt(what, Ahat, yhat);
		static uint32_t w[MP_MLDSA_K * MP_MLDSA_N];
		static int32_t w1[MP_MLDSA_K][MP_MLDSA_N];
		for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
			memcpy(&w[i * MP_MLDSA_N], &what[i * MP_MLDSA_N], MP_MLDSA_N * sizeof(uint32_t));
			_intt(&w[i * MP_MLDSA_N]);
			for(unsigned int n = 0; n < MP_MLDSA_N; n++) {
				w1[i][n] = (int32_t)_highbits(w[i * MP_MLDSA_N + n]);
			}
		}

		_w1encode(w1b, (const int32_t (*)[MP_MLDSA_N])w1);
		_h_chat(chat, rho, w1b, w1bytes);

		static int32_t c[MP_MLDSA_N];
		_sampleinball(c, chat, MP_MLDSA_CHASH);
		static uint32_t chat_hat[MP_MLDSA_N];
		for(unsigned int n = 0; n < MP_MLDSA_N; n++) chat_hat[n] = _modq_i(c[n]);
		_ntt(chat_hat);

		/* z = y + NTT^-1(chat_hat (circ) s1hat) ; r0 = LowBits(w - NTT^-1(chat_hat (circ) s2hat)) */
		int32_t zn = 0, r0n = 0;
		for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
			uint32_t tmp[MP_MLDSA_N];
			_pmul(tmp, chat_hat, &s1hat[i * MP_MLDSA_N]);
			_intt(tmp);
			for(unsigned int n = 0; n < MP_MLDSA_N; n++) {
				int32_t v = _cmod((int32_t)tmp[n] + y[i * MP_MLDSA_N + n]);
				z[i * MP_MLDSA_N + n] = v;
				if(v > zn) zn = v;
				if(-v > zn) zn = -v;
			}
		}
		for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
			uint32_t tmp[MP_MLDSA_N];
			_pmul(tmp, chat_hat, &s2hat[i * MP_MLDSA_N]);
			_intt(tmp);
			for(unsigned int n = 0; n < MP_MLDSA_N; n++) {
				int32_t s2p = _cmod((int32_t)tmp[n]);
				uint32_t r = _submod(w[i * MP_MLDSA_N + n], _modq_i(s2p));
				int32_t r0 = _lowbits(r);
				if(r0 > r0n) r0n = r0;
				if(-r0 > r0n) r0n = -r0;
			}
		}

		if(zn < (int32_t)(MP_MLDSA_GAMMA1 - MP_MLDSA_BETA) &&
		   r0n < (int32_t)(MP_MLDSA_GAMMA2 - MP_MLDSA_BETA)) {
			/* hint: h = MakeHint(-<t0>, w - <s2> + <t0>); check <t0> and weight */
			int32_t t0n = 0;
			unsigned int hw = 0;
			memset(h, 0, sizeof(h));
			for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
				uint32_t t0p_hat[MP_MLDSA_N];
				_pmul(t0p_hat, chat_hat, &t0hat[i * MP_MLDSA_N]);
				_intt(t0p_hat);
				uint32_t s2p_hat[MP_MLDSA_N];
				_pmul(s2p_hat, chat_hat, &s2hat[i * MP_MLDSA_N]);
				_intt(s2p_hat);
				for(unsigned int n = 0; n < MP_MLDSA_N; n++) {
					int32_t t0p = _cmod((int32_t)t0p_hat[n]);
					if(t0p > t0n) t0n = t0p;
					if(-t0p > t0n) t0n = -t0p;
					int32_t s2p = _cmod((int32_t)s2p_hat[n]);
					uint32_t r = _addmod(_submod(w[i * MP_MLDSA_N + n], _modq_i(s2p)), _modq_i(t0p));
					h[i][n] = (int32_t)_makehint(-t0p, r);
					if(h[i][n]) hw++;
				}
			}
		if(t0n < (int32_t)MP_MLDSA_GAMMA2 && hw <= MP_MLDSA_OMEGA) {
			_sigencode(sig, chat, z, (const int32_t (*)[MP_MLDSA_N])h);
			return MP_MLDSA_SIG_SIZE;
		}
		}
	}
}

/* ---- ML-DSA. Verify_internal (FIPS 204 Alg 8) ------------------------- */
mp_bool_t mp_mldsa_verify(const void *pub, const void *msg, unsigned int len,
		const void *sig, unsigned int siglen) {
	if(siglen != MP_MLDSA_SIG_SIZE)
		return NO;
	uint8_t rho[32];
	static int32_t t1[MP_MLDSA_K][MP_MLDSA_N];
	_pkdecode(rho, t1, pub);

	static uint8_t chat[MP_MLDSA_CHASH];
	static int32_t z[MP_MLDSA_L * MP_MLDSA_N];
	static int32_t h[MP_MLDSA_K][MP_MLDSA_N];
	if(!_sigdecode(chat, z, h, sig))
		return NO;

	static uint32_t Ahat[MP_MLDSA_K * MP_MLDSA_L * MP_MLDSA_N];
	_expandA(Ahat, rho);

	uint8_t tr[64];
	mp_shake256(tr, 64, pub, MP_MLDSA_PK_SIZE);
	uint8_t mu[64];
	_h_mu(mu, tr, msg, len);

	static int32_t c[MP_MLDSA_N];
	_sampleinball(c, chat, MP_MLDSA_CHASH);
	static uint32_t chat_hat[MP_MLDSA_N];
	for(unsigned int n = 0; n < MP_MLDSA_N; n++) chat_hat[n] = _modq_i(c[n]);
	_ntt(chat_hat);

	/* NTT(t1 * 2^d) */
	static uint32_t t1d_hat[MP_MLDSA_K * MP_MLDSA_N];
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		for(unsigned int n = 0; n < MP_MLDSA_N; n++)
			t1d_hat[i * MP_MLDSA_N + n] = _modq((uint64_t)t1[i][n] << MP_MLDSA_D);
		_ntt(&t1d_hat[i * MP_MLDSA_N]);
	}
	/* NTT(z) */
	static uint32_t zhat[MP_MLDSA_L * MP_MLDSA_N];
	for(unsigned int i = 0; i < MP_MLDSA_L; i++) {
		for(unsigned int n = 0; n < MP_MLDSA_N; n++) zhat[i * MP_MLDSA_N + n] = _modq_i(z[i * MP_MLDSA_N + n]);
		_ntt(&zhat[i * MP_MLDSA_N]);
	}
	/* w' = NTT^-1(ScalarVectorNTT(chat_hat, t1d_hat) - MatrixVectorNTT(Ahat, zhat)) */
	static uint32_t matvec[MP_MLDSA_K * MP_MLDSA_N];
	_matvec_ntt(matvec, Ahat, zhat);
	static uint32_t wphat[MP_MLDSA_K * MP_MLDSA_N];
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		uint32_t tmp[MP_MLDSA_N];
		_pmul(tmp, chat_hat, &t1d_hat[i * MP_MLDSA_N]);
		for(unsigned int n = 0; n < MP_MLDSA_N; n++)
			wphat[i * MP_MLDSA_N + n] = _submod(matvec[i * MP_MLDSA_N + n], tmp[n]);
	}
	static uint32_t wp[MP_MLDSA_K * MP_MLDSA_N];
	static int32_t w1p[MP_MLDSA_K][MP_MLDSA_N];
	for(unsigned int i = 0; i < MP_MLDSA_K; i++) {
		memcpy(&wp[i * MP_MLDSA_N], &wphat[i * MP_MLDSA_N], MP_MLDSA_N * sizeof(uint32_t));
		_intt(&wp[i * MP_MLDSA_N]);
		for(unsigned int n = 0; n < MP_MLDSA_N; n++)
			w1p[i][n] = (int32_t)_usehint((uint32_t)h[i][n], wp[i * MP_MLDSA_N + n]);
	}

	unsigned int w1bl = MP_MLDSA_K * 32 * _bitlen(_HM - 1);
	static uint8_t w1pb[1024];
	_w1encode(w1pb, (const int32_t (*)[MP_MLDSA_N])w1p);
	uint8_t chatp[MP_MLDSA_CHASH];
	_h_chat(chatp, rho, w1pb, w1bl);

	if(_infnorm(z, MP_MLDSA_L * MP_MLDSA_N) >= (int32_t)(MP_MLDSA_GAMMA1 - MP_MLDSA_BETA))
		return NO;
	if(memcmp(chat, chatp, MP_MLDSA_CHASH) != 0)
		return NO;
	return YES;
}

/**@}*/

#endif /* SUPPORT_COMMON_CRYPTO_SIGN_MLDSA */
