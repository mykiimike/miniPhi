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

/* Host vector. Not linked into the firmware.
 * SLH-DSA-SHA2-128s. Deterministic sign must match OpenSSL byte for byte. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#define SUPPORT_COMMON_CRYPTO_SIGN_SLH
#include <mp.h>
#include <crypto/sha256.h>
#include <crypto/slh.h>
#include <hostcrypto.h>
#include "vectors_ref.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../hash/sha256/sha256.c"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../slh.c"
#pragma GCC diagnostic pop

int main(void) {
	uint8_t priv[MP_CRYPTO_SLH_SK_SIZE];
	uint8_t pub[MP_CRYPTO_SLH_PK_SIZE];
	uint8_t sig[MP_CRYPTO_SLH_SIG_SIZE];
	uint8_t bad[MP_CRYPTO_SLH_SIG_SIZE];
	uint8_t pk[MP_CRYPTO_SLH_PK_SIZE];
	const uint8_t *msg;
	unsigned int n;

	if(slh_vec[0].n[0] != 48 || slh_vec[0].n[1] != MP_CRYPTO_SLH_SK_SIZE || slh_vec[0].n[2] != MP_CRYPTO_SLH_PK_SIZE || slh_vec[0].n[4] != MP_CRYPTO_SLH_SIG_SIZE)
		return(host_fail("slh shape"));
	msg = slh_at(0, 3);
	if(mp_crypto_slh_keypair(slh_at(0, 0), 48, priv, pub) != TRUE)
		return(host_fail("slh keypair"));
	if(memcmp(priv, slh_at(0, 1), MP_CRYPTO_SLH_SK_SIZE) != 0)
		return(host_fail("slh priv"));
	if(memcmp(pub, slh_at(0, 2), MP_CRYPTO_SLH_PK_SIZE) != 0)
		return(host_fail("slh pub"));
	if(mp_crypto_slh_verify(pub, msg, slh_vec[0].n[3], slh_at(0, 4), slh_vec[0].n[4]) != YES)
		return(host_fail("slh openssl"));
	n = mp_crypto_slh_sign(priv, msg, slh_vec[0].n[3], sig);
	if(n != MP_CRYPTO_SLH_SIG_SIZE || memcmp(sig, slh_at(0, 4), n) != 0)
		return(host_fail("slh bytes"));
	if(mp_crypto_slh_verify(pub, msg, slh_vec[0].n[3], sig, n) != YES)
		return(host_fail("slh sign"));
	host_valid_n++;
	memcpy(bad, slh_at(0, 4), n);
	bad[40] ^= 1;
	if(mp_crypto_slh_verify(pub, msg, slh_vec[0].n[3], bad, n) == YES)
		return(host_fail("slh sig"));
	if(slh_vec[0].n[3] && mp_crypto_slh_verify(pub, "miniPhj", slh_vec[0].n[3], slh_at(0, 4), n) == YES)
		return(host_fail("slh msg"));
	memcpy(pk, pub, sizeof(pk));
	pk[0] ^= 1;
	if(mp_crypto_slh_verify(pk, msg, slh_vec[0].n[3], slh_at(0, 4), n) == YES)
		return(host_fail("slh pk"));
	host_invalid_n += 3;
	host_report("slh");
	return(0);
}
