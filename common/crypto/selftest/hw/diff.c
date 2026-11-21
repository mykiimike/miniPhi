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

#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST

#include <crypto/selftest.h>
#include <string.h>

/* One entry per accelerator that has a software twin. The public call is the
 * hardware path; mp_crypto_*_soft_* is the portable one. Two hardware contexts
 * take turns on every input. With no accelerator the file registers nothing.
 * mp_crypto_selftest_hw_kernel() is what makes the second pass call mp_clock_level(). */

#if (defined(MP_CRYPTO_SELFTEST_SHA1) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1) && defined(MP_ARCH_SHA1)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA256) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256) && defined(MP_ARCH_SHA256)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA384) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384) && defined(MP_ARCH_SHA384)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA512) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512) && defined(MP_ARCH_SHA512)) \
	|| (defined(MP_CRYPTO_SELFTEST_BLAKE2S) && defined(SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S) && defined(MP_ARCH_BLAKE2S)) \
	|| (defined(MP_CRYPTO_SELFTEST_AES) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES) && defined(MP_ARCH_AES)) \
	|| (defined(MP_CRYPTO_SELFTEST_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES) && defined(MP_ARCH_AES)) \
	|| (defined(MP_CRYPTO_SELFTEST_HMAC) && defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256) && defined(MP_ARCH_HMAC))
#define _HW_DIFF 1
#endif

#ifdef _HW_DIFF

#define _CHUNK 200u
#define _FULL 64u

static uint8_t _scratch[_CHUNK + 8];
static mp_kernel_t *_kernel;

static uint32_t _rng(uint32_t *s) {
	uint32_t x = *s;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	if(!x)
		x = 0xA5A5A5A5u;
	*s = x;
	return(x);
}

static void _fill(uint8_t *dst, unsigned int n, uint32_t *s) {
	unsigned int i;
	uint32_t w = 0;

	for(i = 0; i < n; i++) {
		if((i & 3u) == 0)
			w = _rng(s);
		dst[i] = (uint8_t)(w >> ((i & 3u) * 8));
	}
}

static unsigned int _cases(int full) {
	return(full ? _FULL : 8u);
}

static mp_bool_t _other_clock(mp_bool_t (*body)(void)) {
	mp_power_level_t now;
	mp_power_level_t alt;
	mp_bool_t ok;

	if(!_kernel || !body)
		return(YES);
	now = mp_power_level(_kernel);
	alt = now == MP_POWER_LOW ? MP_POWER_HIGH : MP_POWER_LOW;
	if(mp_clock_level(_kernel, alt) != TRUE)
		return(YES);
	ok = body();
	mp_clock_level(_kernel, now);
	return(ok);
}

#if (defined(MP_CRYPTO_SELFTEST_SHA1) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1) && defined(MP_ARCH_SHA1)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA256) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256) && defined(MP_ARCH_SHA256)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA384) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384) && defined(MP_ARCH_SHA384)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA512) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512) && defined(MP_ARCH_SHA512)) \
	|| (defined(MP_CRYPTO_SELFTEST_BLAKE2S) && defined(SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S) && defined(MP_ARCH_BLAKE2S)) \
	|| (defined(MP_CRYPTO_SELFTEST_HMAC) && defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256) && defined(MP_ARCH_HMAC))
#define _HW_STREAM 1
#endif

#ifdef _HW_STREAM

static unsigned int _len(unsigned int i, int full) {
	static const unsigned int fixed[] = {
		0, 1, 63, 64, 65, 3, 15, 16, 17, 31,
		127, 128, 129, 255, 256, 511, 512, 513, 1024, 3072, 200,
	};
	uint32_t s;

	if(!full)
		return(fixed[i % 8u]);
	if(i < sizeof(fixed) / sizeof(fixed[0]))
		return(fixed[i]);
	s = 0xC0FFEEu ^ (i * 0x9E3779B9u);
	return(1u + (_rng(&s) % 240u));
}

/* 0 would spin. A chunk stays inside _scratch, including the unaligned byte. */
static unsigned int _take(unsigned int i, unsigned int block, unsigned int left) {
	unsigned int n;

	if(!left)
		return(0);
	switch(i % 5u) {
	case 1: n = 1; break;
	case 2: n = block > 1u ? block - 1u : 1u; break;
	case 3: n = 3; break;
	case 4: n = block + 1u; break;
	default: n = left; break;
	}
	if(n > _CHUNK)
		n = _CHUNK;
	if(n > left)
		n = left;
	if(!n)
		n = left > _CHUNK ? _CHUNK : left;
	return(n);
}

typedef void (*_up_f)(void *ctx, const void *data, unsigned int n);

static void _feed_pair(void *a, void *b, _up_f up, unsigned int lena, unsigned int lenb, uint32_t seeda, uint32_t seedb, unsigned int i, unsigned int block) {
	uint32_t sa = seeda;
	uint32_t sb = seedb;
	unsigned int la = lena;
	unsigned int lb = lenb;
	unsigned int off = (i & 1u) ? 1u : 0u;
	unsigned int n;

	while(la || lb) {
		if(la) {
			n = _take(i, block, la);
			_fill(_scratch + off, n, &sa);
			up(a, _scratch + off, n);
			la -= n;
		}
		if(lb) {
			n = _take(i, block, lb);
			_fill(_scratch + off, n, &sb);
			up(b, _scratch + off, n);
			lb -= n;
		}
	}
}

static uint32_t _seed(unsigned int i, unsigned int which) {
	uint32_t s = 0x6A09E667u ^ (i * 0x9E3779B9u);

	if(which)
		s ^= 0xBB67AE85u;
	return(s);
}

#endif

#if (defined(MP_CRYPTO_SELFTEST_SHA1) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1) && defined(MP_ARCH_SHA1)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA256) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256) && defined(MP_ARCH_SHA256)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA384) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384) && defined(MP_ARCH_SHA384)) \
	|| (defined(MP_CRYPTO_SELFTEST_SHA512) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512) && defined(MP_ARCH_SHA512)) \
	|| (defined(MP_CRYPTO_SELFTEST_BLAKE2S) && defined(SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S) && defined(MP_ARCH_BLAKE2S))

typedef void (*_init_f)(void *ctx);
typedef void (*_fin_f)(void *ctx, uint8_t *digest);
typedef void (*_one_f)(const void *data, unsigned int n, uint8_t *digest);
typedef int (*_arm_f)(void *ctx);

static mp_bool_t _hash_go(void *hw0, void *hw1, void *sw0, void *sw1, _init_f hw_init, _init_f sw_init, _up_f update, _fin_f final, _one_f oneshot, _arm_f armed, unsigned int dig, unsigned int block, int full) {
	unsigned int ncase = _cases(full);
	unsigned int i;
	uint8_t d0[64], d1[64], s0[64], s1[64], one[64];

	for(i = 0; i < ncase; i++) {
		unsigned int len = _len(i, full);
		unsigned int off = (i & 1u) ? 1u : 0u;
		uint32_t seed;

		hw_init(hw0);
		hw_init(hw1);
		if(!armed(hw0) || !armed(hw1))
			return(NO);
		sw_init(sw0);
		sw_init(sw1);
		_feed_pair(hw0, hw1, update, len, len, _seed(i, 0), _seed(i, 1), i, block);
		_feed_pair(sw0, sw1, update, len, len, _seed(i, 0), _seed(i, 1), i, block);
		final(hw0, d0);
		final(hw1, d1);
		final(sw0, s0);
		final(sw1, s1);
		if(mp_crypto_selftest_equal(d0, s0, dig) != YES)
			return(NO);
		if(mp_crypto_selftest_equal(d1, s1, dig) != YES)
			return(NO);
		if(len <= _CHUNK) {
			seed = _seed(i, 0);
			_fill(_scratch + off, len, &seed);
			oneshot(_scratch + off, len, one);
			if(mp_crypto_selftest_equal(one, s0, dig) != YES)
				return(NO);
		}
		memset(d0, 0, sizeof(d0));
		memset(d1, 0, sizeof(d1));
		memset(s0, 0, sizeof(s0));
		memset(s1, 0, sizeof(s1));
		memset(one, 0, sizeof(one));
	}
	return(YES);
}

#define _HW_WRAP(tag, T, init, soft, update, final, shot) \
	static void tag##_hwi(void *c) { init((T *)c); } \
	static void tag##_swi(void *c) { soft((T *)c); } \
	static void tag##_up(void *c, const void *p, unsigned int n) { update((T *)c, p, n); } \
	static void tag##_fin(void *c, uint8_t *d) { final((T *)c, d); } \
	static void tag##_one(const void *p, unsigned int n, uint8_t *d) { shot(p, n, d); } \
	static int tag##_arm(void *c) { return(((T *)c)->onHardware == YES); }

#define _HW_ENTRY(fn, tag, T, dig, block) \
	static mp_bool_t tag##_cases(int full) { \
		T hw0, hw1, sw0, sw1; \
		return(_hash_go(&hw0, &hw1, &sw0, &sw1, tag##_hwi, tag##_swi, tag##_up, tag##_fin, tag##_one, tag##_arm, dig, block, full)); \
	} \
	static mp_bool_t tag##_short(void) { return(tag##_cases(0)); } \
	mp_bool_t fn(void) { \
		if(tag##_cases(1) != YES) \
			return(NO); \
		return(_other_clock(tag##_short)); \
	}

#if defined(MP_CRYPTO_SELFTEST_SHA1) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1) && defined(MP_ARCH_SHA1)
#include <crypto/sha1.h>
_HW_WRAP(sha1, mp_crypto_sha1_t, mp_crypto_sha1_init, mp_crypto_sha1_soft_init, mp_crypto_sha1_update, mp_crypto_sha1_final, mp_crypto_sha1)
_HW_ENTRY(mp_crypto_selftest_hw_sha1, sha1, mp_crypto_sha1_t, MP_CRYPTO_SHA1_SIZE, MP_CRYPTO_SHA1_BLOCK)
#endif

#if defined(MP_CRYPTO_SELFTEST_SHA256) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256) && defined(MP_ARCH_SHA256)
#include <crypto/sha256.h>
_HW_WRAP(sha256, mp_crypto_sha256_t, mp_crypto_sha256_init, mp_crypto_sha256_soft_init, mp_crypto_sha256_update, mp_crypto_sha256_final, mp_crypto_sha256)
_HW_ENTRY(mp_crypto_selftest_hw_sha256, sha256, mp_crypto_sha256_t, MP_CRYPTO_SHA256_SIZE, MP_CRYPTO_SHA256_BLOCK)
#endif

#if defined(MP_CRYPTO_SELFTEST_SHA384) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384) && defined(MP_ARCH_SHA384)
#include <crypto/sha384.h>
_HW_WRAP(sha384, mp_crypto_sha384_t, mp_crypto_sha384_init, mp_crypto_sha384_soft_init, mp_crypto_sha384_update, mp_crypto_sha384_final, mp_crypto_sha384)
_HW_ENTRY(mp_crypto_selftest_hw_sha384, sha384, mp_crypto_sha384_t, MP_CRYPTO_SHA384_SIZE, MP_CRYPTO_SHA512_BLOCK)
#endif

#if defined(MP_CRYPTO_SELFTEST_SHA512) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512) && defined(MP_ARCH_SHA512)
#include <crypto/sha512.h>
_HW_WRAP(sha512, mp_crypto_sha512_t, mp_crypto_sha512_init, mp_crypto_sha512_soft_init, mp_crypto_sha512_update, mp_crypto_sha512_final, mp_crypto_sha512)
_HW_ENTRY(mp_crypto_selftest_hw_sha512, sha512, mp_crypto_sha512_t, MP_CRYPTO_SHA512_SIZE, MP_CRYPTO_SHA512_BLOCK)
#endif

#if defined(MP_CRYPTO_SELFTEST_BLAKE2S) && defined(SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S) && defined(MP_ARCH_BLAKE2S)
#include <crypto/blake2s.h>
_HW_WRAP(blake2s, mp_crypto_blake2s_t, mp_crypto_blake2s_init, mp_crypto_blake2s_soft_init, mp_crypto_blake2s_update, mp_crypto_blake2s_final, mp_crypto_blake2s)
_HW_ENTRY(mp_crypto_selftest_hw_blake2s, blake2s, mp_crypto_blake2s_t, MP_CRYPTO_BLAKE2S_SIZE, MP_CRYPTO_BLAKE2S_BLOCK)
#endif

#endif

#if defined(MP_CRYPTO_SELFTEST_AES) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES) && defined(MP_ARCH_AES)
#include <crypto/aes.h>

static void _aes_free(mp_crypto_aes_t *ha, mp_crypto_aes_t *hb, mp_crypto_aes_t *sa, mp_crypto_aes_t *sb, uint8_t *keya, uint8_t *keyb) {
	mp_crypto_aes_free(ha);
	mp_crypto_aes_free(hb);
	mp_crypto_aes_free(sa);
	mp_crypto_aes_free(sb);
	memset(keya, 0, 32);
	memset(keyb, 0, 32);
}

static mp_bool_t _aes_cases(int full) {
	unsigned int ncase = _cases(full);
	unsigned int i;
	mp_crypto_aes_t ha, hb, sa, sb;

	for(i = 0; i < ncase; i++) {
		unsigned int ka;
		unsigned int kb;
		unsigned int inlen;
		unsigned int off;
		uint8_t keya[32], keyb[32];
		uint8_t p[4][16];
		uint8_t h[4][16], s[4][16];
		uint8_t back[16];
		uint32_t seed = 0xA5A50000u ^ (i * 0x9E3779B9u);
		unsigned int n;

		if((i % 5u) == 3u) {
			ka = 16;
			kb = 16;
		}
		else if((i % 5u) == 4u) {
			ka = 32;
			kb = 32;
		}
		else {
			static const unsigned int k[3] = { 16, 24, 32 };

			ka = k[i % 3u];
			kb = k[(i + 1u) % 3u];
		}
		_fill(keya, ka, &seed);
		_fill(keyb, kb, &seed);
		for(n = 0; n < 4u; n++)
			_fill(p[n], 16, &seed);
		if(mp_crypto_aes_init(&ha, keya, ka) != TRUE || mp_crypto_aes_init(&hb, keyb, kb) != TRUE
			|| mp_crypto_aes_soft_init(&sa, keya, ka) != TRUE || mp_crypto_aes_soft_init(&sb, keyb, kb) != TRUE) {
			_aes_free(&ha, &hb, &sa, &sb, keya, keyb);
			return(NO);
		}
		/* 24 may stay in software. 16 and 32 are the sizes the block takes. */
		if((ka == 16 || ka == 32) && ha.onHardware != YES) {
			_aes_free(&ha, &hb, &sa, &sb, keya, keyb);
			return(NO);
		}
		if((kb == 16 || kb == 32) && hb.onHardware != YES) {
			_aes_free(&ha, &hb, &sa, &sb, keya, keyb);
			return(NO);
		}
		inlen = (i % 7u) == 0u ? (i % 17u) : 16u;
		if(inlen > 16u)
			inlen = 16u;
		off = (i & 1u) ? 1u : 0u;
		for(n = 0; n < 4u; n += 2u) {
			memcpy(_scratch + off, p[n], 16);
			if(mp_crypto_aes_update_block(&ha, _scratch + off, inlen, h[n]) != 16)
				goto aes_fail;
			memcpy(_scratch + off, p[n + 1u], 16);
			if(mp_crypto_aes_update_block(&hb, _scratch + off, inlen, h[n + 1u]) != 16)
				goto aes_fail;
		}
		for(n = 0; n < 4u; n += 2u) {
			memcpy(_scratch + off, p[n], 16);
			if(mp_crypto_aes_update_block(&sa, _scratch + off, inlen, s[n]) != 16)
				goto aes_fail;
			memcpy(_scratch + off, p[n + 1u], 16);
			if(mp_crypto_aes_update_block(&sb, _scratch + off, inlen, s[n + 1u]) != 16)
				goto aes_fail;
		}
		for(n = 0; n < 4u; n++) {
			if(mp_crypto_selftest_equal(h[n], s[n], 16) != YES)
				goto aes_fail;
		}
		if(inlen == 16u) {
			if(mp_crypto_aes_update_block_inv(&ha, h[0], 16, back) != 16)
				goto aes_fail;
			if(mp_crypto_selftest_equal(back, p[0], 16) != YES)
				goto aes_fail;
			if(mp_crypto_aes_update_block_inv(&hb, h[1], 16, back) != 16)
				goto aes_fail;
			if(mp_crypto_selftest_equal(back, p[1], 16) != YES)
				goto aes_fail;
			if(mp_crypto_aes_update_block_inv(&sa, s[0], 16, back) != 16)
				goto aes_fail;
			if(mp_crypto_selftest_equal(back, p[0], 16) != YES)
				goto aes_fail;
			memcpy(_scratch + off, p[2], 16);
			if(mp_crypto_aes_update_block(&ha, _scratch + off, 16, _scratch + off) != 16)
				goto aes_fail;
			if(mp_crypto_selftest_equal(_scratch + off, s[2], 16) != YES)
				goto aes_fail;
			if(mp_crypto_aes_update_block_inv(&ha, _scratch + off, 16, _scratch + off) != 16)
				goto aes_fail;
			if(mp_crypto_selftest_equal(_scratch + off, p[2], 16) != YES)
				goto aes_fail;
		}
		_aes_free(&ha, &hb, &sa, &sb, keya, keyb);
		continue;
aes_fail:
		_aes_free(&ha, &hb, &sa, &sb, keya, keyb);
		return(NO);
	}
	return(YES);
}

static mp_bool_t _aes_short(void) {
	return(_aes_cases(0));
}

mp_bool_t mp_crypto_selftest_hw_aes(void) {
	if(_aes_cases(1) != YES)
		return(NO);
	return(_other_clock(_aes_short));
}

#endif

#if defined(MP_CRYPTO_SELFTEST_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES) && defined(MP_ARCH_AES)
#include <crypto/xts.h>

static mp_bool_t _xts_open(mp_crypto_xts_t *a, mp_crypto_xts_t *b, const uint8_t *key, unsigned int keylen, int hardware) {
	unsigned int half = keylen / 2u;

	if(mp_crypto_xts_init(a, key, keylen) != TRUE || mp_crypto_xts_init(b, key, keylen) != TRUE)
		return(NO);
	if(!hardware) {
		if(mp_crypto_aes_soft_init(&a->data, key, half) != TRUE)
			return(NO);
		if(mp_crypto_aes_soft_init(&a->tweak, key + half, half) != TRUE)
			return(NO);
		if(mp_crypto_aes_soft_init(&b->data, key, half) != TRUE)
			return(NO);
		if(mp_crypto_aes_soft_init(&b->tweak, key + half, half) != TRUE)
			return(NO);
		return(YES);
	}
	if(a->data.onHardware != YES || a->tweak.onHardware != YES)
		return(NO);
	if(b->data.onHardware != YES || b->tweak.onHardware != YES)
		return(NO);
	return(YES);
}

static void _xts_close(mp_crypto_xts_t *a, mp_crypto_xts_t *b) {
	mp_crypto_xts_free(a);
	mp_crypto_xts_free(b);
}

static mp_bool_t _xts_cases(int full) {
	static const unsigned int lens[] = { 16, 17, 31, 32, 33, 48, 64, 65 };
	unsigned int ncase = full ? 32u : 4u;
	unsigned int i;

	for(i = 0; i < ncase; i++) {
		unsigned int keylen = (i & 1u) ? 64u : 32u;
		unsigned int len = lens[i % 8u];
		unsigned int off = (i & 1u) ? 1u : 0u;
		uint8_t key[64];
		uint8_t tw[32];
		uint8_t plain0[68], plain1[68];
		uint8_t h0[68], h1[68], s0[68], s1[68], back[68];
		mp_crypto_xts_t a, b;
		uint32_t seed = 0x13579BDFu ^ (i * 0x9E3779B9u);

		_fill(key, keylen, &seed);
		_fill(tw, 32, &seed);
		_fill(plain0, len, &seed);
		_fill(plain1, len, &seed);
		if(_xts_open(&a, &b, key, keylen, 1) != YES) {
			_xts_close(&a, &b);
			memset(key, 0, sizeof(key));
			return(NO);
		}
		if(mp_crypto_xts_set_tweak(&a, tw, 16) != TRUE || mp_crypto_xts_set_tweak(&b, tw + 16, 16) != TRUE
			|| mp_crypto_xts_update(&a, plain0, len, h0) != len || mp_crypto_xts_update(&b, plain1, len, h1) != len) {
			_xts_close(&a, &b);
			memset(key, 0, sizeof(key));
			return(NO);
		}
		memcpy(_scratch + off, plain0, len);
		if(mp_crypto_xts_update(&a, _scratch + off, len, _scratch + off) != len
			|| mp_crypto_selftest_equal(_scratch + off, h0, len) != YES
			|| mp_crypto_xts_update_inv(&a, h0, len, back) != len
			|| mp_crypto_selftest_equal(back, plain0, len) != YES
			|| mp_crypto_xts_update_inv(&b, h1, len, back) != len
			|| mp_crypto_selftest_equal(back, plain1, len) != YES) {
			_xts_close(&a, &b);
			memset(key, 0, sizeof(key));
			return(NO);
		}
		_xts_close(&a, &b);
		if(_xts_open(&a, &b, key, keylen, 0) != YES) {
			_xts_close(&a, &b);
			memset(key, 0, sizeof(key));
			return(NO);
		}
		if(mp_crypto_xts_set_tweak(&a, tw, 16) != TRUE || mp_crypto_xts_set_tweak(&b, tw + 16, 16) != TRUE
			|| mp_crypto_xts_update(&a, plain0, len, s0) != len || mp_crypto_xts_update(&b, plain1, len, s1) != len
			|| mp_crypto_selftest_equal(s0, h0, len) != YES || mp_crypto_selftest_equal(s1, h1, len) != YES
			|| mp_crypto_xts_update_inv(&a, s0, len, back) != len
			|| mp_crypto_selftest_equal(back, plain0, len) != YES) {
			_xts_close(&a, &b);
			memset(key, 0, sizeof(key));
			return(NO);
		}
		_xts_close(&a, &b);
		memset(key, 0, sizeof(key));
		memset(h0, 0, sizeof(h0));
		memset(h1, 0, sizeof(h1));
	}
	return(YES);
}

static mp_bool_t _xts_short(void) {
	return(_xts_cases(0));
}

mp_bool_t mp_crypto_selftest_hw_xts(void) {
	if(_xts_cases(1) != YES)
		return(NO);
	return(_other_clock(_xts_short));
}

#endif

#if defined(MP_CRYPTO_SELFTEST_HMAC) && defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256) && defined(MP_ARCH_HMAC)
#include <crypto/hmac.h>

static void _hmac_hw(void *ctx, const void *data, unsigned int n) {
	mp_crypto_hmac_update(ctx, data, n);
}

static void _hmac_sw(void *ctx, const void *data, unsigned int n) {
	mp_crypto_hmac_soft_update(ctx, data, n);
}

static void _hmac_drop(mp_crypto_hmac_t *ctx) {
	uint8_t dump[MP_CRYPTO_HASH_SIZE_MAX];

	if(ctx->onHardware == YES)
		mp_crypto_hmac_final(ctx, dump);
	else
		mp_crypto_hmac_free(ctx);
	memset(dump, 0, sizeof(dump));
}

static unsigned int _hklen(unsigned int i, unsigned int block) {
	switch(i % 6u) {
	case 0: return(0);
	case 1: return(1);
	case 2: return(block > 1u ? block - 1u : 1u);
	case 3: return(block);
	case 4: return(block + 1u);
	default: return(block * 2u + 3u);
	}
}

/* YES when every accepted pair matches. A hash the block refuses is skipped.
 * The first context on hardware and the second refused is a failure: two
 * contexts have to be alive together. Refusing every hash is a fuse-only block. */
static mp_bool_t _hmac_algo(const char *name, unsigned int dig, unsigned int block, int full) {
	unsigned int ncase = full ? 32u : 4u;
	unsigned int i;

	for(i = 0; i < ncase; i++) {
		mp_crypto_hmac_t hw0, hw1, sw0, sw1;
		unsigned int k0 = _hklen(i, block);
		unsigned int k1 = _hklen(i + 1u, block);
		unsigned int len0 = _len(i, full);
		unsigned int len1 = _len(i + 1u, full);
		uint8_t key0[288], key1[288];
		uint8_t d0[64], d1[64], s0[64], s1[64];
		uint32_t seed = 0x51E0D00Du ^ (i * 0x9E3779B9u);
		int hw_ok;

		if(k0 > sizeof(key0) || k1 > sizeof(key1) || dig > 64u)
			return(NO);
		_fill(key0, k0, &seed);
		_fill(key1, k1, &seed);
		memset(&hw0, 0, sizeof(hw0));
		memset(&hw1, 0, sizeof(hw1));
		hw_ok = mp_crypto_hmac_init(&hw0, name, key0, k0) == TRUE && hw0.onHardware == YES;
		if(!hw_ok) {
			_hmac_drop(&hw0);
			_hmac_drop(&hw1);
			memset(key0, 0, sizeof(key0));
			memset(key1, 0, sizeof(key1));
			continue;
		}
		if(mp_crypto_hmac_init(&hw1, name, key1, k1) != TRUE || hw1.onHardware != YES) {
			_hmac_drop(&hw0);
			_hmac_drop(&hw1);
			memset(key0, 0, sizeof(key0));
			memset(key1, 0, sizeof(key1));
			return(NO);
		}
		_feed_pair(&hw0, &hw1, _hmac_hw, len0, len1, _seed(i, 0), _seed(i, 1), i, block);
		{
			unsigned int n0 = mp_crypto_hmac_final(&hw0, d0);
			unsigned int n1 = mp_crypto_hmac_final(&hw1, d1);

			if(n0 != dig || n1 != dig) {
				memset(key0, 0, sizeof(key0));
				memset(key1, 0, sizeof(key1));
				return(NO);
			}
		}
		memset(&sw0, 0, sizeof(sw0));
		memset(&sw1, 0, sizeof(sw1));
		if(mp_crypto_hmac_soft_init(&sw0, name, key0, k0) != TRUE || mp_crypto_hmac_soft_init(&sw1, name, key1, k1) != TRUE) {
			mp_crypto_hmac_free(&sw0);
			mp_crypto_hmac_free(&sw1);
			memset(key0, 0, sizeof(key0));
			memset(key1, 0, sizeof(key1));
			return(NO);
		}
		_feed_pair(&sw0, &sw1, _hmac_sw, len0, len1, _seed(i, 0), _seed(i, 1), i, block);
		{
			unsigned int n0 = mp_crypto_hmac_soft_final(&sw0, s0);
			unsigned int n1 = mp_crypto_hmac_soft_final(&sw1, s1);

			if(n0 != dig || n1 != dig || mp_crypto_selftest_equal(d0, s0, dig) != YES || mp_crypto_selftest_equal(d1, s1, dig) != YES) {
				memset(key0, 0, sizeof(key0));
				memset(key1, 0, sizeof(key1));
				return(NO);
			}
		}
		memset(key0, 0, sizeof(key0));
		memset(key1, 0, sizeof(key1));
		memset(d0, 0, sizeof(d0));
		memset(d1, 0, sizeof(d1));
	}
	return(YES);
}

static mp_bool_t _hmac_cases(int full) {
	if(_hmac_algo("SHA256", MP_CRYPTO_SHA256_SIZE, MP_CRYPTO_SHA256_BLOCK, full) != YES)
		return(NO);
#if defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1)
	if(_hmac_algo("SHA1", MP_CRYPTO_SHA1_SIZE, MP_CRYPTO_SHA1_BLOCK, full) != YES)
		return(NO);
#endif
#if defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384)
	if(_hmac_algo("SHA384", MP_CRYPTO_SHA384_SIZE, MP_CRYPTO_SHA512_BLOCK, full) != YES)
		return(NO);
#endif
#if defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512)
	if(_hmac_algo("SHA512", MP_CRYPTO_SHA512_SIZE, MP_CRYPTO_SHA512_BLOCK, full) != YES)
		return(NO);
#endif
	return(YES);
}

static mp_bool_t _hmac_short(void) {
	return(_hmac_cases(0));
}

mp_bool_t mp_crypto_selftest_hw_hmac(void) {
	if(_hmac_cases(1) != YES)
		return(NO);
	return(_other_clock(_hmac_short));
}

#endif

void mp_crypto_selftest_hw_kernel(mp_kernel_t *kernel) {
	_kernel = kernel;
}

#else

void mp_crypto_selftest_hw_kernel(mp_kernel_t *kernel) {
	(void)kernel;
}

#endif

#endif
