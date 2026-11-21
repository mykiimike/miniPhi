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

/* Host vectors. Not linked into the firmware.
 * AES is one block per call, not a stream. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_CIPHER_AES
#define MP_CRYPTO_AES_BITS 256
#include <mp.h>
#include <crypto/aes.h>
#include <hostcrypto.h>
#include "vectors_ref.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../aes.c"

int main(void) {
	unsigned int i;
	uint8_t ct[16];
	uint8_t pt[16];
	uint8_t bad[16];
	mp_crypto_aes_t ctx;

	for(i = 0; i < AES_COUNT; i++) {
		unsigned int k = aes_vec[i].n[0];

		if(mp_crypto_aes_init(&ctx, aes_at(i, 0), k) != TRUE)
			return(host_fail("aes init"));
		if(mp_crypto_aes_update_block(&ctx, aes_at(i, 1), 16, ct) != 16 || !host_mem(ct, aes_at(i, 2), 16)) {
			fprintf(stderr, "aes %u\n", aes_vec[i].id);
			return(1);
		}
		if(mp_crypto_aes_update_block_inv(&ctx, ct, 16, pt) != 16 || !host_mem(pt, aes_at(i, 1), 16))
			return(host_fail("aes inv"));
		mp_crypto_aes_free(&ctx);
		host_valid_n++;
	}
	memset(bad, 1, sizeof(bad));
	if(mp_crypto_aes_init(&ctx, bad, 15) != FALSE || mp_crypto_aes_init(&ctx, bad, 33) != FALSE)
		return(host_fail("aes keylen"));
	host_invalid_n += 2;
	host_report("aes");
	return(0);
}
