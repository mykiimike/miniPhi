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
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_SLH

#ifndef _HAVE_MP_COMMON_SLH_H
	#define _HAVE_MP_COMMON_SLH_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonSlh
	 * @{
	 */

	/* SLH-DSA-SHA2-128s (FIPS 205, category 1, SHA-2 instantiation, n=16).
	 *   n = 16, h = 63, d = 7, h' = 9, a = 12, k = 14, lg_w = 4 (w = 16), m = 30
	 *   len1 = 32, len2 = 3, wots_len = 35
	 *   pk  = 32 bytes  (PK.seed || PK.root)
	 *   sk  = 64 bytes  (SK.seed || SK.prf || PK.seed || PK.root)
	 *   sig = 7856 bytes (R || FORS || HT)
	 * Constant time: the secret seed only ever enters a SHA-256 input; no
	 * branch, table index or memory address is ever derived from it. */

	#define MP_SLH_N       16
	#define MP_SLH_H       63
	#define MP_SLH_D       7
	#define MP_SLH_HP      9      /* h' = h / d */
	#define MP_SLH_A       12
	#define MP_SLH_K       14
	#define MP_SLH_W       16     /* w = 2^lg_w, lg_w = 4 */
	#define MP_SLH_LG_W    4
	#define MP_SLH_M       30
	#define MP_SLH_LEN1    32
	#define MP_SLH_LEN2    3
	#define MP_SLH_LEN     35     /* wots_len */

	#define MP_SLH_PK_SIZE  (2*MP_SLH_N)            /* 32  */
	#define MP_SLH_SK_SIZE  (4*MP_SLH_N)            /* 64  */
	#define MP_SLH_SIG_SIZE (MP_SLH_N + MP_SLH_K*(MP_SLH_A+1)*MP_SLH_N + MP_SLH_D*(MP_SLH_HP+MP_SLH_LEN)*MP_SLH_N)  /* 7856 */

	/* ADRS type codes (FIPS 205). */
	#define MP_SLH_ADRS_WOTS_HASH   0
	#define MP_SLH_ADRS_WOTS_PK     1
	#define MP_SLH_ADRS_TREE        2
	#define MP_SLH_ADRS_FORS_TREE   3
	#define MP_SLH_ADRS_FORS_ROOTS  4
	#define MP_SLH_ADRS_WOTS_PRF    5
	#define MP_SLH_ADRS_FORS_PRF    6

	/**
	 * Build a key pair from `seedlen` bytes of entropy (needs 48: SK.seed,
	 * SK.prf, PK.seed). Returns FALSE if the seed is too short. Finalises
	 * priv (64 bytes) and pub (32 bytes).
	 */
	mp_ret_t mp_slh_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);

	/** Deterministic signature (OpTRand = 0). Writes MP_SLH_SIG_SIZE bytes; returns the size. */
	unsigned int mp_slh_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig);

	/** Returns YES if the signature verifies against pub, NO otherwise. */
	mp_bool_t mp_slh_verify(const void *pub, const void *msg, unsigned int len,
		const void *sig, unsigned int siglen);

	/** @}*/
#endif
#endif
