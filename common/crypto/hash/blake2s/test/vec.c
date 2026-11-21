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

#define SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
#include <mp.h>
#include <crypto/blake2s.h>
#include <hostcrypto.h>
#include "vectors_ref.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../blake2s.c"

static const uint8_t *keyed;

static void init(void *c) {
	if(keyed)
		mp_crypto_blake2s_init_key(c, keyed, 3);
	else
		mp_crypto_blake2s_init(c);
}

static void update(void *c, const void *p, unsigned int n) {
	mp_crypto_blake2s_update(c, p, n);
}

static void final(void *c, uint8_t *d) {
	mp_crypto_blake2s_final(c, d);
}

static void shot(const void *p, unsigned int n, uint8_t *d) {
	mp_crypto_blake2s_t ctx;

	init(&ctx);
	update(&ctx, p, n);
	final(&ctx, d);
}

int main(void) {
	unsigned int i;
	uint8_t dig[32];
	mp_crypto_blake2s_t ctx;
	static const uint8_t key[3] = { 'k', 'e', 'y' };

	for(i = 0; i < B2_COUNT; i++) {
		const uint8_t *k = b2_at(i, 1);

		keyed = b2_vec[i].n[1] ? k : NULL;
		shot(b2_at(i, 0), b2_vec[i].n[0], dig);
		if(b2_vec[i].n[2] != 32 || !host_mem(dig, b2_at(i, 2), 32)) {
			fprintf(stderr, "blake2s %u\n", b2_vec[i].id);
			return(1);
		}
		host_valid_n++;
	}
	keyed = NULL;
	if(host_chunk("blake2s", MP_CRYPTO_BLAKE2S_BLOCK, 32, &ctx, init, update, final, shot))
		return(1);
	keyed = key;
	if(host_chunk("blake2s-key", MP_CRYPTO_BLAKE2S_BLOCK, 32, &ctx, init, update, final, shot))
		return(1);
	host_report("blake2s");
	return(0);
}
