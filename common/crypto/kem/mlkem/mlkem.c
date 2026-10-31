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

#ifdef SUPPORT_COMMON_CRYPTO_KEM_MLKEM

/**
@defgroup mpCommonMlkem ML-KEM

@ingroup mpCommon

@brief ML-KEM-512, ML-KEM-768 and ML-KEM-1024 (FIPS 203)

The parameter set is MP_CRYPTO_MLKEM_LEVEL. Arithmetic is a fixed function
of the whole state: reduction is a Barrett multiply, and the NTT and
compression never branch on a secret or index a table with one. The matrix
is sampled from the public seed rho, so its rejection sampling may take a
variable number of blocks. The buffers below are shared. One operation runs
at a time.

@{
*/

#define _Q 3329
#define _N 256
#define _K MP_CRYPTO_MLKEM_K
#define _ETA1 MP_CRYPTO_MLKEM_ETA1
#define _ETA2 MP_CRYPTO_MLKEM_ETA2
#define _DU MP_CRYPTO_MLKEM_DU
#define _DV MP_CRYPTO_MLKEM_DV
#define _HALF (_Q / 2)
/** floor(2^32 / 3329). One correction covers every product this file builds. */
#define _BARRETT 1290167u

/** zeta^{BitRev7(i)} mod q. Index 0 is zeta^0 and is not read by the NTT. */
static const int16_t _zeta[128] = {
	1, 1729, 2580, 3289, 2642, 630, 1897, 848, 1062, 1919, 193, 797,
	2786, 3260, 569, 1746, 296, 2447, 1339, 1476, 3046, 56, 2240, 1333,
	1426, 2094, 535, 2882, 2393, 2879, 1974, 821, 289, 331, 3253, 1756,
	1197, 2304, 2277, 2055, 650, 1977, 2513, 632, 2865, 33, 1320, 1915,
	2319, 1435, 807, 452, 1438, 2868, 1534, 2402, 2647, 2617, 1481, 648,
	2474, 3110, 1227, 910, 17, 2761, 583, 2649, 1637, 723, 2288, 1100,
	1409, 2662, 3281, 233, 756, 2156, 3015, 3050, 1703, 1651, 2789, 1789,
	1847, 952, 1461, 2687, 939, 2308, 2437, 2388, 733, 2337, 268, 641,
	1584, 2298, 2037, 3220, 375, 2549, 2090, 1645, 1063, 319, 2773, 757,
	2099, 561, 2466, 2594, 2804, 1092, 403, 1026, 1143, 2150, 2775, 886,
	1722, 1212, 1874, 1029, 2110, 2935, 885, 2154,
};

/** zeta^{2*BitRev7(i)+1} mod q, the base-case multiplier of pair i. */
static const int16_t _gamma[128] = {
	17, 3312, 2761, 568, 583, 2746, 2649, 680, 1637, 1692, 723, 2606,
	2288, 1041, 1100, 2229, 1409, 1920, 2662, 667, 3281, 48, 233, 3096,
	756, 2573, 2156, 1173, 3015, 314, 3050, 279, 1703, 1626, 1651, 1678,
	2789, 540, 1789, 1540, 1847, 1482, 952, 2377, 1461, 1868, 2687, 642,
	939, 2390, 2308, 1021, 2437, 892, 2388, 941, 733, 2596, 2337, 992,
	268, 3061, 641, 2688, 1584, 1745, 2298, 1031, 2037, 1292, 3220, 109,
	375, 2954, 2549, 780, 2090, 1239, 1645, 1684, 1063, 2266, 319, 3010,
	2773, 556, 757, 2572, 2099, 1230, 561, 2768, 2466, 863, 2594, 735,
	2804, 525, 1092, 2237, 403, 2926, 1026, 2303, 1143, 2186, 2150, 1179,
	2775, 554, 886, 2443, 1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300,
	2110, 1219, 2935, 394, 885, 2444, 2154, 1175,
};

static int16_t _a[_K][_K][_N];
static int16_t _s[_K][_N];
static int16_t _e[_K][_N];
static int16_t _t[_K][_N];
static int16_t _u[_K][_N];
static int16_t _v[_N];
static int16_t _tmp[_N];
static uint8_t _ct2[MP_CRYPTO_MLKEM_CT];
static uint8_t _jin[32 + MP_CRYPTO_MLKEM_CT];

/** Quotient by q. u stays below (q - 1)^2, where one correction is enough. */
static uint32_t _divq(uint32_t u) {
	uint32_t q = (uint32_t)(((uint64_t)u * _BARRETT) >> 32);
	uint32_t r = u - q * (uint32_t)_Q;
	uint32_t ge = ((r - (uint32_t)_Q) >> 31) ^ 1u;

	return(q + ge);
}

/** Coefficient into [0, q). A negative input is at most one q below zero. */
static int16_t _freeze(int32_t x) {
	uint32_t neg = (uint32_t)(x >> 31);
	uint32_t u = (uint32_t)(x + (int32_t)((uint32_t)_Q & neg));
	uint32_t q = _divq(u);

	return((int16_t)(u - q * (uint32_t)_Q));
}

static int16_t _add(int16_t a, int16_t b) {
	return(_freeze((int32_t)a + b));
}

static int16_t _sub(int16_t a, int16_t b) {
	return(_freeze((int32_t)a - b));
}

static int16_t _mul(int16_t a, int16_t b) {
	return(_freeze((int32_t)a * b));
}

static void _ntt(int16_t f[_N]) {
	int len, start, j, i = 1;

	for(len = 128; len >= 2; len >>= 1) {
		for(start = 0; start < _N; start += 2 * len) {
			int16_t zeta = _zeta[i++];

			for(j = start; j < start + len; j++) {
				int16_t u = f[j];
				int16_t t = _mul(zeta, f[j + len]);

				f[j] = _add(u, t);
				f[j + len] = _sub(u, t);
			}
		}
	}
}

static void _invntt(int16_t f[_N]) {
	int len, start, j, i = 127;

	for(len = 2; len <= 128; len <<= 1) {
		for(start = 0; start < _N; start += 2 * len) {
			int16_t zeta = _zeta[i--];

			for(j = start; j < start + len; j++) {
				int16_t t = f[j];

				f[j] = _add(t, f[j + len]);
				f[j + len] = _mul(zeta, _sub(f[j + len], t));
			}
		}
	}
	for(j = 0; j < _N; j++)
		f[j] = _mul(f[j], 3303);
}

static void _basemul(int16_t r[_N], const int16_t a[_N], const int16_t b[_N]) {
	int i;

	for(i = 0; i < 128; i++) {
		int16_t a0 = a[2 * i], a1 = a[2 * i + 1];
		int16_t b0 = b[2 * i], b1 = b[2 * i + 1];
		int16_t g = _gamma[i];

		r[2 * i] = _add(_mul(a0, b0), _mul(_mul(a1, b1), g));
		r[2 * i + 1] = _add(_mul(a0, b1), _mul(a1, b0));
	}
}

static void _encode(uint8_t *out, const int16_t *a, int d, int n) {
	uint32_t acc = 0;
	int bits = 0, o = 0, i;
	int mask = (1 << d) - 1;

	for(i = 0; i < n; i++) {
		acc |= (uint32_t)(_freeze(a[i]) & mask) << bits;
		bits += d;
		while(bits >= 8) {
			out[o++] = (uint8_t)acc;
			acc >>= 8;
			bits -= 8;
		}
	}
}

static void _decode(int16_t *a, const uint8_t *in, int d, int n) {
	uint32_t acc = 0;
	int bits = 0, o = 0, i;
	int mask = (1 << d) - 1;

	for(i = 0; i < n; i++) {
		while(bits < d) {
			acc |= (uint32_t)in[o++] << bits;
			bits += 8;
		}
		a[i] = (int16_t)(acc & (uint32_t)mask);
		acc >>= d;
		bits -= d;
	}
}

/** round(2^d / q * x) mod 2^d. d is a public parameter. */
static int16_t _compress(int16_t x, int d) {
	uint32_t v = ((uint32_t)_freeze(x) << d) + (uint32_t)_HALF;

	return((int16_t)(_divq(v) & ((1u << d) - 1u)));
}

static int16_t _decompress(int16_t y, int d) {
	return((int16_t)(((uint32_t)y * (uint32_t)_Q + (1u << (d - 1))) >> d));
}

static int _bit(const uint8_t *b, int i) {
	return((b[i >> 3] >> (i & 7)) & 1);
}

static void _cbd(int16_t f[_N], const uint8_t *buf, int eta) {
	int i, j;

	for(i = 0; i < _N; i++) {
		int x = 0, y = 0;

		for(j = 0; j < eta; j++) {
			x += _bit(buf, 2 * i * eta + j);
			y += _bit(buf, 2 * i * eta + eta + j);
		}
		f[i] = _freeze(x - y);
	}
}

static void _prf(uint8_t *out, unsigned outlen, const uint8_t sigma[32], uint8_t n) {
	uint8_t in[33];

	memcpy(in, sigma, 32);
	in[32] = n;
	mp_crypto_shake256(out, outlen, in, 33);
}

/** Public matrix entry. Rejection depends on rho, which is not a secret. */
static void _sample(int16_t f[_N], const uint8_t rho[32], uint8_t x, uint8_t y) {
	mp_crypto_shake128_t c;
	uint8_t in[34];
	uint8_t buf[168];
	unsigned off = 0, filled = 0, n = 0;

	memcpy(in, rho, 32);
	in[32] = x;
	in[33] = y;
	mp_crypto_shake128_init(&c);
	mp_crypto_shake128_absorb(&c, in, 34);
	mp_crypto_shake128_finalize(&c);
	while(n < _N) {
		uint32_t d1, d2;

		if(off + 3 > filled) {
			mp_crypto_shake128_squeeze(&c, buf, sizeof(buf));
			off = 0;
			filled = sizeof(buf);
		}
		d1 = (uint32_t)buf[off] | ((uint32_t)(buf[off + 1] & 0x0fu) << 8);
		d2 = (uint32_t)(buf[off + 1] >> 4) | ((uint32_t)buf[off + 2] << 4);
		off += 3;
		if(d1 < _Q)
			f[n++] = (int16_t)d1;
		if(n < _N && d2 < _Q)
			f[n++] = (int16_t)d2;
	}
	memset(&c, 0, sizeof(c));
	memset(buf, 0, sizeof(buf));
}

static void _gen_a(const uint8_t rho[32]) {
	int i, j;

	for(i = 0; i < _K; i++)
		for(j = 0; j < _K; j++)
			_sample(_a[i][j], rho, (uint8_t)j, (uint8_t)i);
}

/** transpose is a public flag: 0 is A, 1 is A transposed. */
static void _matvec(int16_t out[][_N], const int16_t vec[][_N], int transpose) {
	int i, j, n;

	for(i = 0; i < _K; i++) {
		for(n = 0; n < _N; n++)
			out[i][n] = 0;
		for(j = 0; j < _K; j++) {
			if(transpose)
				_basemul(_tmp, _a[j][i], vec[j]);
			else
				_basemul(_tmp, _a[i][j], vec[j]);
			for(n = 0; n < _N; n++)
				out[i][n] = _add(out[i][n], _tmp[n]);
		}
	}
}

static void _from_msg(int16_t f[_N], const uint8_t m[32]) {
	int i;

	for(i = 0; i < _N; i++) {
		int bit = (m[i >> 3] >> (i & 7)) & 1;

		f[i] = _decompress((int16_t)bit, 1);
	}
}

static void _to_msg(uint8_t m[32], const int16_t f[_N]) {
	int i;

	memset(m, 0, 32);
	for(i = 0; i < _N; i++) {
		int bit = _compress(f[i], 1);

		m[i >> 3] |= (uint8_t)(bit << (i & 7));
	}
}

static void _wipe(void) {
	memset(_a, 0, sizeof(_a));
	memset(_s, 0, sizeof(_s));
	memset(_e, 0, sizeof(_e));
	memset(_t, 0, sizeof(_t));
	memset(_u, 0, sizeof(_u));
	memset(_v, 0, sizeof(_v));
	memset(_tmp, 0, sizeof(_tmp));
	memset(_ct2, 0, sizeof(_ct2));
	memset(_jin, 0, sizeof(_jin));
}

static uint8_t _neq(const uint8_t *a, const uint8_t *b, unsigned n) {
	unsigned i;
	uint8_t d = 0;

	for(i = 0; i < n; i++)
		d |= (uint8_t)(a[i] ^ b[i]);
	return((uint8_t)((d | (uint8_t)-d) >> 7));
}

/** A 0/1 the compiler must not inspect. GCC turns a known mask into a branch. */
static uint32_t _opaque(uint32_t x) {
	__asm__ volatile("" : "+r"(x));
	return(x);
}

static void _cmov(uint8_t *dst, const uint8_t *src, unsigned n, uint8_t bit) {
	unsigned i;
	uint8_t m = (uint8_t)-(uint8_t)_opaque(bit);

	for(i = 0; i < n; i++)
		dst[i] ^= (uint8_t)(m & (dst[i] ^ src[i]));
}

static void _encrypt(uint8_t *ct, const void *ek_in, const void *m_in, const uint8_t r[32]) {
	const uint8_t *ek = ek_in;
	const uint8_t *m = m_in;
	uint8_t prf[192];
	uint8_t nonce = 0;
	int i, n;
	unsigned off;

	_gen_a(ek + _K * MP_CRYPTO_MLKEM_POLY12);
	for(i = 0; i < _K; i++) {
		_prf(prf, 64 * _ETA1, r, nonce++);
		_cbd(_s[i], prf, _ETA1);
		_ntt(_s[i]);
	}
	for(i = 0; i < _K; i++) {
		_prf(prf, 64 * _ETA2, r, nonce++);
		_cbd(_e[i], prf, _ETA2);
	}
	_prf(prf, 64 * _ETA2, r, nonce);
	_cbd(_v, prf, _ETA2);
	_matvec(_u, _s, 1);
	for(i = 0; i < _K; i++) {
		_invntt(_u[i]);
		for(n = 0; n < _N; n++)
			_u[i][n] = _add(_u[i][n], _e[i][n]);
	}
	off = 0;
	for(i = 0; i < _K; i++) {
		_decode(_t[i], ek + off, 12, _N);
		off += MP_CRYPTO_MLKEM_POLY12;
	}
	for(n = 0; n < _N; n++)
		_tmp[n] = 0;
	for(i = 0; i < _K; i++) {
		_basemul(_e[0], _t[i], _s[i]);
		for(n = 0; n < _N; n++)
			_tmp[n] = _add(_tmp[n], _e[0][n]);
	}
	_invntt(_tmp);
	_from_msg(_e[0], m);
	for(n = 0; n < _N; n++)
		_v[n] = _add(_add(_tmp[n], _v[n]), _e[0][n]);
	off = 0;
	for(i = 0; i < _K; i++) {
		for(n = 0; n < _N; n++)
			_u[i][n] = _compress(_u[i][n], _DU);
		_encode(ct + off, _u[i], _DU, _N);
		off += _DU * _N / 8;
	}
	for(n = 0; n < _N; n++)
		_v[n] = _compress(_v[n], _DV);
	_encode(ct + off, _v, _DV, _N);
	memset(prf, 0, sizeof(prf));
}

static void _decrypt(uint8_t m[32], const uint8_t *dk, const uint8_t *ct) {
	unsigned off = 0;
	int i, n;

	for(i = 0; i < _K; i++) {
		_decode(_u[i], ct + off, _DU, _N);
		for(n = 0; n < _N; n++)
			_u[i][n] = _decompress(_u[i][n], _DU);
		_ntt(_u[i]);
		off += _DU * _N / 8;
	}
	_decode(_v, ct + off, _DV, _N);
	for(n = 0; n < _N; n++)
		_v[n] = _decompress(_v[n], _DV);
	for(i = 0; i < _K; i++)
		_decode(_s[i], dk + i * MP_CRYPTO_MLKEM_POLY12, 12, _N);
	for(n = 0; n < _N; n++)
		_tmp[n] = 0;
	for(i = 0; i < _K; i++) {
		_basemul(_e[0], _s[i], _u[i]);
		for(n = 0; n < _N; n++)
			_tmp[n] = _add(_tmp[n], _e[0][n]);
	}
	_invntt(_tmp);
	for(n = 0; n < _N; n++)
		_v[n] = _sub(_v[n], _tmp[n]);
	_to_msg(m, _v);
}

/**
 * @brief Build an ML-KEM key pair
 * @param[in] seed 64 bytes, d then z
 * @param[in] seedlen Length of seed
 * @param[out] priv MP_CRYPTO_MLKEM_PRIV bytes
 * @param[out] pub MP_CRYPTO_MLKEM_PUB bytes
 * @return FALSE when the seed is shorter than 64 bytes
 */
mp_ret_t mp_crypto_mlkem_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	const uint8_t *d;
	uint8_t gin[33], gout[64], prf[192];
	uint8_t *ek = pub;
	uint8_t *dk = priv;
	uint8_t nonce = 0;
	unsigned off;
	int i;

	if(!seed || !priv || !pub || seedlen < MP_CRYPTO_MLKEM_SEED)
		return(FALSE);
	d = seed;
	memcpy(gin, d, 32);
	gin[32] = (uint8_t)_K;
	mp_crypto_sha3_512(gin, 33, gout);
	_gen_a(gout);
	for(i = 0; i < _K; i++) {
		_prf(prf, 64 * _ETA1, gout + 32, nonce++);
		_cbd(_s[i], prf, _ETA1);
		_ntt(_s[i]);
	}
	for(i = 0; i < _K; i++) {
		_prf(prf, 64 * _ETA1, gout + 32, nonce++);
		_cbd(_e[i], prf, _ETA1);
		_ntt(_e[i]);
	}
	_matvec(_t, _s, 0);
	for(i = 0; i < _K; i++) {
		int n;

		for(n = 0; n < _N; n++)
			_t[i][n] = _add(_t[i][n], _e[i][n]);
	}
	off = 0;
	for(i = 0; i < _K; i++) {
		_encode(ek + off, _t[i], 12, _N);
		off += MP_CRYPTO_MLKEM_POLY12;
	}
	memcpy(ek + off, gout, 32);
	off = 0;
	for(i = 0; i < _K; i++) {
		_encode(dk + off, _s[i], 12, _N);
		off += MP_CRYPTO_MLKEM_POLY12;
	}
	memcpy(dk + off, ek, MP_CRYPTO_MLKEM_PUB);
	mp_crypto_sha3_256(ek, MP_CRYPTO_MLKEM_PUB, dk + off + MP_CRYPTO_MLKEM_PUB);
	memcpy(dk + MP_CRYPTO_MLKEM_PRIV - 32, d + 32, 32);
	memset(gin, 0, sizeof(gin));
	memset(gout, 0, sizeof(gout));
	memset(prf, 0, sizeof(prf));
	_wipe();
	return(TRUE);
}

/**
 * @brief Encapsulate under an ML-KEM public key
 * @param[in] pub MP_CRYPTO_MLKEM_PUB bytes
 * @param[in] coins The 32-byte message m
 * @param[in] coinslen Length of coins
 * @param[out] ss 32-byte shared secret
 * @param[out] ct Ciphertext
 * @return The ciphertext length, or 0
 */
unsigned int mp_crypto_mlkem_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct) {
	uint8_t gin[64], gout[64];

	if(!pub || !coins || !ss || !ct || coinslen < MP_CRYPTO_MLKEM_COINS)
		return(0);
	mp_crypto_sha3_256(pub, MP_CRYPTO_MLKEM_PUB, gin + 32);
	memcpy(gin, coins, 32);
	mp_crypto_sha3_512(gin, 64, gout);
	_encrypt(ct, pub, coins, gout + 32);
	memcpy(ss, gout, 32);
	memset(gin, 0, sizeof(gin));
	memset(gout, 0, sizeof(gout));
	_wipe();
	return(MP_CRYPTO_MLKEM_CT);
}

/**
 * @brief Decapsulate an ML-KEM ciphertext
 * @param[in] priv MP_CRYPTO_MLKEM_PRIV bytes
 * @param[in] ct Ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss 32-byte shared secret
 * @return YES when ctlen matches. The secret is written either way in that case.
 */
mp_bool_t mp_crypto_mlkem_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss) {
	const uint8_t *dk = priv;
	const uint8_t *ek;
	const uint8_t *h;
	const uint8_t *z;
	uint8_t m[32], gin[64], gout[64], reject[32];
	uint8_t bad;

	if(!priv || !ct || !ss || ctlen != MP_CRYPTO_MLKEM_CT)
		return(NO);
	ek = dk + _K * MP_CRYPTO_MLKEM_POLY12;
	h = dk + MP_CRYPTO_MLKEM_PRIV - 64;
	z = dk + MP_CRYPTO_MLKEM_PRIV - 32;
	_decrypt(m, dk, ct);
	memcpy(gin, m, 32);
	memcpy(gin + 32, h, 32);
	mp_crypto_sha3_512(gin, 64, gout);
	_encrypt(_ct2, ek, m, gout + 32);
	bad = _neq(ct, _ct2, MP_CRYPTO_MLKEM_CT);
	memcpy(_jin, z, 32);
	memcpy(_jin + 32, ct, MP_CRYPTO_MLKEM_CT);
	mp_crypto_shake256(reject, 32, _jin, 32 + MP_CRYPTO_MLKEM_CT);
	_cmov(gout, reject, 32, bad);
	memcpy(ss, gout, 32);
	memset(m, 0, sizeof(m));
	memset(gin, 0, sizeof(gin));
	memset(gout, 0, sizeof(gout));
	memset(reject, 0, sizeof(reject));
	_wipe();
	return(YES);
}

/**
 * @brief Decapsulate with a fuse slot
 *
 * A series that declares an ML-KEM accelerator provides mp_arch_mlkem_decaps_fuse().
 * wrapped is the blob that arch defines. This function does not burn a fuse.
 *
 * @param[in] fuse Slot of role MP_CRYPTO_FUSE_MLKEM
 * @param[in] wrapped Blob the arch defines
 * @param[in] wrappedlen Byte length of wrapped
 * @param[in] ct Ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss 32-byte shared secret
 * @return YES when the accelerator wrote ss, NO when this series has none
 */
mp_bool_t mp_crypto_mlkem_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss) {
	static mp_bool_t said;

	if(!ss || (ctlen && !ct))
		return(NO);
#ifdef MP_ARCH_MLKEM
	mp_bool_t ok;

#ifdef SUPPORT_COMMON_CRYPTO_FUSE
	if(mp_crypto_fuse_check(fuse, MP_CRYPTO_FUSE_MLKEM) != YES)
		return(NO);
#endif
	ok = mp_arch_mlkem_decaps_fuse(fuse, wrapped, wrappedlen, ct, ctlen, ss);
	if(ok != YES && said == NO) {
		mp_printk("ML-KEM: the accelerator refused the fuse");
		said = YES;
	}
	return(ok);
#else
	(void)fuse;
	(void)wrapped;
	(void)wrappedlen;
	if(said == NO) {
		mp_printk("ML-KEM: this build has no fuse");
		said = YES;
	}
	return(NO);
#endif
}

/**@}*/

#endif
