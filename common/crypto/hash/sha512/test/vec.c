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

#define SUPPORT_COMMON_CRYPTO_HASH_SHA512
#include <mp.h>
#include <crypto/sha512.h>
#include <hostcrypto.h>
#include "vectors_ref.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../sha512.c"

static void init(void *c) {
	mp_crypto_sha512_init(c);
}

static void update(void *c, const void *p, unsigned int n) {
	mp_crypto_sha512_update(c, p, n);
}

static void final(void *c, uint8_t *d) {
	mp_crypto_sha512_final(c, d);
}

static void shot(const void *p, unsigned int n, uint8_t *d) {
	mp_crypto_sha512(p, n, d);
}

int main(void) {
	unsigned int i;
	uint8_t dig[64];
	uint8_t alt[64];
	mp_crypto_sha512_t ctx;

	for(i = 0; i < SHA512_COUNT; i++) {
		shot(sha512_at(i, 0), sha512_vec[i].n[0], dig);
		if(sha512_vec[i].n[1] != 64 || !host_mem(dig, sha512_at(i, 1), 64)) {
			fprintf(stderr, "sha512 %u\n", sha512_vec[i].id);
			return(1);
		}
		host_valid_n++;
	}
	shot("abc", 3, dig);
	shot("abd", 3, alt);
	if(host_mem(dig, alt, 64))
		return(host_fail("sha512 flip"));
	if(host_chunk("sha512", MP_CRYPTO_SHA512_BLOCK, 64, &ctx, init, update, final, shot))
		return(1);
	host_report("sha512");
	return(0);
}
