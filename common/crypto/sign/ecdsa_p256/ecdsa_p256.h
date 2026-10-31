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
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ECDSA_P256

#ifndef _HAVE_MP_COMMON_ECDSA_P256_H
	#define _HAVE_MP_COMMON_ECDSA_P256_H

	#include <stdint.h>
	#ifdef SUPPORT_COMMON_CRYPTO_FUSE
		#include <crypto/fuse.h>
	#else
		#ifndef _HAVE_MP_FUSE_T
			#define _HAVE_MP_FUSE_T
			typedef struct mp_crypto_fuse_s mp_crypto_fuse_t;
		#endif
	#endif

	/**
	 * @defgroup mpCommonEcdsaP256
	 * @{
	 */

	#define MP_CRYPTO_ECDSA_P256_PRIV 32
	#define MP_CRYPTO_ECDSA_P256_PUB 64
	#define MP_CRYPTO_ECDSA_P256_SIG 64

	/** @} */

	/** seed is 32 bytes, reduced mod n; FALSE when it is shorter or reduces to 0 */
	mp_ret_t mp_crypto_ecdsa_p256_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	void mp_crypto_ecdsa_p256_public(const void *priv, void *pub);
	/** writes 64 bytes and returns 64, or 0 when the key is not usable */
	unsigned int mp_crypto_ecdsa_p256_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig);
	/**
	 * Role MP_CRYPTO_FUSE_ECDSA_P256. wrapped is the blob the series arch defines.
	 * Returns 64, or 0 when this series has no ECDSA P-256 accelerator.
	 */
	unsigned int mp_crypto_ecdsa_p256_sign_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *msg, unsigned int len, uint8_t *sig);
	#ifdef MP_ARCH_ECDSA_P256
		unsigned int mp_arch_ecdsa_p256_sign_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *msg, unsigned int len, uint8_t *sig);
	#endif
	mp_bool_t mp_crypto_ecdsa_p256_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen);

#endif
#endif
