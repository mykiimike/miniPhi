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

/* Host vectors. Not linked into the firmware. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH_SHAKE
#include <mp.h>
#include <crypto/shake.h>
#include <hostcrypto.h>
#include "vectors_sha3.h"
#include "vectors_sha3_512.h"
#include "vectors_shake.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../shake.c"

static void i256(void *c) { mp_crypto_sha3_256_init(c); }
static void u256(void *c, const void *p, unsigned int n) { mp_crypto_sha3_256_update(c, p, n); }
static void f256(void *c, uint8_t *d) { mp_crypto_sha3_256_final(c, d); }
static void s256(const void *p, unsigned int n, uint8_t *d) { mp_crypto_sha3_256(p, n, d); }

static void i512(void *c) { mp_crypto_sha3_512_init(c); }
static void u512(void *c, const void *p, unsigned int n) { mp_crypto_sha3_512_update(c, p, n); }
static void f512(void *c, uint8_t *d) { mp_crypto_sha3_512_final(c, d); }
static void s512(const void *p, unsigned int n, uint8_t *d) { mp_crypto_sha3_512(p, n, d); }

/* Absorb splits and squeeze splits against one mp_crypto_shake128/256 call. */
static int shake_chunk(int wide) {
	uint8_t msg[1100];
	uint8_t expect[64];
	uint8_t got[64];
	unsigned int lens[7];
	unsigned int block = wide ? 136u : 168u;
	unsigned int li;

	host_fill(msg, sizeof(msg));
	lens[0] = 0;
	lens[1] = 1;
	lens[2] = block - 1;
	lens[3] = block;
	lens[4] = block + 1;
	lens[5] = block * 2 + 1;
	lens[6] = 1009;
	for(li = 0; li < 7; li++) {
		unsigned int len = lens[li];
		unsigned int off;
		unsigned int i;
		mp_crypto_shake128_t ctx;

		if(wide)
			mp_crypto_shake256(expect, 32, msg, len);
		else
			mp_crypto_shake128(expect, 32, msg, len);
		for(off = 0; off <= len; off++) {
			if(wide) {
				mp_crypto_shake256_init(&ctx);
				mp_crypto_shake256_absorb(&ctx, msg, off);
				mp_crypto_shake256_absorb(&ctx, msg + off, len - off);
				mp_crypto_shake256_finalize(&ctx);
				mp_crypto_shake256_squeeze(&ctx, got, 10);
				mp_crypto_shake256_squeeze(&ctx, got + 10, 22);
			}
			else {
				mp_crypto_shake128_init(&ctx);
				mp_crypto_shake128_absorb(&ctx, msg, off);
				mp_crypto_shake128_absorb(&ctx, msg + off, len - off);
				mp_crypto_shake128_finalize(&ctx);
				mp_crypto_shake128_squeeze(&ctx, got, 10);
				mp_crypto_shake128_squeeze(&ctx, got + 10, 22);
			}
			if(!host_mem(expect, got, 32))
				return(host_fail("shake"));
		}
		if(wide)
			mp_crypto_shake256_init(&ctx);
		else
			mp_crypto_shake128_init(&ctx);
		for(i = 0; i < len; i++) {
			if(wide)
				mp_crypto_shake256_absorb(&ctx, msg + i, 1);
			else
				mp_crypto_shake128_absorb(&ctx, msg + i, 1);
		}
		if(wide) {
			mp_crypto_shake256_finalize(&ctx);
			mp_crypto_shake256_squeeze(&ctx, got, 1);
			mp_crypto_shake256_squeeze(&ctx, got + 1, 31);
		}
		else {
			mp_crypto_shake128_finalize(&ctx);
			mp_crypto_shake128_squeeze(&ctx, got, 1);
			mp_crypto_shake128_squeeze(&ctx, got + 1, 31);
		}
		if(!host_mem(expect, got, 32))
			return(host_fail("shake"));
	}
	return(0);
}

int main(void) {
	unsigned int i;
	uint8_t dig[64];
	mp_crypto_sha3_256_t a;
	mp_crypto_sha3_512_t b;

	for(i = 0; i < SHA3_256_COUNT; i++) {
		s256(sha3_256_at(i, 0), sha3_256_vec[i].n[0], dig);
		if(sha3_256_vec[i].n[1] != 32 || !host_mem(dig, sha3_256_at(i, 1), 32)) {
			fprintf(stderr, "sha3-256 %u\n", sha3_256_vec[i].id);
			return(1);
		}
		host_valid_n++;
	}
	for(i = 0; i < SHA3_512_COUNT; i++) {
		s512(sha3_512_at(i, 0), sha3_512_vec[i].n[0], dig);
		if(sha3_512_vec[i].n[1] != 64 || !host_mem(dig, sha3_512_at(i, 1), 64)) {
			fprintf(stderr, "sha3-512 %u\n", sha3_512_vec[i].id);
			return(1);
		}
		host_valid_n++;
	}
	for(i = 0; i < SHAKE_COUNT; i++) {
		unsigned int n = shake_vec[i].n[1];

		if(shake_vec[i].id == 2)
			mp_crypto_shake256(dig, n, shake_at(i, 0), shake_vec[i].n[0]);
		else
			mp_crypto_shake128(dig, n, shake_at(i, 0), shake_vec[i].n[0]);
		if(!host_mem(dig, shake_at(i, 1), n)) {
			fprintf(stderr, "shake %u\n", shake_vec[i].id);
			return(1);
		}
		host_valid_n++;
	}
	if(host_chunk("sha3-256", 136, 32, &a, i256, u256, f256, s256))
		return(1);
	if(host_chunk("sha3-512", 72, 64, &b, i512, u512, f512, s512))
		return(1);
	if(shake_chunk(0) || shake_chunk(1))
		return(1);
	host_report("shake");
	return(0);
}
