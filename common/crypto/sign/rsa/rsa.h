/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 * Copyright (C) 2026  VERMAN                                              *
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
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_RSA

#ifndef _HAVE_MP_COMMON_RSA_H
	#define _HAVE_MP_COMMON_RSA_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonRsa
	 * @{
	 */

	#ifndef MP_RSA_BITS
		#error MP_RSA_BITS is missing
	#endif
	#if MP_RSA_BITS < 3072
		#error RSA modulus is below 3072 bits
	#endif
	#if (MP_RSA_BITS % 64) != 0
		#error RSA modulus must be a multiple of 64 bits
	#endif

	/** modulus bytes, and one prime */
	#define MP_RSA_MODULUS (MP_RSA_BITS / 8)
	#define MP_RSA_PRIME (MP_RSA_BITS / 16)
	#define MP_RSA_WMOD (MP_RSA_BITS / 32)
	#define MP_RSA_WPRIME (MP_RSA_BITS / 64)
	#define MP_RSA_E 4
	/** n || e, e a big-endian 65537 */
	#define MP_RSA_PUB (MP_RSA_MODULUS + MP_RSA_E)
	/** n || e || d || p || q || dp || dq || qinv */
	#define MP_RSA_PRIV (MP_RSA_MODULUS + MP_RSA_E + MP_RSA_MODULUS + 5 * MP_RSA_PRIME)
	#define MP_RSA_SIG MP_RSA_MODULUS

	#define MP_RSA_OFF_E MP_RSA_MODULUS
	#define MP_RSA_OFF_D (MP_RSA_OFF_E + MP_RSA_E)
	#define MP_RSA_OFF_P (MP_RSA_OFF_D + MP_RSA_MODULUS)
	#define MP_RSA_OFF_Q (MP_RSA_OFF_P + MP_RSA_PRIME)
	#define MP_RSA_OFF_DP (MP_RSA_OFF_Q + MP_RSA_PRIME)
	#define MP_RSA_OFF_DQ (MP_RSA_OFF_DP + MP_RSA_PRIME)
	#define MP_RSA_OFF_QINV (MP_RSA_OFF_DQ + MP_RSA_PRIME)

	/** @} */

	/**
	 * Two primes of MP_RSA_BITS / 2 from seed, which is at least 32 bytes of
	 * entropy. FALSE when the seed is short or no prime shows up. Far too
	 * slow for the screen.
	 */
	mp_ret_t mp_rsa_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	/** PKCS#1 v1.5 SHA-256. Writes MP_RSA_SIG bytes and returns that, or 0. */
	unsigned int mp_rsa_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig);
	mp_bool_t mp_rsa_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen);

#endif
#endif
