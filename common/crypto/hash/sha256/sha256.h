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
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA256

#ifndef _HAVE_MP_COMMON_SHA256_H
	#define _HAVE_MP_COMMON_SHA256_H

	#include <stdint.h>

	/* the arch header defines mp_arch_sha256_t, the state of one hardware digest */
	#ifdef MP_ARCH_SHA256
		#include <arch/sha256.h>
	#endif

	/**
	 * @defgroup mpCommonSha256
	 * @{
	 */

	#define MP_SHA256_SIZE 32
	#define MP_SHA256_BLOCK 64

	typedef struct mp_sha256_s mp_sha256_t;

	struct mp_sha256_s {
		uint32_t state[8];

		/** bytes hashed so far */
		uint64_t length;

		uint8_t block[MP_SHA256_BLOCK];
		uint8_t used;

	#ifdef MP_ARCH_SHA256
		mp_bool_t onHardware;
		mp_arch_sha256_t hw;
	#endif
	};

	/** @} */

	void mp_sha256_init(mp_sha256_t *ctx);
	void mp_sha256_update(mp_sha256_t *ctx, const void *data, unsigned int size);
	void mp_sha256_final(mp_sha256_t *ctx, uint8_t digest[MP_SHA256_SIZE]);
	void mp_sha256(const void *data, unsigned int size, uint8_t digest[MP_SHA256_SIZE]);

	/* an arch with a SHA256 accelerator provides these; init returns NO when the block is busy or off, and the context then runs in software */
	#ifdef MP_ARCH_SHA256
		mp_bool_t mp_arch_sha256_init(mp_arch_sha256_t *hw);
		void mp_arch_sha256_update(mp_arch_sha256_t *hw, const uint8_t *data, unsigned int size);
		void mp_arch_sha256_final(mp_arch_sha256_t *hw, uint8_t digest[MP_SHA256_SIZE]);
	#endif

#endif
#endif

