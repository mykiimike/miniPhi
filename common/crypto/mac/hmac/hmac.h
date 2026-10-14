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
#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC

#ifndef _HAVE_MP_COMMON_HMAC_H
	#define _HAVE_MP_COMMON_HMAC_H

	#include <stdint.h>

	/* the arch header defines mp_arch_hmac_t, the state of one hardware MAC */
	#ifdef MP_ARCH_HMAC
		#include <arch/hmac.h>
	#endif

	/**
	 * @defgroup mpCommonHmac
	 * @{
	 */

	/** the largest tag: the largest digest */
	#define MP_HMAC_SIZE_MAX MP_HASH_SIZE_MAX

	typedef struct mp_hmac_s mp_hmac_t;

	struct mp_hmac_s {
		/** NULL before init and after final */
		const mp_hash_algo_t *algo;

		/** runs over key ^ ipad, then the message */
		mp_hash_t inner;

		/** key ^ opad, one block of algo */
		uint8_t outer[MP_HASH_BLOCK_MAX];

	#ifdef MP_ARCH_HMAC
		mp_bool_t onHardware;
		mp_arch_hmac_t hw;
	#endif
	};

	/** @} */

	/** hash is a name of HASH, "SHA256"; FALSE when the build does not carry it */
	mp_ret_t mp_hmac_init(mp_hmac_t *ctx, const char *hash, const void *key, unsigned int keylen);
	void mp_hmac_update(mp_hmac_t *ctx, const void *data, unsigned int size);
	/** writes the tag, the digest size of the hash, and returns that size; wipes the context */
	unsigned int mp_hmac_final(mp_hmac_t *ctx, uint8_t *tag);
	/** wipes a context that will not reach mp_hmac_final() */
	void mp_hmac_free(mp_hmac_t *ctx);
	/** one shot; 0 when the build does not carry hash */
	unsigned int mp_hmac(const char *hash, const void *key, unsigned int keylen, const void *data, unsigned int size, uint8_t *tag);

	/* an arch with an HMAC accelerator provides these; init returns NO when it does not take that hash or the block is busy, and the context then runs in software */
	#ifdef MP_ARCH_HMAC
		mp_bool_t mp_arch_hmac_init(mp_arch_hmac_t *hw, const mp_hash_algo_t *algo, const uint8_t *key, unsigned int keylen);
		void mp_arch_hmac_update(mp_arch_hmac_t *hw, const uint8_t *data, unsigned int size);
		void mp_arch_hmac_final(mp_arch_hmac_t *hw, uint8_t *tag);
	#endif

#endif
#endif
