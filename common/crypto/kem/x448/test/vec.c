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
 * Raw RFC 7748. A public key that is not 56 bytes is refused without being read. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH
#define SUPPORT_COMMON_CRYPTO_HASH_SHA512
#define SUPPORT_COMMON_CRYPTO_MAC_HMAC
#define SUPPORT_COMMON_CRYPTO_KDF_HKDF
#define SUPPORT_COMMON_CRYPTO_KEM
#define SUPPORT_COMMON_CRYPTO_KEM_X448
#include <mp.h>
#include <crypto/sha512.h>
#include <crypto/hash.h>
#include <crypto/hmac.h>
#include <crypto/hkdf.h>
#include <crypto/x448.h>
#include <crypto/kem.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../hash/sha512/sha512.c"
#include "../../../hash/index.c"
#include "../../../mac/hmac/hmac.c"
#include "../../../kdf/hkdf/hkdf.c"
#include "../../dhkem.c"
#include "../x448.c"

int main(void) {
	unsigned int i;
	uint8_t out[56];

	for(i = 0; i < X448_COUNT; i++) {
		int accepted = 0;

		if(x448_vec[i].n[0] == 56 && x448_vec[i].n[1] == 56 && x448_vec[i].n[2] == 56) {
			mp_crypto_x448(out, x448_at(i, 0), x448_at(i, 1));
			accepted = memcmp(out, x448_at(i, 2), 56) == 0;
		}
		else if(x448_vec[i].result == 1)
			return(host_fail("x448 length"));
		if(host_expect("x448", x448_vec[i].result, accepted)) {
			fprintf(stderr, "id %u\n", x448_vec[i].id);
			return(1);
		}
	}
	host_report("x448");
	return(0);
}
