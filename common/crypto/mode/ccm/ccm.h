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

#ifdef SUPPORT_COMMON_CRYPTO_MODE_CCM

#ifndef _HAVE_MP_COMMON_CCM_H
	#define _HAVE_MP_COMMON_CCM_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonCcm CCM
	 * @ingroup mpCommon
	 * @brief AES-CCM AEAD (RFC 3610 / NIST SP 800-38C)
	 * @{
	 */

	/** largest tag CCM writes, 16 bytes */
	#define MP_CCM_TAG_MAX 16

	/**
	 * @brief Encrypt and authenticate
	 *
	 * @param[in] aes AES cipher started with the key (mp_cipher_init)
	 * @param[in] nonce 7..13 bytes
	 * @param[in] nlen Nonce bytes
	 * @param[in] aad Additional data, or NULL
	 * @param[in] aadlen AAD bytes
	 * @param[in] pt Plaintext, or NULL
	 * @param[in] ptlen Plaintext bytes
	 * @param[out] ct At least ptlen bytes
	 * @param[out] tag T bytes
	 * @param[in] T Tag size, one of 4,6,8,10,12,14,16
	 * @return TRUE on success
	 */
	mp_ret_t mp_ccm_seal(mp_cipher_t *aes,
		const void *nonce, unsigned int nlen,
		const void *aad, unsigned int aadlen,
		const void *pt, unsigned int ptlen,
		uint8_t *ct, uint8_t *tag, unsigned int T);

	/**
	 * @brief Verify and decrypt
	 *
	 * @param[in] aes AES cipher started with the key
	 * @param[in] nonce 7..13 bytes
	 * @param[in] nlen Nonce bytes
	 * @param[in] aad Additional data, or NULL
	 * @param[in] aadlen AAD bytes
	 * @param[in] ct Ciphertext, or NULL
	 * @param[in] ctlen Ciphertext bytes
	 * @param[in] tag T bytes
	 * @param[in] T Tag size
	 * @param[out] pt At least ctlen bytes (wiped on a tag mismatch)
	 * @return TRUE when the tag matches, FALSE otherwise
	 */
	mp_ret_t mp_ccm_open(mp_cipher_t *aes,
		const void *nonce, unsigned int nlen,
		const void *aad, unsigned int aadlen,
		const void *ct, unsigned int ctlen,
		const void *tag, unsigned int T,
		uint8_t *pt);

	/** @} */

#endif
#endif
