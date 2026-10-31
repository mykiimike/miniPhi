/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
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
#ifdef SUPPORT_COMMON_CRYPTO_FUSE

#ifndef _HAVE_MP_COMMON_FUSE_H
	#define _HAVE_MP_COMMON_FUSE_H

	/**
	 * @defgroup mpCommonFuse
	 * @{
	 */

	/** HMAC upstream. The peripheral returns the tag. */
	#define MP_CRYPTO_FUSE_HMAC 1u
	/** RSA digital signature. The private key stays wrapped. */
	#define MP_CRYPTO_FUSE_DS 2u
	/** XTS-AES of one external-flash line. */
	#define MP_CRYPTO_FUSE_XTS 3u
	/** Ed25519. The series arch maps this role onto its slot. */
	#define MP_CRYPTO_FUSE_ED25519 4u
	/** ECDSA P-256. */
	#define MP_CRYPTO_FUSE_ECDSA_P256 5u
	/** LMS. */
	#define MP_CRYPTO_FUSE_LMS 6u
	/** SLH-DSA. */
	#define MP_CRYPTO_FUSE_SLH 7u
	/** ML-DSA. */
	#define MP_CRYPTO_FUSE_MLDSA 8u
	/** ML-KEM. */
	#define MP_CRYPTO_FUSE_MLKEM 9u
	/** HQC-128. */
	#define MP_CRYPTO_FUSE_HQC 10u
	/** X25519. */
	#define MP_CRYPTO_FUSE_X25519 11u
	/** RSA-KEM. */
	#define MP_CRYPTO_FUSE_RSAKEM 12u

	typedef struct mp_crypto_fuse_s mp_crypto_fuse_t;

	struct mp_crypto_fuse_s {
		/** KEY0 is 0. A C6 has slots 0 to 5. */
		unsigned int slot;

		/** one MP_CRYPTO_FUSE_* role */
		unsigned int purpose;
	};

	/** @} */

	/**
	 * YES when fuse names purpose and the burned key has that role.
	 * A mismatch prints once.
	 */
	mp_bool_t mp_crypto_fuse_check(const mp_crypto_fuse_t *fuse, unsigned int purpose);

	/** the series reads the burned purpose; NO when the slot has another role */
	mp_bool_t mp_arch_fuse_check(unsigned int slot, unsigned int purpose);
	/** the burned purpose value, 0 when slot is out of range */
	unsigned int mp_arch_fuse_purpose(unsigned int slot);

#endif
#endif
