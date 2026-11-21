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

/* Host test of the bignum against a schoolbook reference. Not linked into
 * the firmware. MP_CRYPTO_RSA_BITS is 3072 so a 1536-bit prime fits. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_BN
#define MP_CRYPTO_RSA_BITS 3072

#include <mp.h>
#include "../../lock/lock.h"
#include "../bn.h"

#if MP_CRYPTO_BN_MAX < 48 || MP_CRYPTO_BN_PRIME_MAX < 48
#error 1536-bit prime does not fit
#endif

#include "../../lock/lock.c"
#include "../bn.c"

static int cases;
static int failed;
static uint32_t rng = 0xA5A5F00Du;
static mp_crypto_lock_t held;
static char note[96];

static const int sizes[] = { 1, 2, 3, 8, 16, MP_CRYPTO_BN_MAX };

/* 11, 13, 17 catch 3215031751. 13, 17, 19 catch 2152302898747.
 * Neither set is only the small bases those composites lie about. */
static const uint32_t witnesses[] = { 2, 3, 5, 7, 11, 13, 17, 19 };

/* RFC 2409 section 6.2, the 1024-bit MODP prime. RFC 3526 has no
 * 1024-bit group; its 1536-bit group 5 is tested as well. */
static const char oakley1024[] =
	"FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381"
	"FFFFFFFFFFFFFFFF";

static const char modp1536[] =
	"FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
	"C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
	"83655D23DCA3AD961C62F356208552BB9ED529077096966D"
	"670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF";

struct fill {
	uint32_t v[8];
	int n;
	int i;
	int calls;
};

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static uint32_t rnd(void) {
	rng ^= rng << 13;
	rng ^= rng >> 17;
	rng ^= rng << 5;
	return(rng);
}

static void enter(void) {
	held = mp_crypto_lock();
}

static void leave(void) {
	mp_crypto_unlock(held);
}

static int stop(void) {
	return(failed > 16);
}

static const char *tag(const char *op, int n) {
	snprintf(note, sizeof(note), "%s n=%d", op, n);
	return(note);
}

static void fail(const char *msg) {
	failed++;
	if(failed <= 16)
		fprintf(stderr, "bn: %s\n", msg);
}

static void expect(int cond, const char *msg) {
	cases++;
	if(!cond)
		fail(msg);
}

static void words(const char *msg, const uint32_t *got, const uint32_t *exp, int n) {
	int i;

	cases++;
	if(memcmp(got, exp, (size_t)n * sizeof(uint32_t)) == 0)
		return;
	fail(msg);
	if(failed > 16)
		return;
	fprintf(stderr, "  got");
	for(i = n - 1; i >= 0; i--)
		fprintf(stderr, " %08x", got[i]);
	fprintf(stderr, "\n  exp");
	for(i = n - 1; i >= 0; i--)
		fprintf(stderr, " %08x", exp[i]);
	fprintf(stderr, "\n");
}

static void wipe(uint32_t *a, int n, uint32_t v) {
	int i;

	for(i = 0; i < n; i++)
		a[i] = v;
}

static void rand_words(uint32_t *a, int n) {
	int i;

	for(i = 0; i < n; i++)
		a[i] = rnd();
}

static uint32_t ref_add(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	uint64_t c = 0;
	int i;

	for(i = 0; i < n; i++) {
		c += (uint64_t)a[i] + b[i];
		r[i] = (uint32_t)c;
		c >>= 32;
	}
	return((uint32_t)c);
}

static uint32_t ref_sub(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	uint32_t br = 0;
	int i;

	for(i = 0; i < n; i++) {
		uint64_t d = (uint64_t)a[i] - b[i] - br;

		r[i] = (uint32_t)d;
		br = (d >> 32) ? 1u : 0u;
	}
	return(br);
}

static int ref_cmp(const uint32_t *a, const uint32_t *b, int n) {
	int i;

	for(i = n - 1; i >= 0; i--) {
		if(a[i] > b[i])
			return(1);
		if(a[i] < b[i])
			return(-1);
	}
	return(0);
}

static int ref_zero(const uint32_t *a, int n) {
	int i;

	for(i = 0; i < n; i++)
		if(a[i])
			return(0);
	return(1);
}

static uint32_t ref_shl1(uint32_t *r, const uint32_t *a, int n) {
	uint32_t c = 0;
	int i;

	for(i = 0; i < n; i++) {
		uint32_t next = a[i] >> 31;

		r[i] = (a[i] << 1) | c;
		c = next;
	}
	return(c);
}

static uint32_t ref_shr1(uint32_t *r, const uint32_t *a, int n) {
	uint32_t c = 0;
	int i;

	for(i = n - 1; i >= 0; i--) {
		uint32_t next = a[i] & 1u;

		r[i] = (a[i] >> 1) | (c << 31);
		c = next;
	}
	return(c);
}

static void ref_mul(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	int i, j;

	memset(r, 0, (size_t)n * 2 * sizeof(uint32_t));
	for(i = 0; i < n; i++) {
		uint64_t c = 0;

		for(j = 0; j < n; j++) {
			uint64_t p = (uint64_t)a[j] * b[i] + r[i + j] + c;

			r[i + j] = (uint32_t)p;
			c = p >> 32;
		}
		r[i + n] = (uint32_t)c;
	}
}

/* Restoring division, one bit at a time. den is not zero. */
static void ref_mod(uint32_t *r, const uint32_t *num, int nn, const uint32_t *den, int n) {
	uint32_t rem[MP_CRYPTO_BN_MAX * 2 + 2];
	uint32_t denx[MP_CRYPTO_BN_MAX * 2 + 2];
	int width = n + 1;
	int bits = nn * 32;
	int i;

	memset(rem, 0, sizeof(rem));
	memset(denx, 0, sizeof(denx));
	memcpy(denx, den, (size_t)n * sizeof(uint32_t));
	for(i = bits - 1; i >= 0; i--) {
		ref_shl1(rem, rem, width);
		rem[0] |= (num[i >> 5] >> (i & 31)) & 1u;
		if(ref_cmp(rem, denx, width) >= 0)
			ref_sub(rem, rem, denx, width);
	}
	memcpy(r, rem, (size_t)n * sizeof(uint32_t));
}

static void ref_add_mod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *m, int n) {
	uint32_t sum[MP_CRYPTO_BN_MAX + 1];
	uint32_t wide[MP_CRYPTO_BN_MAX + 1];
	uint32_t out[MP_CRYPTO_BN_MAX + 1];

	sum[n] = ref_add(sum, a, b, n);
	memset(wide, 0, (size_t)(n + 1) * sizeof(uint32_t));
	memcpy(wide, m, (size_t)n * sizeof(uint32_t));
	ref_mod(out, sum, n + 1, wide, n + 1);
	memcpy(r, out, (size_t)n * sizeof(uint32_t));
}

static void ref_sub_mod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *m, int n) {
	uint32_t d[MP_CRYPTO_BN_MAX];

	if(ref_cmp(a, b, n) >= 0)
		ref_sub(r, a, b, n);
	else {
		ref_sub(d, b, a, n);
		ref_sub(r, m, d, n);
	}
}

static uint32_t ref_div_u32(uint32_t *q, const uint32_t *a, int n, uint32_t m) {
	uint64_t cur = 0;
	int i;

	if(!m) {
		if(q)
			memset(q, 0, (size_t)n * sizeof(uint32_t));
		return(0);
	}
	for(i = n - 1; i >= 0; i--) {
		cur = (cur << 32) | a[i];
		if(q)
			q[i] = (uint32_t)(cur / m);
		cur %= m;
	}
	return((uint32_t)cur);
}

/* A zero exponent value is 1. en == 0 is refused by the implementation. */
static void ref_modexp(uint32_t *r, const uint32_t *base, const uint32_t *exp, int en, const uint32_t *m, int n) {
	uint32_t acc[MP_CRYPTO_BN_MAX];
	uint32_t b[MP_CRYPTO_BN_MAX];
	uint32_t sq[MP_CRYPTO_BN_MAX * 2];
	int bit;

	memset(acc, 0, (size_t)n * sizeof(uint32_t));
	acc[0] = 1;
	ref_mod(acc, acc, n, m, n);
	ref_mod(b, base, n, m, n);
	for(bit = en * 32 - 1; bit >= 0; bit--) {
		ref_mul(sq, acc, acc, n);
		ref_mod(acc, sq, n * 2, m, n);
		if((exp[bit >> 5] >> (bit & 31)) & 1u) {
			ref_mul(sq, acc, b, n);
			ref_mod(acc, sq, n * 2, m, n);
		}
	}
	memcpy(r, acc, (size_t)n * sizeof(uint32_t));
}

static void ref_from_be(uint32_t *a, int n, const uint8_t *be, unsigned int len) {
	unsigned int i;

	memset(a, 0, (size_t)n * sizeof(uint32_t));
	for(i = 0; i < len && i < (unsigned int)n * 4u; i++)
		a[i >> 2] |= (uint32_t)be[len - 1u - i] << (8 * (i & 3u));
}

static void ref_to_be(uint8_t *be, unsigned int len, const uint32_t *a, int n) {
	unsigned int i;

	for(i = 0; i < len; i++) {
		unsigned int word = i >> 2;

		if(word < (unsigned int)n)
			be[len - 1u - i] = (uint8_t)(a[word] >> (8 * (i & 3u)));
		else
			be[len - 1u - i] = 0;
	}
}

static void ref_from_le(uint32_t *a, int n, const uint8_t *le, unsigned int len) {
	unsigned int i;

	memset(a, 0, (size_t)n * sizeof(uint32_t));
	for(i = 0; i < len && i < (unsigned int)n * 4u; i++)
		a[i >> 2] |= (uint32_t)le[i] << (8 * (i & 3u));
}

static void ref_to_le(uint8_t *le, unsigned int len, const uint32_t *a, int n) {
	unsigned int i;

	for(i = 0; i < len; i++) {
		unsigned int word = i >> 2;

		if(word < (unsigned int)n)
			le[i] = (uint8_t)(a[word] >> (8 * (i & 3u)));
		else
			le[i] = 0;
	}
}

static void odd_mod(uint32_t *m, int n) {
	rand_words(m, n);
	m[0] |= 1u;
}

static void dec1(uint32_t *r, const uint32_t *a, int n) {
	uint32_t one[MP_CRYPTO_BN_MAX];

	memset(one, 0, (size_t)n * sizeof(uint32_t));
	one[0] = 1;
	ref_sub(r, a, one, n);
}

static int nib(int c) {
	if(c >= '0' && c <= '9')
		return(c - '0');
	if(c >= 'A' && c <= 'F')
		return(c - 'A' + 10);
	if(c >= 'a' && c <= 'f')
		return(c - 'a' + 10);
	return(-1);
}

/* Big-endian hex. The last byte is the low byte of word 0. */
static int hex_words(uint32_t *a, int n, const char *hex) {
	unsigned int len = (unsigned int)strlen(hex);
	unsigned int bytes;
	unsigned int i;

	if((len & 1u) != 0)
		return(0);
	bytes = len / 2u;
	if(bytes != (unsigned int)n * 4u)
		return(0);
	memset(a, 0, (size_t)n * sizeof(uint32_t));
	for(i = 0; i < bytes; i++) {
		unsigned int at = bytes - 1u - i;
		int hi = nib((unsigned char)hex[at * 2u]);
		int lo = nib((unsigned char)hex[at * 2u + 1u]);

		if(hi < 0 || lo < 0)
			return(0);
		a[i >> 2] |= (uint32_t)((hi << 4) | lo) << (8 * (i & 3u));
	}
	return(1);
}

static uint32_t scratch_or(void) {
	uint32_t x = 0;
	int i, j;

	for(i = 0; i < MP_CRYPTO_BN_MAX * 2; i++)
		x |= _div_q[i] | _div_num[i] | _wide[i];
	for(i = 0; i < MP_CRYPTO_BN_MAX + 2; i++)
		x |= _div_rem[i] | _div_den[i] | _div_sub[i];
	for(j = 0; j < 8; j++)
		for(i = 0; i < MP_CRYPTO_BN_MAX; i++)
			x |= _w[j][i];
	for(i = 0; i < MP_CRYPTO_BN_PRIME_MAX; i++)
		x |= _nm1[i] | _odd[i] | _bound[i] | _witness[i] | _x[i];
	return(x);
}

static void constants(void) {
	uint32_t a[2], b[2], r[4], e[4];
	uint8_t be[8] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02 };
	uint8_t le[8] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
	uint8_t hi[5] = { 0x11, 0x22, 0x33, 0x44, 0x55 };
	uint8_t out[8];
	uint32_t w[2];
	uint32_t rem, got;

	a[0] = 0xFFFFFFFFu;
	ref_mul(e, a, a, 1);
	expect(e[0] == 1u && e[1] == 0xFFFFFFFEu, "ref ff*ff");
	enter();
	mp_crypto_bn_mul(r, a, a, 1);
	leave();
	expect(r[0] == 1u && r[1] == 0xFFFFFFFEu, "ff*ff");

	a[0] = 0;
	a[1] = 1;
	b[0] = 0;
	b[1] = 1;
	ref_mul(e, a, b, 2);
	expect(e[0] == 0 && e[1] == 0 && e[2] == 1 && e[3] == 0, "ref 2^64");
	enter();
	mp_crypto_bn_mul(r, a, b, 2);
	leave();
	expect(r[0] == 0 && r[1] == 0 && r[2] == 1 && r[3] == 0, "2^64");

	/* 2^32 / 3 = 0x55555555, remainder 1. */
	rem = ref_div_u32(e, a, 2, 3);
	expect(rem == 1 && e[0] == 0x55555555u && e[1] == 0, "ref 2^32/3");
	enter();
	got = mp_crypto_bn_div_u32(r, a, 2, 3);
	leave();
	expect(got == 1 && r[0] == 0x55555555u && r[1] == 0, "2^32/3");

	a[0] = 10;
	b[0] = 3;
	ref_mod(e, a, 1, b, 1);
	expect(e[0] == 1, "ref 10%3");
	enter();
	mp_crypto_bn_mod(r, a, 1, b, 1);
	leave();
	expect(r[0] == 1, "10%3");

	/* 100^13 = 2 (mod 7), 2^10 = 4 (mod 17), 3^5 = 9 (mod 13). */
	a[0] = 100;
	b[0] = 13;
	w[0] = 7;
	ref_modexp(e, a, b, 1, w, 1);
	expect(e[0] == 2, "ref 100^13");
	enter();
	expect(mp_crypto_bn_modexp(r, a, b, 1, w, 1) == TRUE, "100^13 ret");
	leave();
	expect(r[0] == 2, "100^13 mod 7");

	a[0] = 2;
	b[0] = 10;
	w[0] = 17;
	enter();
	expect(mp_crypto_bn_modexp(r, a, b, 1, w, 1) == TRUE, "2^10 ret");
	leave();
	expect(r[0] == 4, "2^10 mod 17");

	a[0] = 3;
	b[0] = 5;
	w[0] = 13;
	enter();
	expect(mp_crypto_bn_modexp(r, a, b, 1, w, 1) == TRUE, "3^5 ret");
	leave();
	expect(r[0] == 9, "3^5 mod 13");

	w[0] = 2;
	w[1] = 1;
	ref_from_be(e, 2, be, 8);
	expect(e[0] == 2 && e[1] == 1, "ref be");
	enter();
	mp_crypto_bn_from_be(r, 2, be, 8);
	leave();
	expect(r[0] == 2 && r[1] == 1, "be words");
	memset(out, 0xFF, sizeof(out));
	enter();
	mp_crypto_bn_to_be(out, 8, r, 2);
	leave();
	expect(memcmp(out, be, 8) == 0, "be bytes");

	ref_from_le(e, 2, le, 8);
	expect(e[0] == 2 && e[1] == 1, "ref le");
	enter();
	mp_crypto_bn_from_le(r, 2, le, 8);
	leave();
	expect(r[0] == 2 && r[1] == 1, "le words");
	enter();
	mp_crypto_bn_to_le(out, 8, r, 2);
	leave();
	expect(memcmp(out, le, 8) == 0, "le bytes");

	/* Five big-endian bytes into one word keep the last four: 0x22334455. */
	ref_from_be(e, 1, hi, 5);
	expect(e[0] == 0x22334455u, "ref be trunc");
	enter();
	mp_crypto_bn_from_be(r, 1, hi, 5);
	leave();
	expect(r[0] == 0x22334455u, "be trunc");
	enter();
	mp_crypto_bn_to_be(out, 5, r, 1);
	leave();
	expect(out[0] == 0x00 && out[1] == 0x22 && out[2] == 0x33, "be pad");
	expect(out[3] == 0x44 && out[4] == 0x55, "be pad low");
}

static void check_fast(int n, int trials) {
	uint32_t a[MP_CRYPTO_BN_MAX];
	uint32_t b[MP_CRYPTO_BN_MAX];
	uint32_t r[MP_CRYPTO_BN_MAX];
	uint32_t e[MP_CRYPTO_BN_MAX];
	uint32_t z[MP_CRYPTO_BN_MAX];
	uint32_t c, got;
	int t;

	memset(z, 0, (size_t)n * sizeof(uint32_t));
	for(t = 0; t < trials && !stop(); t++) {
		rand_words(a, n);
		rand_words(b, n);

		enter();
		got = mp_crypto_bn_add(r, a, b, n);
		leave();
		c = ref_add(e, a, b, n);
		expect(got == c, tag("add carry", n));
		words(tag("add", n), r, e, n);

		enter();
		got = mp_crypto_bn_sub(r, a, b, n);
		leave();
		c = ref_sub(e, a, b, n);
		expect(got == c, tag("sub borrow", n));
		words(tag("sub", n), r, e, n);

		enter();
		got = (uint32_t)mp_crypto_bn_cmp(a, b, n);
		leave();
		expect((int)got == ref_cmp(a, b, n), tag("cmp", n));

		enter();
		got = (uint32_t)mp_crypto_bn_is_zero(a, n);
		leave();
		expect((int)got == ref_zero(a, n), tag("is_zero", n));

		enter();
		got = mp_crypto_bn_shl1(r, a, n);
		leave();
		c = ref_shl1(e, a, n);
		expect(got == c, tag("shl carry", n));
		words(tag("shl", n), r, e, n);

		enter();
		got = mp_crypto_bn_shr1(r, a, n);
		leave();
		c = ref_shr1(e, a, n);
		expect(got == c, tag("shr bit", n));
		words(tag("shr", n), r, e, n);

		memcpy(r, a, (size_t)n * sizeof(uint32_t));
		enter();
		mp_crypto_bn_cmov(r, b, n, 0);
		leave();
		words(tag("cmov0", n), r, a, n);
		enter();
		mp_crypto_bn_cmov(r, b, n, 1);
		leave();
		words(tag("cmov1", n), r, b, n);
	}
}

static void check_endian(int n, int trials) {
	uint8_t raw[MP_CRYPTO_BN_MAX * 4 + 8];
	uint8_t out[MP_CRYPTO_BN_MAX * 4 + 8];
	uint8_t expb[MP_CRYPTO_BN_MAX * 4 + 8];
	uint32_t r[MP_CRYPTO_BN_MAX];
	uint32_t e[MP_CRYPTO_BN_MAX];
	int t, i, len;

	for(t = 0; t < trials && !stop(); t++) {
		len = (int)(rnd() % (unsigned)(n * 4 + 8));
		for(i = 0; i < len; i++)
			raw[i] = (uint8_t)rnd();
		ref_from_be(e, n, raw, (unsigned)len);
		enter();
		mp_crypto_bn_from_be(r, n, raw, (unsigned)len);
		leave();
		words(tag("from_be", n), r, e, n);
		memset(out, 0xA5, sizeof(out));
		memset(expb, 0x5A, (size_t)len);
		ref_to_be(expb, (unsigned)len, e, n);
		enter();
		mp_crypto_bn_to_be(out, (unsigned)len, r, n);
		leave();
		expect(len == 0 || memcmp(out, expb, (size_t)len) == 0, tag("to_be", n));

		ref_from_le(e, n, raw, (unsigned)len);
		enter();
		mp_crypto_bn_from_le(r, n, raw, (unsigned)len);
		leave();
		words(tag("from_le", n), r, e, n);
		ref_to_le(expb, (unsigned)len, e, n);
		enter();
		mp_crypto_bn_to_le(out, (unsigned)len, r, n);
		leave();
		expect(len == 0 || memcmp(out, expb, (size_t)len) == 0, tag("to_le", n));
	}
}

static void check_mul(int n, int trials) {
	uint32_t a[MP_CRYPTO_BN_MAX];
	uint32_t b[MP_CRYPTO_BN_MAX];
	uint32_t r[MP_CRYPTO_BN_MAX * 2];
	uint32_t e[MP_CRYPTO_BN_MAX * 2];
	int t;

	for(t = 0; t < trials && !stop(); t++) {
		rand_words(a, n);
		rand_words(b, n);
		ref_mul(e, a, b, n);
		enter();
		mp_crypto_bn_mul(r, a, b, n);
		leave();
		words(tag("mul", n), r, e, n * 2);
	}
}

static void check_div(int n, int trials) {
	uint32_t a[MP_CRYPTO_BN_MAX];
	uint32_t src[MP_CRYPTO_BN_MAX];
	uint32_t q[MP_CRYPTO_BN_MAX];
	uint32_t e[MP_CRYPTO_BN_MAX];
	uint32_t m, rem, got;
	int t;

	for(t = 0; t < trials && !stop(); t++) {
		rand_words(a, n);
		if((t & 15) == 0)
			m = 1;
		else if((t & 15) == 1)
			m = 0xFFFFFFFFu;
		else
			m = rnd() | 1u;
		memcpy(src, a, (size_t)n * sizeof(uint32_t));
		rem = ref_div_u32(e, src, n, m);
		enter();
		got = mp_crypto_bn_div_u32(a, a, n, m);
		leave();
		expect(got == rem, tag("div alias rem", n));
		words(tag("div alias", n), a, e, n);

		memcpy(a, src, (size_t)n * sizeof(uint32_t));
		enter();
		got = mp_crypto_bn_div_u32(q, a, n, m);
		leave();
		expect(got == rem, tag("div rem", n));
		words(tag("div q", n), q, e, n);
		words(tag("div a", n), a, src, n);

		enter();
		got = mp_crypto_bn_mod_u32(a, n, m);
		leave();
		expect(got == rem, tag("mod_u32", n));
		words(tag("mod_u32 a", n), a, src, n);
	}
	wipe(q, n, 0xA5A5A5A5u);
	enter();
	got = mp_crypto_bn_div_u32(q, a, n, 0);
	leave();
	expect(got == 0, tag("div0 rem", n));
	expect(ref_zero(q, n), tag("div0 q", n));
	enter();
	got = mp_crypto_bn_mod_u32(a, n, 0);
	leave();
	expect(got == 0, tag("mod0", n));
}

static void check_mod(int n, int trials) {
	uint32_t num[MP_CRYPTO_BN_MAX * 2];
	uint32_t a[MP_CRYPTO_BN_MAX];
	uint32_t b[MP_CRYPTO_BN_MAX];
	uint32_t m[MP_CRYPTO_BN_MAX];
	uint32_t r[MP_CRYPTO_BN_MAX];
	uint32_t e[MP_CRYPTO_BN_MAX];
	int t, nn;

	for(t = 0; t < trials && !stop(); t++) {
		nn = n + (int)(rnd() % (unsigned)(n + 1));
		if(nn > MP_CRYPTO_BN_MAX * 2)
			nn = MP_CRYPTO_BN_MAX * 2;
		rand_words(num, nn);
		odd_mod(m, n);
		if((t & 7) == 0)
			memcpy(num, m, (size_t)n * sizeof(uint32_t));
		ref_mod(e, num, nn, m, n);
		enter();
		mp_crypto_bn_mod(r, num, nn, m, n);
		leave();
		words(tag("mod", n), r, e, n);

		ref_mod(a, num, n, m, n);
		ref_mod(b, num + (nn > n ? n : 0), n, m, n);
		ref_add_mod(e, a, b, m, n);
		enter();
		mp_crypto_bn_add_mod(r, a, b, m, n);
		leave();
		words(tag("add_mod", n), r, e, n);
		ref_sub_mod(e, a, b, m, n);
		enter();
		mp_crypto_bn_sub_mod(r, a, b, m, n);
		leave();
		words(tag("sub_mod", n), r, e, n);
	}
}

static void check_exp(int n, int trials) {
	uint32_t base[MP_CRYPTO_BN_MAX];
	uint32_t exp[MP_CRYPTO_BN_MAX];
	uint32_t m[MP_CRYPTO_BN_MAX];
	uint32_t r[MP_CRYPTO_BN_MAX];
	uint32_t e[MP_CRYPTO_BN_MAX];
	int t, en;

	for(t = 0; t < trials && !stop(); t++) {
		en = 1;
		if(n > 1 && n <= 16)
			en = 1 + (int)(rnd() % (unsigned)(n < 4 ? n : 4));
		odd_mod(m, n);
		rand_words(base, n);
		rand_words(exp, en);
		ref_modexp(e, base, exp, en, m, n);
		enter();
		expect(mp_crypto_bn_modexp(r, base, exp, en, m, n) == TRUE, tag("exp ret", n));
		leave();
		words(tag("exp", n), r, e, n);
	}
}

static void edges(int n) {
	uint32_t a[MP_CRYPTO_BN_MAX];
	uint32_t b[MP_CRYPTO_BN_MAX];
	uint32_t m[MP_CRYPTO_BN_MAX];
	uint32_t r[MP_CRYPTO_BN_MAX * 2];
	uint32_t e[MP_CRYPTO_BN_MAX * 2];
	uint32_t z[MP_CRYPTO_BN_MAX];
	uint32_t c, got;
	int i;

	memset(z, 0, (size_t)n * sizeof(uint32_t));
	memset(a, 0, (size_t)n * sizeof(uint32_t));
	memset(b, 0, (size_t)n * sizeof(uint32_t));

	enter();
	mp_crypto_bn_zero(r, n);
	mp_crypto_bn_copy(b, a, n);
	leave();
	words(tag("zero", n), r, z, n);
	words(tag("copy0", n), b, z, n);
	enter();
	expect(mp_crypto_bn_is_zero(r, n) == 1, tag("zero?", n));
	expect(mp_crypto_bn_cmp(a, b, n) == 0, tag("0==0", n));
	leave();

	b[0] = 1;
	enter();
	c = mp_crypto_bn_add(r, a, b, n);
	leave();
	expect(c == 0 && r[0] == 1, tag("0+1", n));
	enter();
	c = mp_crypto_bn_sub(r, a, b, n);
	leave();
	expect(c == 1, tag("0-1 borrow", n));
	for(i = 0; i < n; i++)
		expect(r[i] == 0xFFFFFFFFu, tag("0-1", n));

	wipe(a, n, 0xFFFFFFFFu);
	memset(b, 0, (size_t)n * sizeof(uint32_t));
	b[0] = 1;
	enter();
	c = mp_crypto_bn_add(r, a, b, n);
	leave();
	expect(c == 1, tag("ones+1 carry", n));
	expect(ref_zero(r, n), tag("ones+1", n));

	enter();
	c = mp_crypto_bn_add(r, a, a, n);
	leave();
	expect(c == 1, tag("ones+ones carry", n));
	expect(r[0] == 0xFFFFFFFEu, tag("ones+ones low", n));
	for(i = 1; i < n; i++)
		expect(r[i] == 0xFFFFFFFFu, tag("ones+ones", n));

	memset(a, 0, (size_t)n * sizeof(uint32_t));
	a[0] = 0xFFFFFFFFu;
	memset(b, 0, (size_t)n * sizeof(uint32_t));
	b[0] = 1;
	enter();
	c = mp_crypto_bn_add(r, a, b, n);
	leave();
	if(n == 1)
		expect(c == 1 && r[0] == 0, tag("ripple", n));
	else {
		expect(c == 0 && r[0] == 0 && r[1] == 1, tag("ripple", n));
		for(i = 2; i < n; i++)
			expect(r[i] == 0, tag("ripple high", n));
	}

	memset(a, 0, (size_t)n * sizeof(uint32_t));
	a[n - 1] = 0x80000000u;
	enter();
	c = mp_crypto_bn_add(r, a, a, n);
	leave();
	expect(c == 1 && ref_zero(r, n), tag("top bit add", n));
	enter();
	expect(mp_crypto_bn_cmp(a, z, n) == 1, tag("top > 0", n));
	c = mp_crypto_bn_shl1(r, a, n);
	leave();
	expect(c == 1 && ref_zero(r, n), tag("top shl", n));
	enter();
	c = mp_crypto_bn_shr1(r, a, n);
	leave();
	expect(c == 0 && r[n - 1] == 0x40000000u, tag("top shr", n));

	wipe(a, n, 0xFFFFFFFFu);
	enter();
	c = mp_crypto_bn_shl1(r, a, n);
	leave();
	expect(c == 1 && r[0] == 0xFFFFFFFEu, tag("shl ones", n));
	enter();
	c = mp_crypto_bn_shr1(r, a, n);
	leave();
	expect(c == 1 && r[n - 1] == 0x7FFFFFFFu, tag("shr ones", n));

	wipe(m, n, 0xFFFFFFFFu);
	dec1(a, m, n);
	memset(b, 0, (size_t)n * sizeof(uint32_t));
	b[0] = 1;
	enter();
	mp_crypto_bn_add_mod(r, a, b, m, n);
	leave();
	expect(ref_zero(r, n), tag("(m-1)+1", n));
	enter();
	mp_crypto_bn_add_mod(r, a, a, m, n);
	leave();
	expect(r[0] == 0xFFFFFFFDu, tag("(m-1)*2", n));
	enter();
	mp_crypto_bn_sub_mod(r, z, b, m, n);
	leave();
	words(tag("0-1 mod", n), r, a, n);
	enter();
	mp_crypto_bn_sub_mod(r, a, a, m, n);
	leave();
	expect(ref_zero(r, n), tag("a-a mod", n));

	/* Operand equal to the modulus. */
	enter();
	mp_crypto_bn_mod(r, m, n, m, n);
	leave();
	expect(ref_zero(r, n), tag("m mod m", n));
	memset(b, 0, (size_t)n * sizeof(uint32_t));
	b[0] = 1;
	enter();
	expect(mp_crypto_bn_modexp(r, m, b, 1, m, n) == TRUE, tag("m^1 ret", n));
	leave();
	expect(ref_zero(r, n), tag("m^1", n));
	b[0] = 2;
	enter();
	expect(mp_crypto_bn_modexp(r, m, b, 1, m, n) == TRUE, tag("m^2 ret", n));
	leave();
	expect(ref_zero(r, n), tag("m^2", n));
	b[0] = 0;
	ref_modexp(e, m, b, 1, m, n);
	enter();
	expect(mp_crypto_bn_modexp(r, m, b, 1, m, n) == TRUE, tag("m^0 ret", n));
	leave();
	words(tag("m^0", n), r, e, n);
	expect(r[0] == 1, tag("m^0 is 1", n));

	memset(a, 0, (size_t)n * sizeof(uint32_t));
	b[0] = 5;
	enter();
	expect(mp_crypto_bn_modexp(r, a, b, 1, m, n) == TRUE, tag("0^5 ret", n));
	leave();
	expect(ref_zero(r, n), tag("0^5", n));
	b[0] = 0;
	enter();
	expect(mp_crypto_bn_modexp(r, a, b, 1, m, n) == TRUE, tag("0^0 ret", n));
	leave();
	expect(r[0] == 1 && ref_zero(r + 1, n - 1), tag("0^0", n));

	if(n <= 8) {
		dec1(b, m, n);
		a[0] = 2;
		memset(a + 1, 0, (size_t)(n - 1) * sizeof(uint32_t));
		ref_modexp(e, a, b, n, m, n);
		enter();
		expect(mp_crypto_bn_modexp(r, a, b, n, m, n) == TRUE, tag("^(m-1) ret", n));
		leave();
		words(tag("^(m-1)", n), r, e, n);
	}

	wipe(r, n, 0xA5A5A5A5u);
	memset(m, 0, (size_t)n * sizeof(uint32_t));
	enter();
	got = (uint32_t)mp_crypto_bn_modexp(r, a, b, 1, m, n);
	leave();
	expect(got == FALSE, tag("even 0", n));
	expect(r[0] == 0xA5A5A5A5u, tag("even 0 r", n));
	m[0] = 2;
	enter();
	got = (uint32_t)mp_crypto_bn_modexp(r, a, b, 1, m, n);
	leave();
	expect(got == FALSE && r[0] == 0xA5A5A5A5u, tag("even 2", n));
	wipe(m, n, 0xFFFFFFFFu);
	m[0] = 0xFFFFFFFEu;
	enter();
	got = (uint32_t)mp_crypto_bn_modexp(r, a, b, 1, m, n);
	leave();
	expect(got == FALSE && r[n - 1] == 0xA5A5A5A5u, tag("even wide", n));

	/* Odd modulus, so the refusal is the length and not the low bit. */
	m[0] |= 1u;
	enter();
	got = (uint32_t)mp_crypto_bn_modexp(r, a, b, 0, m, n);
	leave();
	expect(got == FALSE && r[0] == 0xA5A5A5A5u, tag("en 0", n));
	if(n < MP_CRYPTO_BN_MAX) {
		enter();
		got = (uint32_t)mp_crypto_bn_modexp(r, a, b, n + 1, m, n);
		leave();
		expect(got == FALSE && r[0] == 0xA5A5A5A5u, tag("en > n", n));
	}
}

static void wide_div(void) {
	enum { N = MP_CRYPTO_BN_MAX * 2 };
	static uint32_t a[N], src[N], e[N];
	uint32_t rem, got;
	uint32_t sentinel[4];
	int i;

	for(i = 0; i < N; i++)
		a[i] = rnd();
	memcpy(src, a, sizeof(src));
	rem = ref_div_u32(e, src, N, 10);
	enter();
	got = mp_crypto_bn_div_u32(a, a, N, 10);
	leave();
	expect(got == rem, "wide div rem");
	words("wide div", a, e, N);

	wipe(sentinel, 4, 0xA5A5A5A5u);
	enter();
	got = mp_crypto_bn_div_u32(sentinel, src, N + 1, 3);
	leave();
	expect(got == 0, "div n range");
	expect(sentinel[0] == 0xA5A5A5A5u && sentinel[3] == 0xA5A5A5A5u, "div n keeps q");
}

static void fill_le(void *user, uint8_t *dst, unsigned int len) {
	struct fill *f = user;
	uint32_t v = 0;
	unsigned int i;

	f->calls++;
	if(f->n) {
		v = f->v[f->i % f->n];
		f->i++;
	}
	for(i = 0; i < len; i++) {
		dst[i] = (uint8_t)v;
		v >>= 8;
	}
}

static void use(struct fill *f, const uint32_t *w, int n) {
	int i;

	memset(f, 0, sizeof(*f));
	f->n = n;
	for(i = 0; i < n; i++)
		f->v[i] = w[i] - 2u;
}

static void prime_is(const uint32_t *a, int n, int yes, const uint32_t *w, int nw, int rounds, const char *msg) {
	struct fill f;
	mp_bool_t got;

	use(&f, w, nw);
	enter();
	got = mp_crypto_bn_prime(a, n, rounds, fill_le, &f);
	leave();
	expect(got == (yes ? YES : NO), msg);
	if(yes)
		expect(f.calls == rounds, msg);
}

static void prime_u32(uint32_t v, int yes, const char *msg) {
	uint32_t a[1];
	uint32_t w[8];
	struct fill f;
	mp_bool_t got;
	int n = 0;
	int i;

	a[0] = v;
	if((v & 1u) == 0 || v < 3) {
		memset(&f, 0, sizeof(f));
		f.n = 1;
		enter();
		got = mp_crypto_bn_prime(a, 1, 8, fill_le, &f);
		leave();
		expect(got == NO, msg);
		expect(f.calls == 0, msg);
		return;
	}
	for(i = 0; i < 8; i++) {
		if(witnesses[i] + 1u < v)
			w[n++] = witnesses[i];
	}
	if(n == 0) {
		w[0] = 2;
		n = 1;
	}
	prime_is(a, 1, yes, w, n, n, msg);
}

static void prime_three(void) {
	uint32_t a[4];
	struct fill f;
	mp_bool_t got;

	/* Raw 1 would become witness 3 if the empty interval were reduced. */
	memset(&f, 0, sizeof(f));
	f.n = 1;
	f.v[0] = 1;
	a[0] = 3;
	enter();
	got = mp_crypto_bn_prime(a, 1, 4, fill_le, &f);
	leave();
	expect(got == YES, "prime 3");

	memset(a, 0, sizeof(a));
	a[0] = 3;
	f.i = 0;
	f.calls = 0;
	enter();
	got = mp_crypto_bn_prime(a, 4, 4, fill_le, &f);
	leave();
	expect(got == YES, "prime 3 wide");

	memset(a, 0, sizeof(a));
	a[0] = 1;
	f.calls = 0;
	enter();
	got = mp_crypto_bn_prime(a, 4, 4, fill_le, &f);
	leave();
	expect(got == NO, "prime 1 wide");
	expect(f.calls == 0, "prime 1 skips");
}

static void fermat(const uint32_t *p, int n, const char *msg) {
	uint32_t base[MP_CRYPTO_BN_PRIME_MAX];
	uint32_t exp[MP_CRYPTO_BN_PRIME_MAX];
	uint32_t one[MP_CRYPTO_BN_PRIME_MAX];
	uint32_t r[MP_CRYPTO_BN_PRIME_MAX];
	int i;

	memset(base, 0, (size_t)n * sizeof(uint32_t));
	base[0] = 2;
	memset(one, 0, (size_t)n * sizeof(uint32_t));
	one[0] = 1;
	ref_sub(exp, p, one, n);
	enter();
	expect(mp_crypto_bn_modexp(r, base, exp, n, p, n) == TRUE, msg);
	leave();
	expect(r[0] == 1, msg);
	for(i = 1; i < n; i++)
		expect(r[i] == 0, msg);
}

static void primes(void) {
	uint32_t p[MP_CRYPTO_BN_PRIME_MAX];
	uint32_t three[5], mer[5], prod[10];
	uint32_t w2[1];
	uint32_t pair[2];
	struct fill f;
	static const uint32_t comp[] = {
		9, 15, 21, 25, 27, 35, 49, 91, 121, 341, 561, 1105, 1729,
		2047, 2465, 41041, 825265u, 3215031751u
	};
	unsigned int i;

	w2[0] = 2;
	pair[0] = 2;
	pair[1] = 3;

	prime_u32(0, 0, "prime 0");
	prime_u32(1, 0, "prime 1");
	prime_u32(2, 0, "prime 2 even");
	prime_u32(4, 0, "prime 4");
	prime_u32(6, 0, "prime 6");
	prime_three();
	prime_u32(5, 1, "prime 5");
	prime_u32(7, 1, "prime 7");
	prime_u32(11, 1, "prime 11");
	prime_u32(17, 1, "prime 17");
	prime_u32(65537u, 1, "prime 65537");
	prime_u32(2147483647u, 1, "prime 2^31-1");
	for(i = 0; i < sizeof(comp) / sizeof(comp[0]); i++)
		prime_u32(comp[i], 0, "composite");

	/* 2152302898747, a strong pseudoprime to the bases 2..12. */
	memset(p, 0, sizeof(p));
	p[0] = 2152302898747u & 0xFFFFFFFFu;
	p[1] = (uint32_t)(2152302898747u >> 32);
	prime_is(p, 2, 0, witnesses, 8, 8, "psp 2152302898747");

	memset(&f, 0, sizeof(f));
	enter();
	expect(mp_crypto_bn_prime(p, 0, 8, fill_le, &f) == NO, "prime n0");
	expect(mp_crypto_bn_prime(p, MP_CRYPTO_BN_PRIME_MAX + 1, 8, fill_le, &f) == NO, "prime nbig");
	expect(mp_crypto_bn_prime(p, 2, 8, NULL, NULL) == NO, "prime no fill");
	leave();

	/* 2^127-1. */
	memset(p, 0, sizeof(p));
	p[0] = p[1] = p[2] = 0xFFFFFFFFu;
	p[3] = 0x7FFFFFFFu;
	prime_is(p, 4, 1, witnesses, 4, 4, "2^127-1");
	fermat(p, 4, "fermat 2^127-1");

	memset(mer, 0, sizeof(mer));
	memcpy(mer, p, 4 * sizeof(uint32_t));
	memset(three, 0, sizeof(three));
	three[0] = 3;
	ref_mul(prod, mer, three, 5);
	expect(prod[5] == 0 && prod[6] == 0, "3*(2^127-1) width");
	prime_is(prod, 5, 0, pair, 2, 2, "3*(2^127-1)");

	/* FIPS 186-4 P-256, little-endian words. */
	memset(p, 0, sizeof(p));
	p[0] = p[1] = p[2] = 0xFFFFFFFFu;
	p[6] = 1;
	p[7] = 0xFFFFFFFFu;
	prime_is(p, 8, 1, w2, 1, 2, "P-256");
	fermat(p, 8, "fermat P-256");

	expect(hex_words(p, 32, oakley1024), "oakley digits");
	prime_is(p, 32, 1, w2, 1, 1, "RFC2409 group 2");
	fermat(p, 32, "fermat RFC2409");

	expect(hex_words(p, 48, modp1536), "modp1536 digits");
	prime_is(p, 48, 1, w2, 1, 1, "RFC3526 group 5");
	fermat(p, 48, "fermat RFC3526");
}

static void clean_check(void) {
	uint32_t num[1], den[1], r[2], a[1], b[1];
	uint32_t c;

	enter();
	mp_crypto_bn_clean();
	leave();
	expect(scratch_or() == 0, "clean empty");
	num[0] = 5;
	den[0] = 3;
	enter();
	mp_crypto_bn_mod(r, num, 1, den, 1);
	leave();
	expect(r[0] == 2, "5 mod 3");
	expect(scratch_or() != 0, "scratch dirty");
	enter();
	mp_crypto_bn_clean();
	leave();
	expect(scratch_or() == 0, "clean");
	a[0] = 3;
	b[0] = 4;
	enter();
	c = mp_crypto_bn_add(r, a, b, 1);
	leave();
	expect(c == 0 && r[0] == 7, "add after clean");
}

static int mul_n(int n) {
	if(n <= 3)
		return(400);
	if(n <= 8)
		return(160);
	if(n <= 16)
		return(40);
	return(8);
}

static int mod_n(int n) {
	if(n <= 3)
		return(80);
	if(n <= 8)
		return(16);
	if(n <= 16)
		return(4);
	return(2);
}

static int exp_n(int n) {
	if(n <= 3)
		return(16);
	if(n <= 8)
		return(3);
	if(n <= 16)
		return(1);
	return(1);
}

int main(void) {
	int i;

	constants();
	for(i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
		int n = sizes[i];

		if(stop())
			break;
		edges(n);
		check_fast(n, 2000);
		check_endian(n, 400);
		check_mul(n, mul_n(n));
		check_div(n, n > 16 ? 400 : 1000);
		check_mod(n, mod_n(n));
		check_exp(n, exp_n(n));
	}
	if(!stop()) {
		wide_div();
		primes();
		clean_check();
	}
	printf("bn cases %d failed %d\n", cases, failed);
	return(failed ? 1 : 0);
}
