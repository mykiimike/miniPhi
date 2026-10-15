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
#ifdef SUPPORT_COMMON_CRYPTO_MAC

#ifndef _HAVE_MP_COMMON_MAC_H
	#define _HAVE_MP_COMMON_MAC_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonMac
	 * @{
	 */

	/** the largest tag any MAC writes (HMAC-SHA-512) */
	#define MP_MAC_SIZE_MAX 64

	typedef struct mp_mac_s mp_mac_t;

	struct mp_mac_s {
		/** 0 before init, after final and after free */
		int kind;
		/** tag bytes */
		unsigned int size;

		/** as large as the largest MAC in the build */
		union {
		#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC
			mp_hmac_t hmac;
		#endif
		#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
			mp_blake2s_t blake2s;
		#endif
		#ifdef SUPPORT_COMMON_CRYPTO_MAC_POLY1305
			mp_poly1305_t poly1305;
		#endif
			uint8_t none;
		} ctx;
	};

	/** @} */

	/**
	 * "HMAC-SHA256", "hmac-sha-256", "BLAKE2S"...
	 * FALSE when the build does not carry name or the key does not fit; mac is then cleared.
	 */
	mp_ret_t mp_mac_init(mp_mac_t *mac, const char *name, const void *key, unsigned int keylen);
	void mp_mac_update(mp_mac_t *mac, const void *data, unsigned int size);
	/** writes the tag, at most MP_MAC_SIZE_MAX bytes, returns its size, 0 when not started; wipes the context */
	unsigned int mp_mac_final(mp_mac_t *mac, uint8_t *tag);
	/** wipes a context that will not reach mp_mac_final() */
	void mp_mac_free(mp_mac_t *mac);
	/** tag bytes of a started MAC, 0 otherwise */
	unsigned int mp_mac_size(const mp_mac_t *mac);
	/** YES when the size bytes match; the time does not depend on where they differ */
	mp_bool_t mp_mac_equal(const void *a, const void *b, unsigned int size);

#endif
#endif
