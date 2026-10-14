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
#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S

#ifndef _HAVE_MP_COMMON_BLAKE2S_H
	#define _HAVE_MP_COMMON_BLAKE2S_H

	#include <stdint.h>

	/* the arch header defines mp_arch_blake2s_t, the state of one hardware digest */
	#ifdef MP_ARCH_BLAKE2S
		#include <arch/blake2s.h>
	#endif

	/**
	 * @defgroup mpCommonBlake2s
	 * @{
	 */

	#define MP_BLAKE2S_SIZE 32
	#define MP_BLAKE2S_BLOCK 64
	#define MP_BLAKE2S_KEY 32

	typedef struct mp_blake2s_s mp_blake2s_t;

	struct mp_blake2s_s {
		uint32_t state[8];

		/** bytes compressed so far, not counting what is still in block */
		uint64_t count;

		uint8_t block[MP_BLAKE2S_BLOCK];
		uint8_t used;

	#ifdef MP_ARCH_BLAKE2S
		mp_bool_t onHardware;
		mp_arch_blake2s_t hw;
	#endif
	};

	/** @} */

	void mp_blake2s_init(mp_blake2s_t *ctx);
	/** key NULL or keylen 0 is the unkeyed digest; a longer key is cut at MP_BLAKE2S_KEY */
	void mp_blake2s_init_key(mp_blake2s_t *ctx, const void *key, unsigned int keylen);
	void mp_blake2s_update(mp_blake2s_t *ctx, const void *data, unsigned int size);
	void mp_blake2s_final(mp_blake2s_t *ctx, uint8_t digest[MP_BLAKE2S_SIZE]);
	void mp_blake2s(const void *data, unsigned int size, uint8_t digest[MP_BLAKE2S_SIZE]);

	/* an arch with a BLAKE2S accelerator provides these; init returns NO when the block is busy or off, and the context then runs in software. Keyed digests do not call them */
	#ifdef MP_ARCH_BLAKE2S
		mp_bool_t mp_arch_blake2s_init(mp_arch_blake2s_t *hw);
		void mp_arch_blake2s_update(mp_arch_blake2s_t *hw, const uint8_t *data, unsigned int size);
		void mp_arch_blake2s_final(mp_arch_blake2s_t *hw, uint8_t digest[MP_BLAKE2S_SIZE]);
	#endif

#endif
#endif
