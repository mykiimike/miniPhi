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
#ifdef SUPPORT_COMMON_CRYPTO_KEM_MLKEM

#ifndef _HAVE_MP_COMMON_MLKEM_H
	#define _HAVE_MP_COMMON_MLKEM_H

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
	 * @defgroup mpCommonMlkem ML-KEM
	 * @{
	 */

	#ifndef MP_CRYPTO_MLKEM_LEVEL
		#error MP_CRYPTO_MLKEM_LEVEL is missing
	#endif
	#if MP_CRYPTO_MLKEM_LEVEL == 512
		#define MP_CRYPTO_MLKEM_K 2
		#define MP_CRYPTO_MLKEM_ETA1 3
		#define MP_CRYPTO_MLKEM_ETA2 2
		#define MP_CRYPTO_MLKEM_DU 10
		#define MP_CRYPTO_MLKEM_DV 4
	#elif MP_CRYPTO_MLKEM_LEVEL == 768
		#define MP_CRYPTO_MLKEM_K 3
		#define MP_CRYPTO_MLKEM_ETA1 2
		#define MP_CRYPTO_MLKEM_ETA2 2
		#define MP_CRYPTO_MLKEM_DU 10
		#define MP_CRYPTO_MLKEM_DV 4
	#elif MP_CRYPTO_MLKEM_LEVEL == 1024
		#define MP_CRYPTO_MLKEM_K 4
		#define MP_CRYPTO_MLKEM_ETA1 2
		#define MP_CRYPTO_MLKEM_ETA2 2
		#define MP_CRYPTO_MLKEM_DU 11
		#define MP_CRYPTO_MLKEM_DV 5
	#else
		#error MP_CRYPTO_MLKEM_LEVEL must be 512, 768 or 1024
	#endif

	#define MP_CRYPTO_MLKEM_N 256
	#define MP_CRYPTO_MLKEM_SS 32
	#define MP_CRYPTO_MLKEM_SEED 64
	#define MP_CRYPTO_MLKEM_COINS 32
	/** 12-bit encoding of one polynomial */
	#define MP_CRYPTO_MLKEM_POLY12 384
	/** encapsulation key: t_hat || rho */
	#define MP_CRYPTO_MLKEM_PUB (MP_CRYPTO_MLKEM_K * MP_CRYPTO_MLKEM_POLY12 + 32)
	/** decapsulation key: s_hat || ek || H(ek) || z */
	#define MP_CRYPTO_MLKEM_PRIV (MP_CRYPTO_MLKEM_K * MP_CRYPTO_MLKEM_POLY12 + MP_CRYPTO_MLKEM_PUB + 64)
	#define MP_CRYPTO_MLKEM_CT ((MP_CRYPTO_MLKEM_K * MP_CRYPTO_MLKEM_DU * MP_CRYPTO_MLKEM_N / 8) + (MP_CRYPTO_MLKEM_DV * MP_CRYPTO_MLKEM_N / 8))

	/** @} */

	/**
	 * FIPS 203 key pair from seed, which is 64 bytes: d then z.
	 * FALSE when the seed is shorter.
	 */
	mp_ret_t mp_crypto_mlkem_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	/**
	 * Encapsulation. coins is the 32-byte message m.
	 * Writes the shared secret and the ciphertext. Returns the ciphertext length, or 0.
	 */
	unsigned int mp_crypto_mlkem_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct);
	/**
	 * Decapsulation. YES when ctlen is the ciphertext length, and ss is always
	 * written then. A rejected ciphertext yields the implicit-rejection secret.
	 */
	mp_bool_t mp_crypto_mlkem_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss);
	/**
	 * Fuse path. A series accelerator provides mp_arch_mlkem_decaps_fuse().
	 * Prints once and returns NO when this series has none.
	 */
	mp_bool_t mp_crypto_mlkem_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#ifdef MP_ARCH_MLKEM
		mp_bool_t mp_arch_mlkem_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#endif

#endif
#endif
