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
#define SUPPORT_COMMON_CRYPTO_HASH_SHAKE
#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_BN
#define SUPPORT_COMMON_CRYPTO_SIGN_RSA
#define MP_CRYPTO_RSA_BITS 3072
#define SUPPORT_COMMON_CRYPTO_KEM_RSAKEM
#define SUPPORT_COMMON_CRYPTO_SELFTEST
#define MP_CRYPTO_SELFTEST_RSAKEM
#include <mp.h>
#include <crypto/shake.h>
#include <crypto/lock.h>
#include <crypto/bn.h>
#include <crypto/rsa.h>
#include <crypto/rsakem.h>
#include <crypto/selftest.h>

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../hash/shake/shake.c"
#include "../../lock/lock.c"
#define _wide _bn_wide
#include "../../bn/bn.c"
#undef _wide
#include "../../kem/rsa/rsa.c"
#define _ss _kat_ss
#include "../kem/rsakem.c"
#undef _ss
/* keypair lives in sign/rsa and is not what this known answer calls */
mp_ret_t mp_crypto_rsa_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	(void)seed;
	(void)seedlen;
	(void)priv;
	(void)pub;
	return(FALSE);
}
int main(void) {
	if(mp_crypto_selftest_rsakem() != YES) {
		fprintf(stderr, "selftest RSAKEM failed\n");
		return(1);
	}
	return(0);
}
