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
 * No Wycheproof set for HQC. A modified ciphertext decapsulates to the
 * implicit-rejection secret; this checks it is stable and not the honest one. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define SUPPORT_COMMON_CRYPTO_HASH_SHAKE
#define SUPPORT_COMMON_CRYPTO_LOCK
#define SUPPORT_COMMON_CRYPTO_KEM_HQC
#include <mp.h>
#include <crypto/shake.h>
#include <crypto/lock.h>
#include <crypto/hqc.h>
#include <hostcrypto.h>

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#include "../../../hash/shake/shake.c"
#include "../../../lock/lock.c"
#include "../hqc.c"

int main(void) {
	uint8_t seed[MP_CRYPTO_HQC_SEED];
	uint8_t coins[MP_CRYPTO_HQC_COINS];
	uint8_t priv[MP_CRYPTO_HQC_PRIV];
	uint8_t pub[MP_CRYPTO_HQC_PUB];
	uint8_t ct[MP_CRYPTO_HQC_CT];
	uint8_t ss[MP_CRYPTO_HQC_SS];
	uint8_t back[MP_CRYPTO_HQC_SS];
	uint8_t rej[MP_CRYPTO_HQC_SS];

	memset(seed, 0x11, sizeof(seed));
	memset(coins, 0x22, sizeof(coins));
	if(mp_crypto_hqc_keypair(seed, sizeof(seed), priv, pub) != TRUE)
		return(host_fail("hqc keypair"));
	if(mp_crypto_hqc_encaps(pub, coins, sizeof(coins), ss, ct) != MP_CRYPTO_HQC_CT)
		return(host_fail("hqc encaps"));
	if(mp_crypto_hqc_decaps(priv, ct, MP_CRYPTO_HQC_CT, back) != YES || memcmp(back, ss, sizeof(ss)) != 0)
		return(host_fail("hqc decaps"));
	host_valid_n++;
	memset(back, 0xa5, sizeof(back));
	if(mp_crypto_hqc_decaps(priv, ct, MP_CRYPTO_HQC_CT - 1, back) != NO || back[0] != 0xa5)
		return(host_fail("hqc short"));
	host_invalid_n++;
	ct[100] ^= 1;
	if(mp_crypto_hqc_decaps(priv, ct, MP_CRYPTO_HQC_CT, rej) != YES || memcmp(rej, ss, sizeof(ss)) == 0)
		return(host_fail("hqc flip"));
	if(mp_crypto_hqc_decaps(priv, ct, MP_CRYPTO_HQC_CT, back) != YES || memcmp(back, rej, sizeof(ss)) != 0)
		return(host_fail("hqc flip again"));
	host_invalid_n++;
	host_report("hqc");
	return(0);
}
