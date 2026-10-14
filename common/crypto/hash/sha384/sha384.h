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
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA384

#ifndef _HAVE_MP_COMMON_SHA384_H
	#define _HAVE_MP_COMMON_SHA384_H

	#include <stdint.h>
	#include <crypto/sha512.h>

	/* the arch header defines mp_arch_sha384_t, the state of one hardware digest */
	#ifdef MP_ARCH_SHA384
		#include <arch/sha384.h>
	#endif

	/**
	 * @defgroup mpCommonSha384
	 * @{
	 */

	#define MP_SHA384_SIZE 48

	typedef struct mp_sha384_s mp_sha384_t;

	struct mp_sha384_s {
		/** the SHA-512 compression, with the SHA-384 initial value */
		mp_sha512_t raw;

	#ifdef MP_ARCH_SHA384
		mp_bool_t onHardware;
		mp_arch_sha384_t hw;
	#endif
	};

	/** @} */

	void mp_sha384_init(mp_sha384_t *ctx);
	void mp_sha384_update(mp_sha384_t *ctx, const void *data, unsigned int size);
	void mp_sha384_final(mp_sha384_t *ctx, uint8_t digest[MP_SHA384_SIZE]);
	void mp_sha384(const void *data, unsigned int size, uint8_t digest[MP_SHA384_SIZE]);

	/* an arch with a SHA384 accelerator provides these; init returns NO when the block is busy or off, and the context then runs in software */
	#ifdef MP_ARCH_SHA384
		mp_bool_t mp_arch_sha384_init(mp_arch_sha384_t *hw);
		void mp_arch_sha384_update(mp_arch_sha384_t *hw, const uint8_t *data, unsigned int size);
		void mp_arch_sha384_final(mp_arch_sha384_t *hw, uint8_t digest[MP_SHA384_SIZE]);
	#endif

#endif
#endif
