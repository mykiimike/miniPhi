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
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ED25519

#ifndef _HAVE_MP_COMMON_ED25519_H
	#define _HAVE_MP_COMMON_ED25519_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonEd25519
	 * @{
	 */

	#define MP_ED25519_PRIV 32
	#define MP_ED25519_PUB 32
	#define MP_ED25519_SIG 64

	/** @} */

	/** seed is 32 bytes and becomes the private key; FALSE when it is shorter */
	mp_ret_t mp_ed25519_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub);
	/** writes the 32-byte public key for a 32-byte private key */
	void mp_ed25519_public(const void *priv, void *pub);
	/** writes 64 bytes and returns 64, or 0 when priv is not a private key */
	unsigned int mp_ed25519_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig);
	/** YES when sig is a signature by pub on msg */
	mp_bool_t mp_ed25519_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen);

#endif
#endif
