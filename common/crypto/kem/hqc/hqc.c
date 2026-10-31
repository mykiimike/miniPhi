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

#ifdef SUPPORT_COMMON_CRYPTO_KEM_HQC

/**
@defgroup mpCommonHqc HQC-128

@ingroup mpCommon

@brief Code-based key encapsulation, HQC-128

Vectors live in the ring of binary polynomials modulo x^17669-1. A product
by a secret sparse polynomial rotates the dense operand once per secret
position: the position picks the word with a scan, never with an index, and
the bit offset is a pair of 32-bit shifts. A 64-bit variable shift is what
a 32-bit compiler splits with a branch. The Reed-Solomon decoder always runs
thirty discrepancy steps and checks all forty-six positions. Field
multiplication is bit-serial. No logarithm table is indexed by a secret.
The scratch is shared, so one operation runs at a time.

@{
*/

#define _N 17669u
#define _NW 277
#define _N1 46
#define _MSG 16
#define _DELTA 15
#define _N1N2W 276
#define _UBYTES 2209u
#define _VBYTES 2208u
#define _UV (_UBYTES + _VBYTES)
#define _OMEGA 66u
#define _OMEGA_R 75u
#define _SEED 40u
#define _RED 0x1fu
#define _DOM_XOF 2u
#define _DOM_G 3u
#define _DOM_K 4u

/** generator of the shortened Reed-Solomon code, low coefficient first */
static const uint8_t _rs_g[31] = {
	89, 69, 153, 116, 176, 117, 111, 75, 73, 233, 242, 233, 65, 210, 21, 139,
	103, 173, 67, 118, 105, 210, 174, 110, 74, 69, 228, 82, 255, 181, 1
};

static uint64_t _v0[_NW], _v1[_NW], _v2[_NW], _v3[_NW], _v4[_NW], _v5[_NW], _v6[_NW];
static uint64_t _ext[278];
static uint32_t _sup[75], _sup2[75];
static uint8_t _bytes[MP_CRYPTO_HQC_CT];
static uint8_t _bytes2[MP_CRYPTO_HQC_CT];
static mp_crypto_shake256_t _xof;
static uint8_t _exp[255];
static uint8_t _exp_ready;

static void _wipe(void) {
	memset(_v0, 0, sizeof(_v0));
	memset(_v1, 0, sizeof(_v1));
	memset(_v2, 0, sizeof(_v2));
	memset(_v3, 0, sizeof(_v3));
	memset(_v4, 0, sizeof(_v4));
	memset(_v5, 0, sizeof(_v5));
	memset(_v6, 0, sizeof(_v6));
	memset(_ext, 0, sizeof(_ext));
	memset(_sup, 0, sizeof(_sup));
	memset(_sup2, 0, sizeof(_sup2));
	memset(_bytes, 0, sizeof(_bytes));
	memset(_bytes2, 0, sizeof(_bytes2));
	memset(&_xof, 0, sizeof(_xof));
}

static uint8_t _gf_mul(uint8_t a, uint8_t b) {
	uint8_t p = 0;
	int i;

	for(i = 0; i < 8; i++) {
		uint8_t m = (uint8_t)-(b & 1u);
		uint8_t hi = (uint8_t)-(a >> 7);

		p ^= (uint8_t)(a & m);
		a = (uint8_t)((a << 1) ^ (0x1du & hi));
		b >>= 1;
	}
	return(p);
}

static uint8_t _gf_inv(uint8_t a) {
	uint8_t r = 1, b = a;
	int i;

	for(i = 0; i < 7; i++) {
		b = _gf_mul(b, b);
		r = _gf_mul(r, b);
	}
	return(r);
}

static void _gf_table(void) {
	int i;

	if(_exp_ready)
		return;
	_exp[0] = 1;
	for(i = 1; i < 255; i++)
		_exp[i] = _gf_mul(_exp[i - 1], 2);
	_exp_ready = 1;
}

static uint8_t _gf_eval(const uint8_t *poly, int deg, uint8_t x) {
	uint8_t y = 0;
	int i;

	for(i = deg; i >= 0; i--)
		y = (uint8_t)(_gf_mul(y, x) ^ poly[i]);
	return(y);
}

static void _rs_encode(uint8_t cdw[_N1], const uint8_t msg[_MSG]) {
	int i, j, k;

	memset(cdw, 0, _N1);
	for(i = 0; i < _MSG; i++) {
		uint8_t gate = (uint8_t)(msg[_MSG - 1 - i] ^ cdw[_N1 - _MSG - 1]);
		uint8_t tmp[31];

		for(j = 0; j < 31; j++)
			tmp[j] = _gf_mul(gate, _rs_g[j]);
		for(k = _N1 - _MSG - 1; k; k--)
			cdw[k] = (uint8_t)(cdw[k - 1] ^ tmp[k]);
		cdw[0] = tmp[0];
	}
	memcpy(cdw + _N1 - _MSG, msg, _MSG);
}

/** 0xffff when a <= b. a and b are the public loop counters or the degree. */
static uint16_t _mask_le(unsigned a, unsigned b) {
	return((uint16_t)-((uint16_t)((uint16_t)a - (uint16_t)b - 1u) >> 15));
}

static uint16_t _mask_nz(uint16_t d) {
	return((uint16_t)-((uint16_t)((uint16_t)-d >> 15)));
}

static void _rs_syndrome(uint8_t S[30], const uint8_t *c) {
	int i, j;

	for(i = 0; i < 30; i++) {
		uint8_t x = _exp[(i + 1) % 255];
		uint8_t y = 0;

		for(j = _N1 - 1; j >= 0; j--)
			y = (uint8_t)(_gf_mul(y, x) ^ c[j]);
		S[i] = y;
	}
}

/** Berlekamp, thirty steps, every coefficient visited. */
static void _rs_elp(uint8_t sigma[16], const uint8_t S[30]) {
	uint16_t sig[16], X[16], copy[16];
	uint16_t deg = 0, deg_p = 0, pp = 0xffffu, d_p = 1, d;
	int mu, i;

	memset(sig, 0, sizeof(sig));
	memset(X, 0, sizeof(X));
	sig[0] = 1;
	X[1] = 1;
	d = S[0];
	for(mu = 0; mu < 30; mu++) {
		uint16_t deg_copy = deg;
		uint16_t dd = _gf_mul((uint8_t)d, _gf_inv((uint8_t)d_p));
		uint16_t deg_X = (uint16_t)((uint16_t)mu - pp);
		uint16_t deg_Xsp = (uint16_t)(deg_X + deg_p);
		uint16_t mask1 = _mask_nz(d);
		uint16_t mask2 = (uint16_t)-((uint16_t)((uint16_t)(deg - deg_Xsp) >> 15));
		uint16_t mask12 = (uint16_t)(mask1 & mask2);

		memcpy(copy, sig, sizeof(sig));
		for(i = 1; i <= _DELTA; i++) {
			uint16_t use = _mask_le((unsigned)i, (unsigned)mu + 1u);

			sig[i] ^= (uint16_t)_gf_mul((uint8_t)dd, (uint8_t)X[i]) & use;
		}
		deg = (uint16_t)(deg ^ (mask12 & (deg_Xsp ^ deg)));
		if(mu == 29)
			break;
		pp = (uint16_t)(pp ^ (mask12 & ((uint16_t)mu ^ pp)));
		d_p = (uint16_t)(d_p ^ (mask12 & (d ^ d_p)));
		for(i = _DELTA; i > 0; i--) {
			uint16_t left = (uint16_t)(mask12 & copy[i - 1]);
			uint16_t right = (uint16_t)(((uint16_t)~mask12) & X[i - 1]);

			X[i] = (uint16_t)(left ^ right);
		}
		deg_p = (uint16_t)(deg_p ^ (mask12 & (deg_copy ^ deg_p)));
		d = S[mu + 1];
		for(i = 1; i <= _DELTA; i++) {
			uint16_t use = _mask_le((unsigned)i, (unsigned)mu + 1u);

			if(i <= mu + 1)
				d ^= (uint16_t)_gf_mul((uint8_t)(sig[i] & use), S[mu + 1 - i]);
		}
	}
	for(i = 0; i < 16; i++)
		sigma[i] = (uint8_t)sig[i];
	(void)deg;
}

static void _rs_decode(uint8_t msg[_MSG], uint8_t *cdw) {
	uint8_t S[30], sigma[16], w[30];
	int i, k, j;

	_gf_table();
	_rs_syndrome(S, cdw);
	_rs_elp(sigma, S);
	memset(w, 0, sizeof(w));
	for(i = 0; i <= _DELTA; i++) {
		for(k = 0; k < 30 - i; k++)
			w[i + k] ^= _gf_mul(sigma[i], S[k]);
	}
	for(j = 0; j < _N1; j++) {
		uint8_t x = _exp[(255 - j) % 255];
		uint8_t y = _gf_eval(sigma, _DELTA, x);
		uint8_t num = _gf_eval(w, 29, x);
		uint8_t den = 0, xp = 1, e, m;
		int t;

		for(t = 0; t < _DELTA; t++) {
			if((t + 1) & 1)
				den ^= _gf_mul(sigma[t + 1], xp);
			xp = _gf_mul(xp, x);
		}
		e = _gf_mul(num, _gf_inv(den));
		m = (uint8_t)(((y | (uint8_t)-y) >> 7) ^ 1u);
		cdw[j] ^= (uint8_t)(e & (uint8_t)-m);
	}
	memcpy(msg, cdw + _N1 - _MSG, _MSG);
}

static void _rm_byte(uint64_t *cword, uint8_t message) {
	uint32_t first = (uint32_t)-((message >> 7) & 1u);

	first ^= (uint32_t)-((message >> 0) & 1u) & 0xaaaaaaaau;
	first ^= (uint32_t)-((message >> 1) & 1u) & 0xccccccccu;
	first ^= (uint32_t)-((message >> 2) & 1u) & 0xf0f0f0f0u;
	first ^= (uint32_t)-((message >> 3) & 1u) & 0xff00ff00u;
	first ^= (uint32_t)-((message >> 4) & 1u) & 0xffff0000u;
	cword[0] = first;
	first ^= (uint32_t)-((message >> 5) & 1u);
	cword[0] |= (uint64_t)first << 32;
	first ^= (uint32_t)-((message >> 6) & 1u);
	cword[1] = (uint64_t)first << 32;
	first ^= (uint32_t)-((message >> 5) & 1u);
	cword[1] |= first;
}

static void _hadamard(uint16_t src[128], uint16_t dst[128]) {
	uint16_t *p1 = src, *p2 = dst, *p3;
	int pass, i;

	for(pass = 0; pass < 7; pass++) {
		for(i = 0; i < 64; i++) {
			p2[i] = (uint16_t)(p1[2 * i] + p1[2 * i + 1]);
			p2[i + 64] = (uint16_t)(p1[2 * i] - p1[2 * i + 1]);
		}
		p3 = p1;
		p1 = p2;
		p2 = p3;
	}
}

static uint8_t _peaks(const uint16_t transform[128]) {
	uint16_t peak_abs = 0, peak = 0, pos = 0;
	int i;

	for(i = 0; i < 128; i++) {
		uint16_t t = transform[i];
		uint16_t sign = (uint16_t)-(t >> 15);
		uint16_t abs = (uint16_t)(t ^ (sign & (t ^ (uint16_t)-t)));
		uint16_t mask = (uint16_t)-((uint16_t)(peak_abs - abs) >> 15);

		peak ^= (uint16_t)(mask & (peak ^ t));
		pos ^= (uint16_t)(mask & (pos ^ (uint16_t)i));
		peak_abs ^= (uint16_t)(mask & (peak_abs ^ abs));
	}
	pos |= (uint16_t)(128u & (uint16_t)((peak >> 15) - 1));
	return((uint8_t)pos);
}

static void _rm_encode(uint64_t *cdw, const uint8_t *msg) {
	int i, copy;

	memset(cdw, 0, _N1N2W * 8);
	for(i = 0; i < _N1; i++) {
		_rm_byte(&cdw[6 * i], msg[i]);
		for(copy = 1; copy < 3; copy++) {
			cdw[6 * i + 2 * copy] = cdw[6 * i];
			cdw[6 * i + 2 * copy + 1] = cdw[6 * i + 1];
		}
	}
}

static void _rm_decode(uint8_t *msg, const uint64_t *cdw) {
	int i, copy, part, bit;

	for(i = 0; i < _N1; i++) {
		uint16_t expanded[128], transform[128];
		const uint64_t *src = &cdw[6 * i];

		for(part = 0; part < 2; part++) {
			for(bit = 0; bit < 64; bit++)
				expanded[part * 64 + bit] = (uint16_t)((src[part] >> bit) & 1u);
		}
		for(copy = 1; copy < 3; copy++) {
			for(part = 0; part < 2; part++) {
				for(bit = 0; bit < 64; bit++)
					expanded[part * 64 + bit] = (uint16_t)(expanded[part * 64 + bit] + ((src[2 * copy + part] >> bit) & 1u));
			}
		}
		_hadamard(expanded, transform);
		transform[0] = (uint16_t)(transform[0] - 64u * 3u);
		msg[i] = _peaks(transform);
	}
}

static void _code_encode(uint64_t *out, const uint8_t msg[_MSG]) {
	uint8_t rs[_N1];

	_rs_encode(rs, msg);
	_rm_encode(out, rs);
}

static void _code_decode(uint8_t msg[_MSG], const uint64_t *in) {
	uint8_t rs[_N1];

	_rm_decode(rs, in);
	_rs_decode(msg, rs);
}

static void _xof_init(const uint8_t *seed, unsigned len) {
	uint8_t dom = _DOM_XOF;

	mp_crypto_shake256_init(&_xof);
	mp_crypto_shake256_absorb(&_xof, seed, len);
	mp_crypto_shake256_absorb(&_xof, &dom, 1);
	mp_crypto_shake256_finalize(&_xof);
}

/** The expander yields full 8-byte blocks. A short tail still consumes the block. */
static void _squeeze(uint8_t *out, unsigned len) {
	unsigned whole = len & ~7u;
	unsigned rem = len & 7u;
	uint8_t tmp[8];

	if(whole)
		mp_crypto_shake256_squeeze(&_xof, out, whole);
	if(rem) {
		mp_crypto_shake256_squeeze(&_xof, tmp, 8);
		memcpy(out + whole, tmp, rem);
		memset(tmp, 0, sizeof(tmp));
	}
}

static void _ds(uint8_t *out, unsigned outlen, const uint8_t *in, unsigned inlen, uint8_t dom) {
	mp_crypto_shake256_t c;

	mp_crypto_shake256_init(&c);
	mp_crypto_shake256_absorb(&c, in, inlen);
	mp_crypto_shake256_absorb(&c, &dom, 1);
	mp_crypto_shake256_finalize(&c);
	mp_crypto_shake256_squeeze(&c, out, outlen);
	memset(&c, 0, sizeof(c));
}

static void _load(uint64_t *o, const uint8_t *in, unsigned nbytes) {
	unsigned i;

	memset(o, 0, _NW * 8);
	for(i = 0; i < nbytes; i++)
		o[i >> 3] |= (uint64_t)in[i] << (8u * (i & 7u));
	o[_NW - 1] &= _RED;
}

static void _store(uint8_t *out, unsigned nbytes, const uint64_t *in) {
	unsigned i;

	for(i = 0; i < nbytes; i++)
		out[i] = (uint8_t)(in[i >> 3] >> (8u * (i & 7u)));
}

static void _xor(uint64_t *o, const uint64_t *a, const uint64_t *b) {
	int i;

	for(i = 0; i < _NW; i++)
		o[i] = a[i] ^ b[i];
	o[_NW - 1] &= _RED;
}

static uint32_t _eq32(uint32_t a, uint32_t b) {
	return(1u ^ (((a - b) | (b - a)) >> 31));
}

static uint64_t _bitmask(uint32_t pos) {
	uint64_t ret = 0, mask = 1;
	int i;

	for(i = 0; i < 64; i++) {
		uint64_t diff = (uint64_t)pos - (uint64_t)i;
		uint64_t nz = (diff | (0ull - diff)) >> 63;
		uint64_t eq = 0ull - (1ull - nz);

		ret |= mask & eq;
		mask <<= 1;
	}
	return(ret);
}

/** floor(2^32 / (17669 - i)) for i in 0..74. The index is the public counter. */
static const uint32_t _mu[75] = {
	243079, 243093, 243106, 243120, 243134, 243148, 243161, 243175,
	243189, 243203, 243216, 243230, 243244, 243258, 243272, 243285,
	243299, 243313, 243327, 243340, 243354, 243368, 243382, 243396,
	243409, 243423, 243437, 243451, 243465, 243478, 243492, 243506,
	243520, 243534, 243547, 243561, 243575, 243589, 243603, 243616,
	243630, 243644, 243658, 243672, 243686, 243699, 243713, 243727,
	243741, 243755, 243769, 243782, 243796, 243810, 243824, 243838,
	243852, 243865, 243879, 243893, 243907, 243921, 243935, 243949,
	243962, 243976, 243990, 244004, 244018, 244032, 244046, 244059,
	244073, 244087, 244101,
};

/** A shift count the compiler must not range-check. A known 0..63 becomes a branch. */
static uint32_t _opaque(uint32_t x) {
	__asm__ volatile("" : "+r"(x));
	return(x);
}

static uint32_t _reduce(uint32_t a, unsigned i) {
	uint32_t m = _mu[i];
	uint32_t n = _N - (uint32_t)i;
	uint32_t q = (uint32_t)(((uint64_t)a * m) >> 32);
	uint32_t r = a - q * n;
	uint32_t mask;

	r -= n;
	mask = 0u - (r >> 31);
	return(r + (n & mask));
}

/** Algorithm 5 of eprint 2021/1631. weight is public, 66 or 75. */
static void _fixed(uint32_t *support, uint64_t *v, unsigned weight) {
	uint8_t rnd[300];
	unsigned i, j;

	memset(v, 0, _NW * 8);
	_squeeze(rnd, 4u * weight);
	for(i = 0; i < weight; i++) {
		uint32_t a = (uint32_t)rnd[4 * i]
			| ((uint32_t)rnd[4 * i + 1] << 8)
			| ((uint32_t)rnd[4 * i + 2] << 16)
			| ((uint32_t)rnd[4 * i + 3] << 24);

		support[i] = (uint32_t)i + _reduce(a, i);
	}
	for(i = weight - 1; i-- > 0;) {
		uint32_t found = 0;

		for(j = i + 1; j < weight; j++)
			found |= _eq32(support[j], support[i]);
		{
			uint32_t mask = 0u - found;

			support[i] = (mask & i) ^ (~mask & support[i]);
		}
	}
	for(i = 0; i < _NW; i++) {
		uint64_t val = 0;

		for(j = 0; j < weight; j++) {
			uint32_t index = support[j] >> 6;
			uint32_t diff = i - index;
			uint32_t eq = 1u ^ ((diff | (0u - diff)) >> 31);
			uint64_t mask = 0ull - (uint64_t)eq;

			val |= _bitmask(support[j] & 63u) & mask;
		}
		v[i] = val;
	}
	v[_NW - 1] &= _RED;
	memset(rnd, 0, sizeof(rnd));
}

static void _random(uint64_t *v) {
	uint8_t rnd[_UBYTES];

	_squeeze(rnd, _UBYTES);
	_load(v, rnd, _UBYTES);
	memset(rnd, 0, sizeof(rnd));
}

static uint64_t _pick(const uint64_t *a, uint32_t idx) {
	uint64_t r = 0;
	uint32_t j;

	for(j = 0; j < 278; j++) {
		uint32_t diff = j ^ idx;
		uint32_t nz = (diff | (uint32_t)-diff) >> 31;
		uint64_t mask = 0ull - (uint64_t)(1u - nz);

		r |= a[j] & mask;
	}
	return(r);
}

/** Logical right shift by s in 0..63, in 32-bit operations only. */
static void _shr64(uint32_t *lo, uint32_t *hi, uint32_t s) {
	uint32_t l, nz, cross, ge, lo_s, hi_s;

	s = _opaque(s);
	l = s & 31u;
	nz = (l | (0u - l)) >> 31;
	cross = (*hi << ((0u - l) & 31u)) & (0u - nz);
	lo_s = (*lo >> l) | cross;
	hi_s = *hi >> l;
	ge = 0u - (s >> 5);
	*lo = (lo_s & ~ge) | ((*hi >> l) & ge);
	*hi = hi_s & ~ge;
}

/** Logical left shift by s in 0..64. A shift of 64 yields zero. */
static void _shl64(uint32_t *lo, uint32_t *hi, uint32_t s) {
	uint32_t l, nz, cross, ge, hi_s, kill;

	s = _opaque(s);
	l = s & 31u;
	nz = (l | (0u - l)) >> 31;
	cross = (*lo >> ((0u - l) & 31u)) & (0u - nz);
	hi_s = (*hi << l) | cross;
	ge = 0u - (s >> 5);
	*hi = (hi_s & ~ge) | ((*lo << l) & ge);
	*lo = (*lo << l) & ~ge;
	kill = 0u - (s >> 6);
	*lo &= ~kill;
	*hi &= ~kill;
}

/** 64 bits of the n-bit cycle, starting at secret bit s. */
static uint64_t _window(uint32_t s) {
	uint32_t w = s >> 6;
	uint32_t b = s & 63u;
	uint64_t lo = _pick(_ext, w);
	uint64_t hi = _pick(_ext, w + 1u);
	uint32_t lo0 = (uint32_t)lo;
	uint32_t lo1 = (uint32_t)(lo >> 32);
	uint32_t hi0 = (uint32_t)hi;
	uint32_t hi1 = (uint32_t)(hi >> 32);

	_shr64(&lo0, &lo1, b);
	_shl64(&hi0, &hi1, 64u - b);
	return((uint64_t)(lo0 | hi0) | ((uint64_t)(lo1 | hi1) << 32));
}

static void _extend(const uint64_t *a) {
	int i;

	for(i = 0; i < _NW; i++)
		_ext[i] = a[i];
	_ext[_NW - 1] = (a[_NW - 1] & _RED) | (a[0] << 5);
	_ext[_NW] = a[0] >> 59;
}

/** o = dense * x^{support[0]} + ... modulo x^n - 1. o may alias dense. */
static void _mul(uint64_t *o, const uint64_t *dense, const uint32_t *support, unsigned weight) {
	unsigned i, w;

	_extend(dense);
	for(w = 0; w < _NW; w++)
		o[w] = 0;
	for(i = 0; i < weight; i++) {
		uint32_t pos = support[i];

		for(w = 0; w < _NW; w++) {
			uint32_t src = w * 64u + (_N - pos);
			uint32_t over = 1u ^ (uint32_t)(((uint64_t)src - _N) >> 63);

			src -= _N & (0u - over);
			o[w] ^= _window(src);
		}
	}
	o[_NW - 1] &= _RED;
}

static void _pke_encrypt(uint8_t *uv, const uint8_t m[_MSG], const uint8_t theta[64], const uint8_t *pk) {
	_load(_v1, pk + _SEED, _UBYTES);
	_xof_init(pk, _SEED);
	_random(_v0);
	_xof_init(theta, _SEED);
	_fixed(_sup2, _v2, _OMEGA_R);
	_fixed(_sup, _v4, _OMEGA_R);
	_fixed(_sup2, _v3, _OMEGA_R);
	_mul(_v5, _v0, _sup, _OMEGA_R);
	_xor(_v5, _v5, _v2);
	_code_encode(_v4, m);
	_v4[_NW - 1] = 0;
	_mul(_v6, _v1, _sup, _OMEGA_R);
	_xor(_v6, _v6, _v3);
	_xor(_v6, _v6, _v4);
	_store(uv, _UBYTES, _v5);
	_store(uv + _UBYTES, _VBYTES, _v6);
}

static void _pke_decrypt(uint8_t m[_MSG], const uint8_t *sk, const uint8_t *ct) {
	_load(_v0, ct, _UBYTES);
	_load(_v1, ct + _UBYTES, _VBYTES);
	_xof_init(sk, _SEED);
	_fixed(_sup2, _v2, _OMEGA);
	_fixed(_sup, _v3, _OMEGA);
	_mul(_v2, _v0, _sup, _OMEGA);
	_xor(_v2, _v2, _v1);
	_code_decode(m, _v2);
}

static uint8_t _neq(const uint8_t *a, const uint8_t *b, unsigned n) {
	unsigned i;
	uint8_t d = 0;

	for(i = 0; i < n; i++)
		d |= (uint8_t)(a[i] ^ b[i]);
	return((uint8_t)((d | (uint8_t)-d) >> 7));
}

static void _cmov(uint8_t *dst, const uint8_t *src, unsigned n, uint8_t bit) {
	unsigned i;
	uint8_t m = (uint8_t)-(uint8_t)_opaque(bit);

	for(i = 0; i < n; i++)
		dst[i] ^= (uint8_t)(m & (dst[i] ^ src[i]));
}

/**
 * @brief Build an HQC-128 key pair
 * @param[in] seed 40 bytes of secret seed, 16 of sigma, 40 of public seed
 * @param[in] seedlen Length of seed
 * @param[out] priv MP_CRYPTO_HQC_PRIV bytes
 * @param[out] pub MP_CRYPTO_HQC_PUB bytes
 * @return FALSE when the seed is shorter than 96 bytes
 */
mp_ret_t mp_crypto_hqc_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	const uint8_t *in = seed;
	uint8_t *pk = pub;
	uint8_t *sk = priv;

	if(!seed || !priv || !pub || seedlen < MP_CRYPTO_HQC_SEED)
		return(FALSE);
	_xof_init(in, _SEED);
	_fixed(_sup2, _v2, _OMEGA);
	_fixed(_sup, _v3, _OMEGA);
	_xof_init(in + _SEED + _MSG, _SEED);
	_random(_v0);
	_mul(_v1, _v0, _sup, _OMEGA);
	_xor(_v1, _v1, _v2);
	memcpy(pk, in + _SEED + _MSG, _SEED);
	_store(pk + _SEED, _UBYTES, _v1);
	memcpy(sk, in, _SEED);
	memcpy(sk + _SEED, in + _SEED, _MSG);
	memcpy(sk + _SEED + _MSG, pk, MP_CRYPTO_HQC_PUB);
	_wipe();
	return(TRUE);
}

/**
 * @brief Encapsulate under an HQC-128 public key
 * @param[in] pub MP_CRYPTO_HQC_PUB bytes
 * @param[in] coins 16-byte message then 16-byte salt
 * @param[in] coinslen Length of coins
 * @param[out] ss 64-byte shared secret
 * @param[out] ct Ciphertext
 * @return The ciphertext length, or 0
 */
unsigned int mp_crypto_hqc_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct) {
	const uint8_t *in = coins;
	uint8_t theta[64];

	if(!pub || !coins || !ss || !ct || coinslen < MP_CRYPTO_HQC_COINS)
		return(0);
	memcpy(_bytes, in, _MSG);
	memcpy(_bytes + _MSG, pub, MP_CRYPTO_HQC_PUB);
	memcpy(_bytes + _MSG + MP_CRYPTO_HQC_PUB, in + _MSG, 16);
	_ds(theta, 64, _bytes, _MSG + MP_CRYPTO_HQC_PUB + 16, _DOM_G);
	_pke_encrypt(ct, in, theta, pub);
	memcpy(ct + _UV, in + _MSG, 16);
	memcpy(_bytes, in, _MSG);
	memcpy(_bytes + _MSG, ct, _UV);
	_ds(ss, 64, _bytes, _MSG + _UV, _DOM_K);
	memset(theta, 0, sizeof(theta));
	_wipe();
	return(MP_CRYPTO_HQC_CT);
}

/**
 * @brief Decapsulate an HQC-128 ciphertext
 * @param[in] priv MP_CRYPTO_HQC_PRIV bytes
 * @param[in] ct Ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss 64-byte shared secret
 * @return YES when ctlen matches
 */
mp_bool_t mp_crypto_hqc_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss) {
	const uint8_t *sk = priv;
	const uint8_t *pk = sk + _SEED + _MSG;
	uint8_t m[_MSG], sigma[_MSG], salt[16], theta[64];
	uint8_t bad;

	if(!priv || !ct || !ss || ctlen != MP_CRYPTO_HQC_CT)
		return(NO);
	memcpy(sigma, sk + _SEED, _MSG);
	memcpy(salt, (const uint8_t *)ct + _UV, 16);
	_pke_decrypt(m, sk, ct);
	memcpy(_bytes, m, _MSG);
	memcpy(_bytes + _MSG, pk, MP_CRYPTO_HQC_PUB);
	memcpy(_bytes + _MSG + MP_CRYPTO_HQC_PUB, salt, 16);
	_ds(theta, 64, _bytes, _MSG + MP_CRYPTO_HQC_PUB + 16, _DOM_G);
	_pke_encrypt(_bytes2, m, theta, pk);
	bad = _neq(_bytes2, ct, _UV);
	_cmov(m, sigma, _MSG, bad);
	memcpy(_bytes, m, _MSG);
	memcpy(_bytes + _MSG, ct, _UV);
	_ds(ss, 64, _bytes, _MSG + _UV, _DOM_K);
	memset(m, 0, sizeof(m));
	memset(sigma, 0, sizeof(sigma));
	memset(theta, 0, sizeof(theta));
	_wipe();
	return(YES);
}

/**
 * @brief Decapsulate with a fuse slot
 * @param[in] fuse Slot of role MP_CRYPTO_FUSE_HQC
 * @param[in] wrapped Blob the arch defines
 * @param[in] wrappedlen Byte length of wrapped
 * @param[in] ct Ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss 64-byte shared secret
 * @return NO when this series has no HQC accelerator
 */
mp_bool_t mp_crypto_hqc_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss) {
	static mp_bool_t said;

	if(!ss || (ctlen && !ct))
		return(NO);
#ifdef MP_ARCH_HQC
	mp_bool_t ok;

#ifdef SUPPORT_COMMON_CRYPTO_FUSE
	if(mp_crypto_fuse_check(fuse, MP_CRYPTO_FUSE_HQC) != YES)
		return(NO);
#endif
	ok = mp_arch_hqc_decaps_fuse(fuse, wrapped, wrappedlen, ct, ctlen, ss);
	if(ok != YES && said == NO) {
		mp_printk("HQC: the accelerator refused the fuse");
		said = YES;
	}
	return(ok);
#else
	(void)fuse;
	(void)wrapped;
	(void)wrappedlen;
	if(said == NO) {
		mp_printk("HQC: this build has no fuse");
		said = YES;
	}
	return(NO);
#endif
}

/**@}*/

#endif
