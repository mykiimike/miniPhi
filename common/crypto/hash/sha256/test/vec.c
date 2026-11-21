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

#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#include <mp.h>
#include <crypto/sha256.h>
#include <hostcrypto.h>
#include "vectors_ref.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../sha256.c"

static void init(void *c) {
	mp_crypto_sha256_init(c);
}

static void update(void *c, const void *p, unsigned int n) {
	mp_crypto_sha256_update(c, p, n);
}

static void final(void *c, uint8_t *d) {
	mp_crypto_sha256_final(c, d);
}

static void shot(const void *p, unsigned int n, uint8_t *d) {
	mp_crypto_sha256(p, n, d);
}

int main(void) {
	unsigned int i;
	uint8_t dig[32];
	uint8_t alt[32];
	mp_crypto_sha256_t ctx;

	for(i = 0; i < SHA256_COUNT; i++) {
		shot(sha256_at(i, 0), sha256_vec[i].n[0], dig);
		if(sha256_vec[i].n[1] != 32 || !host_mem(dig, sha256_at(i, 1), 32)) {
			fprintf(stderr, "sha256 %u\n", sha256_vec[i].id);
			return(1);
		}
		host_valid_n++;
	}
	shot("abc", 3, dig);
	shot("abd", 3, alt);
	if(host_mem(dig, alt, 32))
		return(host_fail("sha256 flip"));
	if(host_chunk("sha256", MP_CRYPTO_SHA256_BLOCK, 32, &ctx, init, update, final, shot))
		return(1);
	host_report("sha256");
	return(0);
}
