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

/* Host vectors. Not linked into the firmware. PKCS#1 v1.5 SHA-256, 3072-bit. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_BN
#define SUPPORT_COMMON_CRYPTO_HASH
#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#define SUPPORT_COMMON_CRYPTO_MAC_HMAC
#define SUPPORT_COMMON_CRYPTO_RNG_HMAC_DRBG
#define SUPPORT_COMMON_CRYPTO_SIGN_RSA
#define MP_CRYPTO_RSA_BITS 3072
#include <mp.h>
#include <crypto/sha256.h>
#include <crypto/hash.h>
#include <crypto/hmac.h>
#include <crypto/hmac_drbg.h>
#include <crypto/lock.h>
#include <crypto/bn.h>
#include <crypto/rsa.h>
#include <hostcrypto.h>
#include "vectors_wycheproof.h"

#define RSA_COUNT RSA3072_COUNT
#define rsa_vec rsa3072_vec
#define rsa_at rsa3072_at

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../lock/lock.c"
#define _wide _bn_wide
#include "../../../bn/bn.c"
#undef _wide
#include "../../../hash/sha256/sha256.c"
#include "../../../hash/index.c"
#include "../../../mac/hmac/hmac.c"
#include "../../../rng/hmac_drbg/hmac_drbg.c"
#include "../rsa.c"
#include "sig.inc"
