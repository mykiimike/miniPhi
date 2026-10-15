/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_MAC_POLY1305

#ifndef _HAVE_MP_COMMON_POLY1305_H
	#define _HAVE_MP_COMMON_POLY1305_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonPoly1305 Poly1305
	 * @ingroup mpCommon
	 * @brief Poly1305 one-time MAC (RFC 7539)
	 * @{
	 */

	/** Poly1305 key (r || s), one-time, 32 bytes */
	#define MP_POLY1305_KEY 32

	/** Poly1305 tag, 16 bytes */
	#define MP_POLY1305_SIZE 16

	/** Poly1305 block, 16 bytes */
	#define MP_POLY1305_BLOCK 16

	typedef struct mp_poly1305_s mp_poly1305_t;

	/** Poly1305 context. The key is one-time: a fresh key per message,
	 *  never reused. Owned by the caller. */
	struct mp_poly1305_s {
		/** r, clamped, five 26-bit limbs */
		uint32_t r[5];
		/** s, four 32-bit words */
		uint32_t s[4];
		/** accumulator, five 26-bit limbs */
		uint32_t h[5];
		/** buffered bytes < 16 */
		uint8_t buf[MP_POLY1305_BLOCK];
		/** bytes in buf */
		unsigned int bufLeft;
	};

	/** FALSE when the key length is not 32 */
	mp_ret_t mp_poly1305_init(mp_poly1305_t *ctx, const void *key, unsigned int keylen);
	/** absorb any length; the time does not depend on the key */
	void mp_poly1305_update(mp_poly1305_t *ctx, const void *in, unsigned int inlen);
	/** writes 16 bytes, returns 16, 0 when not started; wipes the context */
	unsigned int mp_poly1305_final(mp_poly1305_t *ctx, uint8_t *tag);
	/** wipes the context; safe on a MAC already ended */
	void mp_poly1305_free(mp_poly1305_t *ctx);

	/** @} */

#endif
#endif
