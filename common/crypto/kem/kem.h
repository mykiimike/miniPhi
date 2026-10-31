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
#ifdef SUPPORT_COMMON_CRYPTO_KEM

#ifndef _HAVE_MP_COMMON_KEM_H
	#define _HAVE_MP_COMMON_KEM_H

	#include <stdint.h>
	#ifdef SUPPORT_COMMON_CRYPTO_FUSE
		#include <crypto/fuse.h>
	#else
		#ifndef _HAVE_MP_FUSE_T
			#define _HAVE_MP_FUSE_T
			typedef struct mp_crypto_fuse_s mp_crypto_fuse_t;
		#endif
	#endif

	/**
	 * @defgroup mpCommonKem KEM API
	 * @{
	 */

	/** the largest key, ciphertext and secret this build can hold */
	#define MP_CRYPTO_KEM_PRIV_MAX 32
	#define MP_CRYPTO_KEM_PUB_MAX 32
	#define MP_CRYPTO_KEM_CT_MAX 32
	#define MP_CRYPTO_KEM_SS_MAX 32
	#define MP_CRYPTO_KEM_SEED_MAX 32
	#define MP_CRYPTO_KEM_COINS_MAX 32

	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X25519) && (MP_CRYPTO_X25519_PRIV > MP_CRYPTO_KEM_PRIV_MAX)
		#undef MP_CRYPTO_KEM_PRIV_MAX
		#define MP_CRYPTO_KEM_PRIV_MAX MP_CRYPTO_X25519_PRIV
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X25519) && (MP_CRYPTO_X25519_PUB > MP_CRYPTO_KEM_PUB_MAX)
		#undef MP_CRYPTO_KEM_PUB_MAX
		#define MP_CRYPTO_KEM_PUB_MAX MP_CRYPTO_X25519_PUB
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X25519) && (MP_CRYPTO_X25519_CT > MP_CRYPTO_KEM_CT_MAX)
		#undef MP_CRYPTO_KEM_CT_MAX
		#define MP_CRYPTO_KEM_CT_MAX MP_CRYPTO_X25519_CT
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X25519) && (MP_CRYPTO_X25519_SS > MP_CRYPTO_KEM_SS_MAX)
		#undef MP_CRYPTO_KEM_SS_MAX
		#define MP_CRYPTO_KEM_SS_MAX MP_CRYPTO_X25519_SS
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X25519) && (MP_CRYPTO_X25519_SEED > MP_CRYPTO_KEM_SEED_MAX)
		#undef MP_CRYPTO_KEM_SEED_MAX
		#define MP_CRYPTO_KEM_SEED_MAX MP_CRYPTO_X25519_SEED
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X25519) && (MP_CRYPTO_X25519_COINS > MP_CRYPTO_KEM_COINS_MAX)
		#undef MP_CRYPTO_KEM_COINS_MAX
		#define MP_CRYPTO_KEM_COINS_MAX MP_CRYPTO_X25519_COINS
	#endif

	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X448) && (MP_CRYPTO_X448_PRIV > MP_CRYPTO_KEM_PRIV_MAX)
		#undef MP_CRYPTO_KEM_PRIV_MAX
		#define MP_CRYPTO_KEM_PRIV_MAX MP_CRYPTO_X448_PRIV
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X448) && (MP_CRYPTO_X448_PUB > MP_CRYPTO_KEM_PUB_MAX)
		#undef MP_CRYPTO_KEM_PUB_MAX
		#define MP_CRYPTO_KEM_PUB_MAX MP_CRYPTO_X448_PUB
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X448) && (MP_CRYPTO_X448_CT > MP_CRYPTO_KEM_CT_MAX)
		#undef MP_CRYPTO_KEM_CT_MAX
		#define MP_CRYPTO_KEM_CT_MAX MP_CRYPTO_X448_CT
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X448) && (MP_CRYPTO_X448_SS > MP_CRYPTO_KEM_SS_MAX)
		#undef MP_CRYPTO_KEM_SS_MAX
		#define MP_CRYPTO_KEM_SS_MAX MP_CRYPTO_X448_SS
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X448) && (MP_CRYPTO_X448_SEED > MP_CRYPTO_KEM_SEED_MAX)
		#undef MP_CRYPTO_KEM_SEED_MAX
		#define MP_CRYPTO_KEM_SEED_MAX MP_CRYPTO_X448_SEED
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_X448) && (MP_CRYPTO_X448_COINS > MP_CRYPTO_KEM_COINS_MAX)
		#undef MP_CRYPTO_KEM_COINS_MAX
		#define MP_CRYPTO_KEM_COINS_MAX MP_CRYPTO_X448_COINS
	#endif

	#if defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM) && (MP_CRYPTO_MLKEM_PRIV > MP_CRYPTO_KEM_PRIV_MAX)
		#undef MP_CRYPTO_KEM_PRIV_MAX
		#define MP_CRYPTO_KEM_PRIV_MAX MP_CRYPTO_MLKEM_PRIV
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM) && (MP_CRYPTO_MLKEM_PUB > MP_CRYPTO_KEM_PUB_MAX)
		#undef MP_CRYPTO_KEM_PUB_MAX
		#define MP_CRYPTO_KEM_PUB_MAX MP_CRYPTO_MLKEM_PUB
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM) && (MP_CRYPTO_MLKEM_CT > MP_CRYPTO_KEM_CT_MAX)
		#undef MP_CRYPTO_KEM_CT_MAX
		#define MP_CRYPTO_KEM_CT_MAX MP_CRYPTO_MLKEM_CT
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM) && (MP_CRYPTO_MLKEM_SS > MP_CRYPTO_KEM_SS_MAX)
		#undef MP_CRYPTO_KEM_SS_MAX
		#define MP_CRYPTO_KEM_SS_MAX MP_CRYPTO_MLKEM_SS
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM) && (MP_CRYPTO_MLKEM_SEED > MP_CRYPTO_KEM_SEED_MAX)
		#undef MP_CRYPTO_KEM_SEED_MAX
		#define MP_CRYPTO_KEM_SEED_MAX MP_CRYPTO_MLKEM_SEED
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM) && (MP_CRYPTO_MLKEM_COINS > MP_CRYPTO_KEM_COINS_MAX)
		#undef MP_CRYPTO_KEM_COINS_MAX
		#define MP_CRYPTO_KEM_COINS_MAX MP_CRYPTO_MLKEM_COINS
	#endif

	#if defined(SUPPORT_COMMON_CRYPTO_KEM_HQC) && (MP_CRYPTO_HQC_PRIV > MP_CRYPTO_KEM_PRIV_MAX)
		#undef MP_CRYPTO_KEM_PRIV_MAX
		#define MP_CRYPTO_KEM_PRIV_MAX MP_CRYPTO_HQC_PRIV
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_HQC) && (MP_CRYPTO_HQC_PUB > MP_CRYPTO_KEM_PUB_MAX)
		#undef MP_CRYPTO_KEM_PUB_MAX
		#define MP_CRYPTO_KEM_PUB_MAX MP_CRYPTO_HQC_PUB
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_HQC) && (MP_CRYPTO_HQC_CT > MP_CRYPTO_KEM_CT_MAX)
		#undef MP_CRYPTO_KEM_CT_MAX
		#define MP_CRYPTO_KEM_CT_MAX MP_CRYPTO_HQC_CT
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_HQC) && (MP_CRYPTO_HQC_SS > MP_CRYPTO_KEM_SS_MAX)
		#undef MP_CRYPTO_KEM_SS_MAX
		#define MP_CRYPTO_KEM_SS_MAX MP_CRYPTO_HQC_SS
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_HQC) && (MP_CRYPTO_HQC_SEED > MP_CRYPTO_KEM_SEED_MAX)
		#undef MP_CRYPTO_KEM_SEED_MAX
		#define MP_CRYPTO_KEM_SEED_MAX MP_CRYPTO_HQC_SEED
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_HQC) && (MP_CRYPTO_HQC_COINS > MP_CRYPTO_KEM_COINS_MAX)
		#undef MP_CRYPTO_KEM_COINS_MAX
		#define MP_CRYPTO_KEM_COINS_MAX MP_CRYPTO_HQC_COINS
	#endif

	#if defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && (MP_CRYPTO_RSAKEM_PRIV > MP_CRYPTO_KEM_PRIV_MAX)
		#undef MP_CRYPTO_KEM_PRIV_MAX
		#define MP_CRYPTO_KEM_PRIV_MAX MP_CRYPTO_RSAKEM_PRIV
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && (MP_CRYPTO_RSAKEM_PUB > MP_CRYPTO_KEM_PUB_MAX)
		#undef MP_CRYPTO_KEM_PUB_MAX
		#define MP_CRYPTO_KEM_PUB_MAX MP_CRYPTO_RSAKEM_PUB
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && (MP_CRYPTO_RSAKEM_CT > MP_CRYPTO_KEM_CT_MAX)
		#undef MP_CRYPTO_KEM_CT_MAX
		#define MP_CRYPTO_KEM_CT_MAX MP_CRYPTO_RSAKEM_CT
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && (MP_CRYPTO_RSAKEM_SS > MP_CRYPTO_KEM_SS_MAX)
		#undef MP_CRYPTO_KEM_SS_MAX
		#define MP_CRYPTO_KEM_SS_MAX MP_CRYPTO_RSAKEM_SS
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && (MP_CRYPTO_RSAKEM_SEED > MP_CRYPTO_KEM_SEED_MAX)
		#undef MP_CRYPTO_KEM_SEED_MAX
		#define MP_CRYPTO_KEM_SEED_MAX MP_CRYPTO_RSAKEM_SEED
	#endif
	#if defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && (MP_CRYPTO_RSAKEM_COINS > MP_CRYPTO_KEM_COINS_MAX)
		#undef MP_CRYPTO_KEM_COINS_MAX
		#define MP_CRYPTO_KEM_COINS_MAX MP_CRYPTO_RSAKEM_COINS
	#endif

	typedef struct mp_crypto_kem_algo_s mp_crypto_kem_algo_t;

	/** one algorithm of the build */
	struct mp_crypto_kem_algo_s {
		/** module name, "MLKEM" */
		const char *name;
		/** display name, "ML-KEM" */
		const char *label;
		unsigned int privSize;
		unsigned int pubSize;
		unsigned int ctSize;
		unsigned int ssSize;
		unsigned int seedSize;
		unsigned int coinsSize;

		mp_ret_t (*keypair)(const void *seed, unsigned int seedlen, void *priv, void *pub);
		unsigned int (*encaps)(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct);
		mp_bool_t (*decaps)(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss);
		/**
		 * Fuse path of decapsulation. wrapped is that algorithm's blob.
		 * YES and a shared secret, or NO when this series has no such accelerator.
		 */
		mp_bool_t (*decaps_fuse)(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);
	};

	/** @} */

	int mp_crypto_kem_count(void);
	const mp_crypto_kem_algo_t *mp_crypto_kem_at(int index);
	/** "MLKEM", "ml-kem"...; NULL when the build does not carry it */
	const mp_crypto_kem_algo_t *mp_crypto_kem_find(const char *name);

	/** FALSE when name is unknown or the seed is too short */
	mp_ret_t mp_crypto_kem_keypair(const char *name, const void *seed, unsigned int seedlen, void *priv, void *pub);
	/**
	 * Ciphertext bytes written into ct, or 0. ss receives ssSize bytes.
	 * coins is the algorithm's explicit randomness, so a test can replay it.
	 */
	unsigned int mp_crypto_kem_encaps(const char *name, const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct);
	/**
	 * YES when ct has the algorithm's length. ss is always written in that case.
	 * A ciphertext that fails the check still returns YES: ss is then the rejection
	 * secret, and the work does not depend on which one it was.
	 */
	mp_bool_t mp_crypto_kem_decaps(const char *name, const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss);
	/**
	 * Shared secret from a fuse slot. An algorithm with no accelerator prints once and returns NO.
	 */
	mp_bool_t mp_crypto_kem_decaps_fuse(const char *name, const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss);

	#ifdef SUPPORT_COMMON_CRYPTO_KDF_HKDF
		/** five bytes: "KEM" and the two-byte identifier */
		#define MP_CRYPTO_DHKEM_SUITE 5
		/** longest Diffie-Hellman shared secret ExtractAndExpand accepts */
		#define MP_CRYPTO_DHKEM_DH_MAX 128
		/** longest kem_context: ephemeral public key concatenated with the recipient public key */
		#define MP_CRYPTO_DHKEM_CONTEXT_MAX 160

		/**
		 * ExtractAndExpand from RFC 9180. hash is a name of HASH.
		 * suite is "KEM" followed by the identifier, for example 0x0020 for X25519.
		 * Writes ssLen bytes and returns ssLen, or 0 when hash is absent or a length does not fit.
		 */
		unsigned int mp_crypto_dhkem(uint8_t *ss, unsigned int ssLen, const char *hash, const uint8_t suite[MP_CRYPTO_DHKEM_SUITE], const uint8_t *dh, unsigned int dhLen, const uint8_t *kem_context, unsigned int contextLen);
	#endif

#endif
#endif
