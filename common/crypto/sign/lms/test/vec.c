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

/* Host round-trip. Not linked into the firmware.
 * LMS_SHA256_M32_H5 / LMOTS_SHA256_N32_W8 has no compact published vector here.
 * The caller advances q. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH_SHA256
#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_SIGN_LMS
#include <mp.h>
#include <crypto/sha256.h>
#include <crypto/lock.h>
#include <crypto/lms.h>
#include <hostcrypto.h>

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../hash/sha256/sha256.c"
#include "../../../lock/lock.c"
#include "../lms.c"

int main(void) {
	mp_crypto_lms_priv_t sk;
	uint8_t seed[32];
	uint8_t pub[MP_CRYPTO_LMS_PUB];
	uint8_t sig[MP_CRYPTO_LMS_SIG];
	uint8_t sig2[MP_CRYPTO_LMS_SIG];
	uint8_t bad[MP_CRYPTO_LMS_SIG];
	uint8_t pk[MP_CRYPTO_LMS_PUB];

	memset(seed, 0x11, sizeof(seed));
	if(mp_crypto_lms_keypair(seed, 32, &sk, pub) != TRUE)
		return(host_fail("lms keypair"));
	if(mp_crypto_lms_sign(&sk, "miniPhi", 7, sig) != MP_CRYPTO_LMS_SIG)
		return(host_fail("lms sign"));
	if(mp_crypto_lms_verify(pub, "miniPhi", 7, sig, MP_CRYPTO_LMS_SIG) != YES)
		return(host_fail("lms verify"));
	host_valid_n++;
	sk.q++;
	if(mp_crypto_lms_sign(&sk, "miniPhi", 7, sig2) != MP_CRYPTO_LMS_SIG)
		return(host_fail("lms sign 2"));
	if(memcmp(sig, sig2, MP_CRYPTO_LMS_SIG) == 0)
		return(host_fail("lms q"));
	if(mp_crypto_lms_verify(pub, "miniPhi", 7, sig2, MP_CRYPTO_LMS_SIG) != YES)
		return(host_fail("lms verify 2"));
	if(mp_crypto_lms_verify(pub, "miniPhi", 7, sig, MP_CRYPTO_LMS_SIG) != YES)
		return(host_fail("lms verify 1"));
	memcpy(bad, sig, sizeof(bad));
	bad[20] ^= 1;
	if(mp_crypto_lms_verify(pub, "miniPhi", 7, bad, MP_CRYPTO_LMS_SIG) == YES)
		return(host_fail("lms sig"));
	if(mp_crypto_lms_verify(pub, "miniPhj", 7, sig, MP_CRYPTO_LMS_SIG) == YES)
		return(host_fail("lms msg"));
	memcpy(pk, pub, sizeof(pk));
	pk[30] ^= 1;
	if(mp_crypto_lms_verify(pk, "miniPhi", 7, sig, MP_CRYPTO_LMS_SIG) == YES)
		return(host_fail("lms pk"));
	host_invalid_n += 3;
	host_report("lms");
	return(0);
}
