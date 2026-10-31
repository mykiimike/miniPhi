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
#ifdef SUPPORT_COMMON_CRYPTO_KEM_X448

#ifndef _HAVE_MP_COMMON_X448_H
	#define _HAVE_MP_COMMON_X448_H

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
	 * @defgroup mpCommonX448 X448 KEM
	 * @{
	 */

	#define MP_CRYPTO_X448_PRIV 56
	#define MP_CRYPTO_X448_PUB 56
	#define MP_CRYPTO_X448_CT 56
	#define MP_CRYPTO_X448_SS 64
	#define MP_CRYPTO_X448_SEED 56
	#define MP_CRYPTO_X448_COINS 56

	/** @} */

	/**
	 * RFC 7748. scalar is clamped inside. point is a u-coordinate.
	 * out and the inputs are 56 bytes, little-endian.
	 */
	void mp_crypto_x448(uint8_t out[56], const uint8_t scalar[56], const uint8_t point[56]);
	/** FALSE when the seed is shorter than 56 bytes. The seed is the private key. */
	mp_ret_t mp_crypto_x448_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	/**
	 * DHKEM(X448, HKDF-SHA512). coins is the 56-byte ephemeral secret.
	 * Returns 56, or 0. 0 includes a Diffie-Hellman output of 56 zero bytes.
	 */
	unsigned int mp_crypto_x448_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct);
	/** YES when ctlen is 56 and the Diffie-Hellman output is not all zeros. ss is 64 bytes. */
	mp_bool_t mp_crypto_x448_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss);
	/** Prints once and returns NO when this series has no X448 accelerator. */
	mp_bool_t mp_crypto_x448_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#ifdef MP_ARCH_X448
		mp_bool_t mp_arch_x448_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#endif

#endif
#endif
