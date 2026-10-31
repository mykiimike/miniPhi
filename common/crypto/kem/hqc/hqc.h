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
#ifdef SUPPORT_COMMON_CRYPTO_KEM_HQC

#ifndef _HAVE_MP_COMMON_HQC_H
	#define _HAVE_MP_COMMON_HQC_H

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
	 * @defgroup mpCommonHqc HQC-128
	 * @{
	 */

	#define MP_CRYPTO_HQC_N 17669
	#define MP_CRYPTO_HQC_SEED_BYTES 40
	#define MP_CRYPTO_HQC_SALT 16
	#define MP_CRYPTO_HQC_MSG 16
	/** sk_seed || sigma || pk */
	#define MP_CRYPTO_HQC_PRIV 2305
	/** pk_seed || s */
	#define MP_CRYPTO_HQC_PUB 2249
	/** u || v || salt */
	#define MP_CRYPTO_HQC_CT 4433
	#define MP_CRYPTO_HQC_SS 64
	/** sk_seed || sigma || pk_seed */
	#define MP_CRYPTO_HQC_SEED 96
	/** message || salt */
	#define MP_CRYPTO_HQC_COINS 32

	/** @} */

	/**
	 * Key pair from seed: 40 bytes of secret seed, 16 of sigma, 40 of public seed.
	 * FALSE when the seed is shorter.
	 */
	mp_ret_t mp_crypto_hqc_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	/**
	 * Encapsulation. coins is 16 bytes of message then 16 of salt.
	 * Returns the ciphertext length, or 0.
	 */
	unsigned int mp_crypto_hqc_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct);
	/**
	 * YES when ctlen is the ciphertext length. A rejected ciphertext still
	 * writes a shared secret, derived from sigma instead of the message.
	 */
	mp_bool_t mp_crypto_hqc_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss);
	/** Prints once and returns NO when this series has no HQC accelerator. */
	mp_bool_t mp_crypto_hqc_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#ifdef MP_ARCH_HQC
		mp_bool_t mp_arch_hqc_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#endif

#endif
#endif
