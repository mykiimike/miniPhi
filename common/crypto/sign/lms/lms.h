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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_LMS

#ifndef _HAVE_MP_COMMON_LMS_H
	#define _HAVE_MP_COMMON_LMS_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonLms
	 * @{
	 */

	/* LMS_SHA256_M32_H5 with LMOTS_SHA256_N32_W8 (RFC 8554) */
	#define MP_LMS_N 32
	#define MP_LMS_W 8
	#define MP_LMS_P 34
	#define MP_LMS_LS 0
	#define MP_LMS_H 5
	#define MP_LMS_M 32
	#define MP_LMS_LEAVES 32

	/* u32str(type=5) || u32str(otstype=4) || I(16) || T[1](32) */
	#define MP_LMS_PUB 56
	/* I(16) || seed(32) || q(4) */
	#define MP_LMS_PRIV 52
	/* q(4) || lmots_sig(1124) || type(4) || path(5*32) */
	#define MP_LMS_SIG 1292

	#define MP_LMS_TYPE 5
	#define MP_LMS_OTS_TYPE 4

	/** the leaf counter lives in the private key; the caller persists it */
	typedef struct mp_lms_priv_s mp_lms_priv_t;
	struct mp_lms_priv_s {
		uint8_t I[16];
		uint8_t seed[32];
		/** next leaf to sign with; the caller MUST advance it after a signature */
		uint32_t q;
	};

	/** FALSE when the seed is too short */
	mp_ret_t mp_lms_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	/** signature bytes (MP_LMS_SIG), or 0 when q is exhausted or the key is bad */
	unsigned int mp_lms_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig);
	/** YES when the signature checks */
	mp_bool_t mp_lms_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen);

	/** @} */

#endif
#endif
