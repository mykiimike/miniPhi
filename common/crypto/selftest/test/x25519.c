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

/* Host run of the on-device known answer. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#define SUPPORT_COMMON_CRYPTO_HASH
#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#define SUPPORT_COMMON_CRYPTO_MAC_HMAC
#define SUPPORT_COMMON_CRYPTO_KDF_HKDF
#define SUPPORT_COMMON_CRYPTO_KEM
#define SUPPORT_COMMON_CRYPTO_KEM_X25519
#define SUPPORT_COMMON_CRYPTO_SELFTEST
#define MP_CRYPTO_SELFTEST_X25519
#include <mp.h>
#include <crypto/sha256.h>
#include <crypto/hash.h>
#include <crypto/hmac.h>
#include <crypto/hkdf.h>
#include <crypto/x25519.h>
#include <crypto/kem.h>
#include <crypto/selftest.h>

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../hash/sha256/sha256.c"
#include "../../hash/index.c"
#include "../../mac/hmac/hmac.c"
#include "../../kdf/hkdf/hkdf.c"
#include "../../kem/dhkem.c"
#include "../../kem/x25519/x25519.c"
#include "../kem/x25519.c"
int main(void) {
	if(mp_crypto_selftest_x25519() != YES) {
		fprintf(stderr, "selftest X25519 failed\n");
		return(1);
	}
	return(0);
}
