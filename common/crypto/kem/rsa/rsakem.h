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
#ifdef SUPPORT_COMMON_CRYPTO_KEM_RSAKEM

#ifndef _HAVE_MP_COMMON_RSAKEM_H
	#define _HAVE_MP_COMMON_RSAKEM_H

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
	 * @defgroup mpCommonRsakem RSA-KEM
	 * @{
	 */

	#define MP_CRYPTO_RSAKEM_PUB MP_CRYPTO_RSA_PUB
	#define MP_CRYPTO_RSAKEM_PRIV MP_CRYPTO_RSA_PRIV
	#define MP_CRYPTO_RSAKEM_CT MP_CRYPTO_RSA_MODULUS
	#define MP_CRYPTO_RSAKEM_SS 32
	#define MP_CRYPTO_RSAKEM_SEED 32
	/** one modulus, the integer z */
	#define MP_CRYPTO_RSAKEM_COINS MP_CRYPTO_RSA_MODULUS

	/** @} */

	/**
	 * The RSA key pair. seed is at least 32 bytes. Far too slow for the screen.
	 */
	mp_ret_t mp_crypto_rsakem_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	/**
	 * coins is z, one modulus of big-endian bytes, in the range [1, n).
	 * The shared secret is SHAKE-256 of those bytes, 32 long.
	 * Returns the ciphertext length, or 0 when z is out of range.
	 */
	unsigned int mp_crypto_rsakem_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct);
	/**
	 * YES when ctlen is one modulus. A ciphertext outside [1, n) still writes
	 * a rejection secret of 32 bytes.
	 */
	mp_bool_t mp_crypto_rsakem_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss);
	/** Prints once and returns NO when this series has no RSA-KEM accelerator. */
	mp_bool_t mp_crypto_rsakem_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#ifdef MP_ARCH_RSAKEM
		mp_bool_t mp_arch_rsakem_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	#endif

#endif
#endif
