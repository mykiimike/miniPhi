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
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA1

#ifndef _HAVE_MP_COMMON_SHA1_H
	#define _HAVE_MP_COMMON_SHA1_H

	#include <stdint.h>

	/* the arch header defines mp_arch_sha1_t, the state of one hardware digest */
	#ifdef MP_ARCH_SHA1
		#include <arch/sha1.h>
	#endif

	/**
	 * @defgroup mpCommonSha1
	 * @{
	 */

	#define MP_SHA1_SIZE 20
	#define MP_SHA1_BLOCK 64

	typedef struct mp_sha1_s mp_sha1_t;

	struct mp_sha1_s {
		uint32_t state[5];

		/** bytes hashed so far */
		uint64_t length;

		uint8_t block[MP_SHA1_BLOCK];
		uint8_t used;

	#ifdef MP_ARCH_SHA1
		mp_bool_t onHardware;
		mp_arch_sha1_t hw;
	#endif
	};

	/** @} */

	void mp_sha1_init(mp_sha1_t *ctx);
	void mp_sha1_update(mp_sha1_t *ctx, const void *data, unsigned int size);
	void mp_sha1_final(mp_sha1_t *ctx, uint8_t digest[MP_SHA1_SIZE]);
	void mp_sha1(const void *data, unsigned int size, uint8_t digest[MP_SHA1_SIZE]);

	/* an arch with a SHA1 accelerator provides these; init returns NO when the block is busy or off, and the context then runs in software */
	#ifdef MP_ARCH_SHA1
		mp_bool_t mp_arch_sha1_init(mp_arch_sha1_t *hw);
		void mp_arch_sha1_update(mp_arch_sha1_t *hw, const uint8_t *data, unsigned int size);
		void mp_arch_sha1_final(mp_arch_sha1_t *hw, uint8_t digest[MP_SHA1_SIZE]);
	#endif

#endif
#endif

