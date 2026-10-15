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

#ifdef SUPPORT_COMMON_CRYPTO_MODE_CHACHA20_POLY1305

#ifndef _HAVE_MP_COMMON_CHACHA20_POLY1305_H
	#define _HAVE_MP_COMMON_CHACHA20_POLY1305_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonChacha20Poly1305 ChaCha20-Poly1305
	 * @ingroup mpCommon
	 * @brief ChaCha20-Poly1305 AEAD (RFC 8439)
	 * @{
	 */

	/** key, 32 bytes */
	#define MP_CHACHA20_POLY1305_KEY 32
	/** nonce, 12 bytes */
	#define MP_CHACHA20_POLY1305_NONCE 12
	/** tag, 16 bytes */
	#define MP_CHACHA20_POLY1305_TAG 16

	/**
	 * @brief Encrypt and authenticate
	 *
	 * @param[in] key 32 bytes
	 * @param[in] nonce 12 bytes
	 * @param[in] aad Additional data, or NULL
	 * @param[in] aadlen AAD bytes
	 * @param[in] pt Plaintext, or NULL
	 * @param[in] ptlen Plaintext bytes
	 * @param[out] ct At least ptlen bytes
	 * @param[out] tag 16 bytes
	 * @return TRUE on success
	 */
	mp_ret_t mp_chacha20_poly1305_encrypt(const void *key, unsigned int keylen,
		const void *nonce, unsigned int noncelen,
		const void *aad, unsigned int aadlen,
		const void *pt, unsigned int ptlen,
		uint8_t *ct, uint8_t *tag);

	/**
	 * @brief Verify and decrypt
	 *
	 * @param[in] key 32 bytes
	 * @param[in] nonce 12 bytes
	 * @param[in] aad Additional data, or NULL
	 * @param[in] aadlen AAD bytes
	 * @param[in] ct Ciphertext, or NULL
	 * @param[in] ctlen Ciphertext bytes
	 * @param[in] tag 16 bytes
	 * @param[out] pt At least ctlen bytes (wiped on a tag mismatch)
	 * @return TRUE when the tag matches, FALSE otherwise
	 */
	mp_ret_t mp_chacha20_poly1305_decrypt(const void *key, unsigned int keylen,
		const void *nonce, unsigned int noncelen,
		const void *aad, unsigned int aadlen,
		const void *ct, unsigned int ctlen,
		const void *tag,
		uint8_t *pt);

	/** @} */

#endif
#endif
