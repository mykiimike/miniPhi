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

/* Host rejection checks on the stored 3072-bit key. Not linked into the firmware.
 * Out of range (0 or >= n) the secret is SHAKE256 of the ciphertext.
 * An in-range ciphertext follows the KDF. 4096-bit RSA-KEM has no stored key. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH_SHAKE
#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_BN
#define SUPPORT_COMMON_CRYPTO_SIGN_RSA
#define SUPPORT_COMMON_CRYPTO_KEM_RSAKEM
#define MP_CRYPTO_RSA_BITS 3072
#define MP_CRYPTO_SELFTEST_RSAKEM
#define SUPPORT_COMMON_CRYPTO_SELFTEST
#include <mp.h>
#include <crypto/shake.h>
#include <crypto/lock.h>
#include <crypto/bn.h>
#include <crypto/rsa.h>
#include <crypto/rsakem.h>
#include <crypto/selftest.h>
#include <hostcrypto.h>

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

mp_ret_t mp_crypto_rsa_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	(void)seed;
	(void)seedlen;
	(void)priv;
	(void)pub;
	return(FALSE);
}

#include "../../../hash/shake/shake.c"
#include "../../../lock/lock.c"
#define _wide _bn_wide
#include "../../../bn/bn.c"
#undef _wide
#include "../rsa.c"
#define _ss _kat_ss
#include "../../../selftest/kem/rsakem.c"
#undef _ss

int main(void) {
	uint8_t z[MP_CRYPTO_RSA_MODULUS];
	uint8_t ct[MP_CRYPTO_RSA_MODULUS];
	uint8_t ss[32];
	uint8_t expect[32];
	uint8_t back[32];

	memcpy(ct, _priv, MP_CRYPTO_RSA_MODULUS);
	mp_crypto_shake256(expect, 32, ct, sizeof(ct));
	if(mp_crypto_rsakem_decaps(_priv, ct, sizeof(ct), ss) != YES || memcmp(ss, expect, 32) != 0)
		return(host_fail("rsakem n"));
	host_invalid_n++;
	memset(ct, 0, sizeof(ct));
	mp_crypto_shake256(expect, 32, ct, sizeof(ct));
	if(mp_crypto_rsakem_decaps(_priv, ct, sizeof(ct), ss) != YES || memcmp(ss, expect, 32) != 0)
		return(host_fail("rsakem 0"));
	host_invalid_n++;
	/* z = 2. n-1 is a fixed point of an odd exponent, so it is a poor probe. */
	memset(z, 0, sizeof(z));
	z[sizeof(z) - 1] = 2;
	if(mp_crypto_rsakem_encaps(_priv, z, sizeof(z), ss, ct) != sizeof(ct))
		return(host_fail("rsakem encaps"));
	mp_crypto_shake256(expect, 32, z, sizeof(z));
	if(memcmp(ss, expect, 32) != 0)
		return(host_fail("rsakem kdf"));
	if(mp_crypto_rsakem_decaps(_priv, ct, sizeof(ct), back) != YES || memcmp(back, ss, 32) != 0)
		return(host_fail("rsakem in range"));
	ct[0] ^= 1;
	if(mp_crypto_rsakem_decaps(_priv, ct, sizeof(ct), back) != YES || memcmp(back, ss, 32) == 0)
		return(host_fail("rsakem flip"));
	host_valid_n++;
	memset(back, 0xa5, sizeof(back));
	if(mp_crypto_rsakem_decaps(_priv, ct, sizeof(ct) - 1, back) != NO || back[0] != 0xa5)
		return(host_fail("rsakem short"));
	host_invalid_n++;
	host_report("rsakem");
	return(0);
}
