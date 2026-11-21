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

#define SUPPORT_COMMON_CRYPTO_MAC_POLY1305
#include <mp.h>
#include <crypto/poly1305.h>
#include <hostcrypto.h>
#include "vectors_rfc.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../poly1305.c"

static const uint8_t *pkey;

static void init(void *c) {
	mp_crypto_poly1305_init(c, pkey, 32);
}

static void update(void *c, const void *p, unsigned int n) {
	mp_crypto_poly1305_update(c, p, n);
}

static void final(void *c, uint8_t *d) {
	mp_crypto_poly1305_final(c, d);
}

static void shot(const void *p, unsigned int n, uint8_t *d) {
	mp_crypto_poly1305_t ctx;

	init(&ctx);
	update(&ctx, p, n);
	final(&ctx, d);
}

int main(void) {
	unsigned int i;
	uint8_t tag[16];
	uint8_t bad[32];
	mp_crypto_poly1305_t ctx;

	for(i = 0; i < POLY_COUNT; i++) {
		pkey = poly_at(i, 0);
		shot(poly_at(i, 1), poly_vec[i].n[1], tag);
		if(!host_mem(tag, poly_at(i, 2), 16))
			return(host_fail("poly1305"));
		host_valid_n++;
	}
	tag[0] ^= 1;
	if(host_mem(tag, poly_at(0, 2), 16))
		return(host_fail("poly flip"));
	memset(bad, 1, sizeof(bad));
	if(mp_crypto_poly1305_init(&ctx, bad, 31) != FALSE)
		return(host_fail("poly key"));
	host_invalid_n++;
	if(host_chunk("poly1305", 16, 16, &ctx, init, update, final, shot))
		return(1);
	host_report("poly1305");
	return(0);
}
