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
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512

#ifndef _HAVE_MP_COMMON_SHA512_H
	#define _HAVE_MP_COMMON_SHA512_H

	#include <stdint.h>

	/* the arch header defines mp_arch_sha512_t, the state of one hardware digest */
	#ifdef MP_ARCH_SHA512
		#include <arch/sha512.h>
	#endif

	/**
	 * @defgroup mpCommonSha512
	 * @{
	 */

	#define MP_SHA512_SIZE 64
	#define MP_SHA512_BLOCK 128

	typedef struct mp_sha512_s mp_sha512_t;

	struct mp_sha512_s {
		uint64_t state[8];

		/** bytes hashed so far */
		uint64_t length;

		uint8_t block[MP_SHA512_BLOCK];

		/** bytes pending in block; a full block is 128, so this is not a uint8_t */
		unsigned int used;

		/** bytes written by final, 64 here and 48 when SHA-384 drives this context */
		unsigned int outlen;

	#ifdef MP_ARCH_SHA512
		mp_bool_t onHardware;
		mp_arch_sha512_t hw;
	#endif
	};

	/** @} */

	void mp_sha512_init(mp_sha512_t *ctx);
	/** software only, no accelerator; SHA-384 starts a digest this way */
	void mp_sha512_init_iv(mp_sha512_t *ctx, const uint64_t iv[8], unsigned int outlen);
	void mp_sha512_update(mp_sha512_t *ctx, const void *data, unsigned int size);
	void mp_sha512_final(mp_sha512_t *ctx, uint8_t *digest);
	void mp_sha512(const void *data, unsigned int size, uint8_t digest[MP_SHA512_SIZE]);
	void mp_sha512_block(uint64_t state[8], const uint8_t block[MP_SHA512_BLOCK]);

	/* an arch with a SHA512 accelerator provides these; init returns NO when the block is busy or off, and the context then runs in software */
	#ifdef MP_ARCH_SHA512
		mp_bool_t mp_arch_sha512_init(mp_arch_sha512_t *hw);
		void mp_arch_sha512_update(mp_arch_sha512_t *hw, const uint8_t *data, unsigned int size);
		void mp_arch_sha512_final(mp_arch_sha512_t *hw, uint8_t digest[MP_SHA512_SIZE]);
	#endif

#endif
#endif
