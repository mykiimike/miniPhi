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
#ifdef SUPPORT_COMMON_CRYPTO_SIGN

#ifndef _HAVE_MP_COMMON_SIGN_H
	#define _HAVE_MP_COMMON_SIGN_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonSign
	 * @{
	 */

	/** the largest key and signature the build can hold; RSA sets these */
	#ifdef SUPPORT_COMMON_CRYPTO_SIGN_RSA
		#define MP_SIGN_PRIV_MAX MP_RSA_PRIV
		#define MP_SIGN_PUB_MAX MP_RSA_PUB
		#define MP_SIGN_SIG_MAX MP_RSA_SIG
	#else
		#define MP_SIGN_PRIV_MAX 64
		#define MP_SIGN_PUB_MAX 64
		#define MP_SIGN_SIG_MAX 64
	#endif

	typedef struct mp_sign_algo_s mp_sign_algo_t;

	/** one algorithm of the build */
	struct mp_sign_algo_s {
		/** module name, "ED25519" */
		const char *name;
		/** display name, "Ed25519" */
		const char *label;
		/** private key bytes */
		unsigned int privSize;
		/** public key bytes */
		unsigned int pubSize;
		/** signature bytes */
		unsigned int sigSize;

		mp_ret_t (*keypair)(const void *seed, unsigned int seedlen, void *priv, void *pub);
		unsigned int (*sign)(const void *priv, const void *msg, unsigned int len, uint8_t *sig);
		mp_bool_t (*verify)(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen);
	};

	/** @} */

	int mp_sign_count(void);
	const mp_sign_algo_t *mp_sign_at(int index);
	/** "ED25519", "ed25519"...; NULL when the build does not carry it */
	const mp_sign_algo_t *mp_sign_find(const char *name);

	/** FALSE when name is unknown or the seed is too short */
	mp_ret_t mp_sign_keypair(const char *name, const void *seed, unsigned int seedlen, void *priv, void *pub);
	/** signature bytes, or 0 */
	unsigned int mp_sign(const char *name, const void *priv, const void *msg, unsigned int len, uint8_t *sig);
	/** NO when name is unknown or the signature does not check */
	mp_bool_t mp_sign_verify(const char *name, const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen);

#endif
#endif
