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

/* Host test of the constant-time RSA inverse. Not linked into the firmware. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_BN
#define SUPPORT_COMMON_CRYPTO_HASH
#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#define SUPPORT_COMMON_CRYPTO_MAC_HMAC
#define SUPPORT_COMMON_CRYPTO_RNG_HMAC_DRBG
#define SUPPORT_COMMON_CRYPTO_SIGN_RSA
#define MP_CRYPTO_RSA_BITS 3072

#include <mp.h>
#include "../../../hash/sha256/sha256.h"
#include "../../../hash/hash.h"
#include "../../../mac/hmac/hmac.h"
#include "../../../rng/hmac_drbg/hmac_drbg.h"
#include "../../../lock/lock.h"
#include "../../../bn/bn.h"
#include "../rsa.h"

#include "../../../lock/lock.c"
/* bn.c and rsa.c each have a scratch named _wide; one translation unit needs two of them */
#define _wide _bn_wide
#include "../../../bn/bn.c"
#undef _wide
#include "../../../hash/sha256/sha256.c"
#include "../../../hash/index.c"
#include "../../../mac/hmac/hmac.c"
#include "../../../rng/hmac_drbg/hmac_drbg.c"
#include "../rsa.c"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

/* PKCS#1 v1.5 SHA-256 signature of "miniPhi", the vector the cryptography page verifies. */
static const uint8_t _stored_pub[MP_CRYPTO_RSA_PUB] = {
	0x79, 0x46, 0x20, 0x6b, 0x4d, 0x0e, 0xdd, 0xb4, 0x60, 0x7a, 0x85, 0x64,
	0xc1, 0x42, 0x41, 0xf9, 0x38, 0x87, 0x4e, 0xde, 0xdf, 0x04, 0xe8, 0xc5,
	0x70, 0xde, 0x1c, 0x1c, 0xd9, 0x1d, 0x04, 0x91, 0x7d, 0x65, 0xe8, 0x4b,
	0x6b, 0xa4, 0x75, 0x1f, 0xa4, 0x7c, 0xa7, 0x8b, 0x65, 0xf4, 0xde, 0x90,
	0x34, 0x62, 0x4f, 0x3d, 0xb5, 0x37, 0xe2, 0xc4, 0x3c, 0xf6, 0x7d, 0xef,
	0xb7, 0x5b, 0x21, 0xb3, 0xfc, 0xfe, 0x18, 0x67, 0xec, 0x85, 0x07, 0x84,
	0xb8, 0x0f, 0x4a, 0xfe, 0x4c, 0xe2, 0xd5, 0xdf, 0xc1, 0xf7, 0xee, 0x86,
	0xb8, 0xf3, 0xee, 0x73, 0xd8, 0xe1, 0x28, 0x8e, 0xee, 0x22, 0x5b, 0x6c,
	0x14, 0xb7, 0xdb, 0x8d, 0x27, 0xc7, 0xe8, 0x45, 0xa7, 0x52, 0x5f, 0x91,
	0x19, 0x67, 0xa8, 0xa5, 0x23, 0x28, 0x39, 0x28, 0x7c, 0x38, 0xef, 0x3e,
	0x3d, 0x69, 0x4f, 0x8c, 0x13, 0xf6, 0x1b, 0xc2, 0xdf, 0x79, 0xa0, 0xa8,
	0x36, 0xa3, 0xa3, 0x21, 0xc8, 0x33, 0x05, 0x64, 0x56, 0xa6, 0xb7, 0xea,
	0xad, 0x24, 0x7e, 0x49, 0x4f, 0x03, 0x23, 0xef, 0x37, 0x1f, 0xdc, 0x04,
	0x2f, 0xc8, 0x72, 0xb1, 0xc5, 0x59, 0x7d, 0xd1, 0x28, 0xa0, 0x8a, 0xc9,
	0x13, 0xc8, 0x41, 0x46, 0xfc, 0x98, 0x94, 0x4d, 0x52, 0xfc, 0xc2, 0x1d,
	0x8b, 0xfe, 0x04, 0x02, 0x84, 0x0d, 0x60, 0xe8, 0x20, 0x4a, 0x6a, 0x37,
	0x3b, 0xea, 0xcd, 0xf5, 0xa3, 0xc6, 0x01, 0xe3, 0xf7, 0xb3, 0xd3, 0x29,
	0x15, 0x1c, 0x07, 0xe1, 0x84, 0xf3, 0x27, 0x38, 0x73, 0x38, 0x51, 0x86,
	0xa3, 0x82, 0xee, 0x53, 0x16, 0xbb, 0xba, 0xa2, 0x2f, 0x38, 0xf2, 0xeb,
	0xfb, 0x5c, 0xa1, 0x86, 0xb5, 0x04, 0xa3, 0x6c, 0x96, 0xda, 0xb4, 0x72,
	0xc0, 0xcd, 0xf1, 0xe3, 0xf8, 0xad, 0x51, 0xa1, 0xbc, 0xc1, 0x3e, 0xb1,
	0x30, 0x5a, 0xc7, 0x44, 0x13, 0xd8, 0x9e, 0xf9, 0x0b, 0xf8, 0x7e, 0xb6,
	0x67, 0xc8, 0x15, 0xd1, 0x52, 0xc3, 0xec, 0xeb, 0x3f, 0x21, 0x93, 0x67,
	0x1d, 0x15, 0xb9, 0xb6, 0xcc, 0xc2, 0x07, 0x1c, 0x05, 0xfb, 0x7a, 0x75,
	0x98, 0xac, 0x13, 0x1b, 0x1c, 0x70, 0xc2, 0xed, 0x9d, 0xf1, 0xe3, 0x51,
	0xcf, 0x04, 0x1c, 0xa4, 0x72, 0x68, 0x33, 0x46, 0x18, 0x61, 0x17, 0xca,
	0xf0, 0x93, 0x91, 0xa5, 0x64, 0xdc, 0x16, 0xe6, 0xeb, 0x02, 0x6c, 0xfa,
	0xf3, 0xad, 0x7c, 0xde, 0xa3, 0x94, 0x80, 0x97, 0x05, 0xd8, 0x74, 0x83,
	0x16, 0xb7, 0x12, 0xac, 0xfb, 0x03, 0xbf, 0xad, 0x0e, 0xcb, 0xeb, 0x6d,
	0xc4, 0xcb, 0xad, 0x7d, 0xca, 0xb0, 0x3f, 0xa5, 0x8d, 0x24, 0xdc, 0x4b,
	0xa1, 0x99, 0xdd, 0x01, 0xbb, 0x5e, 0x5f, 0x02, 0xcb, 0x88, 0x84, 0xf4,
	0x80, 0x5e, 0x07, 0xcd, 0xdc, 0x16, 0x3a, 0xda, 0x45, 0x5e, 0xc4, 0x47,
	0x00, 0x01, 0x00, 0x01,
};

static const uint8_t _stored_sig[MP_CRYPTO_RSA_SIG] = {
	0x5c, 0xf3, 0xff, 0x5d, 0xc2, 0xbd, 0x22, 0xcc, 0x93, 0xa5, 0x7d, 0x60,
	0xe5, 0xa5, 0xb8, 0xe9, 0xe7, 0x1b, 0x2a, 0xfd, 0x1d, 0x14, 0x55, 0x2b,
	0x9a, 0x64, 0xb2, 0x8d, 0x7f, 0x5c, 0xef, 0xbb, 0x73, 0x52, 0xc4, 0x7a,
	0x44, 0x28, 0xc1, 0x64, 0xe9, 0x5f, 0x2d, 0x2a, 0x36, 0x9e, 0x3c, 0x50,
	0xaf, 0x75, 0x13, 0x47, 0xb1, 0x84, 0x03, 0x1e, 0x92, 0x25, 0xb6, 0x93,
	0xbc, 0x9b, 0xd5, 0xa4, 0x45, 0x5d, 0x54, 0x97, 0x18, 0xee, 0xff, 0xb6,
	0xbd, 0x39, 0xe4, 0x12, 0xb2, 0x4c, 0x7a, 0x72, 0xd8, 0x02, 0x2e, 0x2e,
	0x51, 0x28, 0x13, 0x0f, 0xc9, 0x57, 0xa0, 0xfd, 0xdf, 0x75, 0x38, 0x71,
	0xf0, 0x96, 0xc8, 0xcb, 0x3c, 0x65, 0xf1, 0x8f, 0xf4, 0x41, 0xdf, 0x93,
	0xc0, 0x04, 0xcb, 0x6b, 0x12, 0xa3, 0xae, 0x39, 0x9a, 0xf5, 0x44, 0xb3,
	0x70, 0xe6, 0x89, 0xf7, 0x54, 0xf3, 0x5c, 0x47, 0x69, 0xae, 0x4c, 0xb9,
	0xed, 0xed, 0x92, 0x66, 0xd7, 0xaa, 0x31, 0xc0, 0x64, 0x1c, 0x41, 0xad,
	0x33, 0xd2, 0x6f, 0x49, 0x0c, 0xa1, 0x5b, 0x20, 0x42, 0x42, 0x6a, 0xcc,
	0x8f, 0xee, 0x0e, 0x78, 0xc7, 0xa0, 0x48, 0xf1, 0x18, 0x2f, 0x64, 0x7f,
	0xbf, 0xf1, 0x17, 0x8c, 0x68, 0x9b, 0x3d, 0xe0, 0x92, 0xd0, 0xd1, 0x0a,
	0x24, 0x28, 0xc7, 0x27, 0x06, 0xb9, 0x97, 0x5b, 0x69, 0x3d, 0x9a, 0xd9,
	0x0b, 0x60, 0x5a, 0x5c, 0xd3, 0x98, 0xb1, 0x2b, 0x95, 0x49, 0xfa, 0x8f,
	0x2c, 0x9b, 0x5e, 0x4b, 0x0d, 0x39, 0xbb, 0xf5, 0x9c, 0x7f, 0x6e, 0xfb,
	0x9c, 0xcd, 0x5c, 0x2f, 0x22, 0x2e, 0x12, 0x8d, 0xe5, 0xa2, 0xc4, 0x92,
	0xfe, 0x48, 0x02, 0xa4, 0xb9, 0x7c, 0x7a, 0x0d, 0x8e, 0x40, 0xb7, 0x8c,
	0xab, 0xb9, 0xfa, 0x8d, 0x98, 0x1e, 0x3e, 0x45, 0x8f, 0x56, 0x51, 0x85,
	0xcf, 0x4f, 0x9d, 0x9e, 0xe2, 0x1d, 0x7c, 0xfc, 0x03, 0x82, 0xed, 0xb8,
	0x1c, 0x3d, 0x9e, 0x0b, 0x04, 0x39, 0x00, 0x02, 0x9e, 0x63, 0x11, 0xf3,
	0x52, 0xb3, 0x1b, 0xc0, 0x05, 0x68, 0x0e, 0x97, 0x93, 0x9e, 0xc8, 0xb8,
	0xfd, 0x91, 0xef, 0xd8, 0x1e, 0xfd, 0x97, 0x84, 0x06, 0x62, 0xc6, 0x3e,
	0x4a, 0xae, 0x9e, 0xa1, 0x54, 0x15, 0x16, 0x81, 0x3f, 0xbe, 0x59, 0xe4,
	0xe0, 0xaf, 0x2d, 0xf8, 0xfb, 0xc7, 0xda, 0x05, 0x0e, 0x8c, 0x65, 0xec,
	0x02, 0xd9, 0xf8, 0xae, 0x23, 0x72, 0xf0, 0x11, 0xd1, 0x8c, 0x9e, 0x26,
	0x9e, 0x3a, 0xdf, 0x0a, 0x8f, 0x52, 0xc5, 0x7d, 0xf0, 0x3a, 0x2a, 0xd3,
	0x5e, 0x7c, 0x94, 0xa3, 0x96, 0xb8, 0x91, 0x11, 0x74, 0x45, 0xef, 0x5b,
	0xa2, 0x02, 0x76, 0xfb, 0xa5, 0xa8, 0xac, 0xf8, 0x46, 0x03, 0x4e, 0x2c,
	0xf0, 0xf9, 0xc1, 0x87, 0x8e, 0x2e, 0xdf, 0xf3, 0x47, 0x2e, 0x1c, 0xb1,
};

static uint32_t _rng = 0xA5A5F00Du;

static uint32_t rnd(void) {
	uint32_t x = _rng;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	_rng = x;
	return(x);
}

static int fail(const char *what) {
	fprintf(stderr, "rsa key: %s\n", what);
	return(1);
}

static uint32_t ref_mod_e(uint32_t x) {
	return((uint32_t)((uint64_t)x % _E));
}

static int words_eq(const uint32_t *a, const uint32_t *b, int n) {
	uint32_t d = 0;
	int i;

	for(i = 0; i < n; i++)
		d |= a[i] ^ b[i];
	return(d == 0);
}

static int is_one(const uint32_t *a, int n) {
	uint32_t d = a[0] ^ 1u;
	int i;

	for(i = 1; i < n; i++)
		d |= a[i];
	return(d == 0);
}

static uint32_t ref_div(uint32_t *q, const uint32_t *a, int n, uint32_t m) {
	uint64_t r = 0;
	int i;

	for(i = n - 1; i >= 0; i--) {
		uint64_t cur = (r << 32) | a[i];

		if(q)
			q[i] = (uint32_t)(cur / m);
		r = cur % m;
	}
	return((uint32_t)r);
}

static int check_div(const uint32_t *a, int n, uint32_t m) {
	uint32_t q[MP_CRYPTO_BN_MAX * 2];
	uint32_t refq[MP_CRYPTO_BN_MAX * 2];
	uint32_t alias[MP_CRYPTO_BN_MAX * 2];
	uint32_t got;
	uint32_t expect;

	expect = ref_div(refq, a, n, m);
	got = mp_crypto_bn_div_u32(q, a, n, m);
	if(got != expect || !words_eq(q, refq, n))
		return(fail("div"));
	if(mp_crypto_bn_mod_u32(a, n, m) != expect)
		return(fail("mod"));
	memcpy(alias, a, (unsigned)n * sizeof(uint32_t));
	if(mp_crypto_bn_div_u32(alias, alias, n, m) != expect || !words_eq(alias, refq, n))
		return(fail("div alias"));
	return(0);
}

static void mul_word(uint32_t *r, const uint32_t *a, int n, uint32_t m) {
	uint64_t c = 0;
	int i;

	for(i = 0; i < n; i++) {
		uint64_t p = (uint64_t)a[i] * m + c;

		r[i] = (uint32_t)p;
		c = p >> 32;
	}
	r[n] = (uint32_t)c;
}

static int ref_ge(const uint32_t *a, const uint32_t *b, int n) {
	int i;

	for(i = n - 1; i >= 0; i--) {
		if(a[i] != b[i])
			return(a[i] > b[i]);
	}
	return(1);
}

static void ref_sub(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	uint64_t br = 0;
	int i;

	for(i = 0; i < n; i++) {
		uint64_t v = (uint64_t)a[i] - b[i] - br;

		r[i] = (uint32_t)v;
		br = v >> 63;
	}
}

static void ref_mod(uint32_t *rem, const uint32_t *num, int nn, const uint32_t *den, int n) {
	uint32_t r[MP_CRYPTO_BN_MAX * 2 + 2];
	uint32_t d[MP_CRYPTO_BN_MAX * 2 + 2];
	int bits = nn * 32;
	int i;

	memset(r, 0, sizeof(r));
	memset(d, 0, sizeof(d));
	memcpy(d, den, (unsigned)n * sizeof(uint32_t));
	for(i = bits - 1; i >= 0; i--) {
		int k;
		uint32_t bit = (num[i >> 5] >> (i & 31)) & 1u;

		for(k = n; k > 0; k--)
			r[k] = (r[k] << 1) | (r[k - 1] >> 31);
		r[0] = (r[0] << 1) | bit;
		if(ref_ge(r, d, n + 1))
			ref_sub(r, r, d, n + 1);
	}
	memcpy(rem, r, (unsigned)n * sizeof(uint32_t));
}

static void ref_mul(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	int i, j;

	memset(r, 0, (unsigned)(n * 2) * sizeof(uint32_t));
	for(i = 0; i < n; i++) {
		uint64_t c = 0;

		for(j = 0; j < n; j++) {
			uint64_t s = (uint64_t)r[i + j] + (uint64_t)a[j] * b[i] + c;

			r[i + j] = (uint32_t)s;
			c = s >> 32;
		}
		r[i + n] = (uint32_t)c;
	}
}

static void sub1(uint32_t *r, const uint32_t *a, int n) {
	uint64_t br = 1;
	int i;

	for(i = 0; i < n; i++) {
		uint64_t v = (uint64_t)a[i] - br;

		r[i] = (uint32_t)v;
		br = v >> 63;
	}
}

static int check_mod_e(uint32_t x) {
	if(_mod_e(x) != ref_mod_e(x))
		return(fail("mod e"));
	return(0);
}

static int test_mod_e(void) {
	uint32_t x;
	uint32_t lo;
	uint32_t a;
	uint32_t b;
	int i;

	for(x = 0; x <= _E * 2u + 1u; x++) {
		if(check_mod_e(x))
			return(1);
	}
	for(lo = 0; lo < 65536u; lo++) {
		if(check_mod_e((0xFFFFu << 16) | lo))
			return(1);
		if(check_mod_e((lo << 16) | 0xFFFFu))
			return(1);
	}
	if(check_mod_e(0xFFFFFFFFu))
		return(1);
	for(x = 0;; x += 4099u) {
		if(check_mod_e(x))
			return(1);
		if(x > 0xFFFFFFFFu - 4099u)
			break;
	}
	if(_mod_e(0) != 0 || _mod_e(_E) != 0 || _mod_e(_E - 1u) != _E - 1u)
		return(fail("mod e edge"));
	if(_pow_mod_e(2) != 32769u || _mul_mod_e(2, 32769u) != 1)
		return(fail("inv of 2"));
	for(a = 0; a < _E; a++) {
		uint32_t inv = _pow_mod_e(a);
		uint32_t back = _mul_mod_e(inv, a);

		if(a == 0) {
			if(inv != 0 || back != 0)
				return(fail("pow 0"));
		}
		else if(back != 1)
			return(fail("pow"));
		if(_mul_mod_e(a, a) != ref_mod_e((uint32_t)((uint64_t)a * a % _E)))
			return(fail("square"));
	}
	for(i = 0; i < 20000; i++) {
		a = rnd() % _E;
		b = rnd() % _E;
		if(_mul_mod_e(a, b) != (uint32_t)((uint64_t)a * b % _E))
			return(fail("mul e"));
	}
	if(_mul_mod_e(_E - 1u, _E - 1u) != 1)
		return(fail("mul e square"));
	return(0);
}

static int test_div(void) {
	uint32_t a[MP_CRYPTO_BN_MAX * 2];
	uint32_t q[MP_CRYPTO_BN_MAX * 2 + 1];
	uint32_t prod[MP_CRYPTO_BN_MAX * 2 + 1];
	uint32_t guard = 0xA5A5A5A5u;
	uint32_t ms[8];
	int widths[6];
	int w, t, i;

	ms[0] = 1;
	ms[1] = 2;
	ms[2] = 3;
	ms[3] = _E - 1u;
	ms[4] = _E;
	ms[5] = 0x7FFFFFFFu;
	ms[6] = 0x80000000u;
	ms[7] = 0xFFFFFFFFu;
	widths[0] = 1;
	widths[1] = 2;
	widths[2] = 4;
	widths[3] = 8;
	widths[4] = MP_CRYPTO_BN_MAX;
	widths[5] = MP_CRYPTO_BN_MAX * 2;

	memset(a, 0, sizeof(a));
	if(check_div(a, 1, _E))
		return(1);
	a[0] = _E - 1u;
	if(check_div(a, 1, _E))
		return(1);
	a[0] = _E;
	if(check_div(a, 1, _E))
		return(1);
	a[0] = 0xFFFFFFFFu;
	if(check_div(a, 1, _E) || check_div(a, 1, 0xFFFFFFFFu) || check_div(a, 1, 3))
		return(1);
	memset(a, 0xFF, sizeof(a));
	for(w = 0; w < 6; w++) {
		for(t = 0; t < 8; t++) {
			if(check_div(a, widths[w], ms[t]))
				return(1);
		}
	}
	memset(a, 0, sizeof(a));
	a[7] = 0xFFFFFFFFu;
	if(check_div(a, 8, _E) || check_div(a, 8, 0xFFFFFFFFu))
		return(1);
	for(t = 0; t < 40; t++) {
		int n = 1 + (int)(rnd() % (MP_CRYPTO_BN_MAX * 2));
		uint32_t m = ms[rnd() % 8];

		for(i = 0; i < n; i++)
			a[i] = rnd();
		if(check_div(a, n, m))
			return(1);
	}
	for(w = 0; w < 4; w++) {
		int n = widths[w];

		for(i = 0; i < n; i++)
			q[i] = rnd();
		q[n - 1] = 0xFFFFFFFFu;
		mul_word(prod, q, n, _E);
		if(mp_crypto_bn_div_u32(a, prod, n + 1, _E) != 0 || a[n] != 0 || !words_eq(a, q, n))
			return(fail("exact"));
		memset(q, 0, (unsigned)n * sizeof(uint32_t));
		mul_word(prod, q, n, _E);
		if(mp_crypto_bn_div_u32(a, prod, n + 1, _E) != 0 || a[n] != 0 || !words_eq(a, q, n))
			return(fail("exact zero"));
	}
	a[0] = 3;
	a[1] = 4;
	if(mp_crypto_bn_div_u32(a, a, 2, 0) != 0 || a[0] != 0 || a[1] != 0)
		return(fail("div 0"));
	if(mp_crypto_bn_mod_u32(a, 2, 0) != 0)
		return(fail("mod 0"));
	if(mp_crypto_bn_div_u32(&guard, &guard, 0, 3) != 0 || guard != 0xA5A5A5A5u)
		return(fail("div n0"));
	if(mp_crypto_bn_div_u32(&guard, &guard, MP_CRYPTO_BN_MAX * 2 + 1, 3) != 0 || guard != 0xA5A5A5A5u)
		return(fail("div wide"));
	return(0);
}

static int test_d(void) {
	uint32_t phi[8];
	uint32_t d[8];
	uint32_t prod[9];
	uint32_t rem[8];
	uint32_t one;
	int n, trial, i;

	one = 65538u;
	if(!_d_from_phi(d, &one, 1) || d[0] != 65537u)
		return(fail("d of 65538"));
	one = _E - 1u;
	if(!_d_from_phi(d, &one, 1) || d[0] != 1u)
		return(fail("d of e-1"));
	one = 0;
	if(_d_from_phi(d, &one, 1))
		return(fail("d of 0"));
	one = _E;
	if(_d_from_phi(d, &one, 1))
		return(fail("d of e"));
	for(n = 1; n <= 8; n *= 2) {
		for(trial = 0; trial < 8; trial++) {
			for(i = 0; i < n; i++)
				phi[i] = rnd();
			if(n > 1)
				phi[n - 1] = 0xFFFFFFFFu;
			if(!_d_from_phi(d, phi, n)) {
				if(mp_crypto_bn_mod_u32(phi, n, _E) != 0)
					return(fail("d rejected a coprime phi"));
				continue;
			}
			if(mp_crypto_bn_mod_u32(phi, n, _E) == 0)
				return(fail("d accepted a multiple of e"));
			mul_word(prod, d, n, _E);
			ref_mod(rem, prod, n + 1, phi, n);
			if(!is_one(rem, n))
				return(fail("d*e mod phi"));
		}
	}
	return(0);
}

static int test_key(void) {
	static const uint8_t seed[32] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
	};
	static uint8_t priv[MP_CRYPTO_RSA_PRIV];
	static uint8_t pub[MP_CRYPTO_RSA_PUB];
	static uint8_t sig[MP_CRYPTO_RSA_SIG];
	static uint32_t kp[MP_CRYPTO_RSA_WPRIME];
	static uint32_t kq[MP_CRYPTO_RSA_WPRIME];
	static uint32_t kd[MP_CRYPTO_RSA_WMOD];
	static uint32_t kinv[MP_CRYPTO_RSA_WPRIME];
	static uint32_t pm1[MP_CRYPTO_RSA_WPRIME];
	static uint32_t qm1[MP_CRYPTO_RSA_WPRIME];
	static uint32_t phi[MP_CRYPTO_RSA_WMOD];
	static uint32_t prod[MP_CRYPTO_RSA_WMOD + 1];
	static uint32_t wide[MP_CRYPTO_RSA_WMOD * 2];
	static uint32_t rem[MP_CRYPTO_RSA_WMOD];
	unsigned int n;
	int wp = MP_CRYPTO_RSA_WPRIME;
	int wm = MP_CRYPTO_RSA_WMOD;

	if(mp_crypto_rsa_keypair(seed, sizeof(seed), priv, pub) != TRUE)
		return(fail("keypair"));
	if(memcmp(pub, priv, MP_CRYPTO_RSA_PUB) != 0)
		return(fail("pub prefix"));
	if(priv[MP_CRYPTO_RSA_OFF_E] != 0x00 || priv[MP_CRYPTO_RSA_OFF_E + 1] != 0x01
		|| priv[MP_CRYPTO_RSA_OFF_E + 2] != 0x00 || priv[MP_CRYPTO_RSA_OFF_E + 3] != 0x01)
		return(fail("e bytes"));
	mp_crypto_bn_from_be(kp, wp, priv + MP_CRYPTO_RSA_OFF_P, MP_CRYPTO_RSA_PRIME);
	mp_crypto_bn_from_be(kq, wp, priv + MP_CRYPTO_RSA_OFF_Q, MP_CRYPTO_RSA_PRIME);
	mp_crypto_bn_from_be(kd, wm, priv + MP_CRYPTO_RSA_OFF_D, MP_CRYPTO_RSA_MODULUS);
	mp_crypto_bn_from_be(kinv, wp, priv + MP_CRYPTO_RSA_OFF_QINV, MP_CRYPTO_RSA_PRIME);
	if((kp[0] & 1u) == 0 || (kq[0] & 1u) == 0)
		return(fail("prime parity"));
	if((kp[wp - 1] & 0x80000000u) == 0 || (kq[wp - 1] & 0x80000000u) == 0)
		return(fail("prime top"));
	sub1(pm1, kp, wp);
	sub1(qm1, kq, wp);
	ref_mul(phi, pm1, qm1, wp);
	mul_word(prod, kd, wm, _E);
	ref_mod(rem, prod, wm + 1, phi, wm);
	if(!is_one(rem, wm))
		return(fail("d*e mod phi"));
	ref_mul(wide, kinv, kq, wp);
	ref_mod(rem, wide, wm, kp, wp);
	if(!is_one(rem, wp))
		return(fail("qinv*q mod p"));
	n = mp_crypto_rsa_sign(priv, "miniPhi", 7, sig);
	if(n != MP_CRYPTO_RSA_SIG)
		return(fail("sign"));
	if(mp_crypto_rsa_verify(pub, "miniPhi", 7, sig, n) != YES)
		return(fail("verify"));
	if(mp_crypto_rsa_verify(pub, "miniPhj", 7, sig, n) == YES)
		return(fail("verify accepted a different message"));
	if(mp_crypto_rsa_verify(_stored_pub, "miniPhi", 7, _stored_sig, MP_CRYPTO_RSA_SIG) != YES)
		return(fail("stored signature"));
	return(0);
}

int main(void) {
	uint32_t num[1];
	uint32_t den[1];
	uint32_t rem[1];

	num[0] = 100;
	den[0] = 7;
	ref_mod(rem, num, 1, den, 1);
	if(rem[0] != 2)
		return(fail("ref mod"));
	if(test_mod_e())
		return(1);
	if(test_div())
		return(1);
	if(test_d())
		return(1);
	if(test_key())
		return(1);
	return(0);
}
