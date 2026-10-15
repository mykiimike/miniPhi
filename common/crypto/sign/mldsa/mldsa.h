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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_MLDSA

#ifndef _HAVE_MP_COMMON_MLDSA_H
	#define _HAVE_MP_COMMON_MLDSA_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonMldsa
	 * @{
	 */

	/* ML-DSA (FIPS 204). The security level is a build setting picked by
	 * MP_MLDSA_LEVEL (2, 3 or 5). Common to all levels: q = 8380417, n = 256,
	 * the 512th root of unity zeta = 1753, d = 13 (dropped bits). Sizes:
	 *   level 2 (ML-DSA-44): k=4, l=4, eta=2, tau=39, pk=1312, sk=2560, sig=2420
	 *   level 3 (ML-DSA-65): k=6, l=5, eta=4, tau=49, pk=1952, sk=4032, sig=3309
	 *   level 5 (ML-DSA-87): k=8, l=7, eta=2, tau=60, pk=2592, sk=4896, sig=4627
	 * Constant time: the NTT and Montgomery reduction are fixed functions of
	 * the full state, the rejection loop uses constant-time selection, and no
	 * branch or table index reads a secret. */

	#define MP_MLDSA_Q    8380417
	#define MP_MLDSA_N    256
	#define MP_MLDSA_ZETA 1753
	#define MP_MLDSA_D    13

	#if !defined(MP_MLDSA_LEVEL)
		#define MP_MLDSA_LEVEL 2
	#endif

	#if MP_MLDSA_LEVEL == 2
		#define MP_MLDSA_K    4
		#define MP_MLDSA_L    4
		#define MP_MLDSA_ETA  2
		#define MP_MLDSA_TAU  39
		#define MP_MLDSA_BETA 78
		#define MP_MLDSA_GAMMA1  131072        /* 2^17  */
		#define MP_MLDSA_GAMMA2  95232         /* (q-1)/88 */
		#define MP_MLDSA_OMEGA 80
		#define MP_MLDSA_LAMBDA 128
		#define MP_MLDSA_PK_SIZE  1312
		#define MP_MLDSA_SK_SIZE  2560
		#define MP_MLDSA_SIG_SIZE 2420
	#elif MP_MLDSA_LEVEL == 3
		#define MP_MLDSA_K    6
		#define MP_MLDSA_L    5
		#define MP_MLDSA_ETA  4
		#define MP_MLDSA_TAU  49
		#define MP_MLDSA_BETA 196
		#define MP_MLDSA_GAMMA1  524288        /* 2^19  */
		#define MP_MLDSA_GAMMA2  261888        /* (q-1)/32 */
		#define MP_MLDSA_OMEGA 55
		#define MP_MLDSA_LAMBDA 192
		#define MP_MLDSA_PK_SIZE  1952
		#define MP_MLDSA_SK_SIZE  4032
		#define MP_MLDSA_SIG_SIZE 3309
	#elif MP_MLDSA_LEVEL == 5
		#define MP_MLDSA_K    8
		#define MP_MLDSA_L    7
		#define MP_MLDSA_ETA  2
		#define MP_MLDSA_TAU  60
		#define MP_MLDSA_BETA 120
		#define MP_MLDSA_GAMMA1  524288        /* 2^19  */
		#define MP_MLDSA_GAMMA2  261888        /* (q-1)/32 */
		#define MP_MLDSA_OMEGA 75
		#define MP_MLDSA_LAMBDA 256
		#define MP_MLDSA_PK_SIZE  2592
		#define MP_MLDSA_SK_SIZE  4896
		#define MP_MLDSA_SIG_SIZE 4627
	#else
		#error "MP_MLDSA_LEVEL must be 2, 3 or 5"
	#endif

	/* c~ hash length: ceil(lambda/4) bytes. */
	#define MP_MLDSA_CHASH  (MP_MLDSA_LAMBDA / 4)

	/* Build a key pair from 32 bytes of entropy. Writes priv (MP_MLDSA_SK_SIZE)
	 * and pub (MP_MLDSA_PK_SIZE). Returns FALSE on a bad seed. */
	mp_ret_t mp_mldsa_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);

	/* Deterministic signature (rnd = 0). Writes MP_MLDSA_SIG_SIZE bytes and
	 * returns the size, or 0 on failure. */
	unsigned int mp_mldsa_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig);

	/* Returns YES if the signature verifies, NO otherwise. */
	mp_bool_t mp_mldsa_verify(const void *pub, const void *msg, unsigned int len,
		const void *sig, unsigned int siglen);

	/** @}*/
#endif
#endif
