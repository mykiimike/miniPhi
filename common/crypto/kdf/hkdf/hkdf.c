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

#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_KDF_HKDF

/**
@defgroup mpCommonHkdf HKDF

@ingroup mpCommon

@brief HKDF (RFC 5869) over any hash of the build

Extract is HMAC(salt, IKM). An empty salt is one block of zeros, HashLen
bytes. Expand is HMAC(PRK, T(i-1) || info || i) for i from 1, and the
caller keeps the first L bytes. L is a public length, at most 255 digests.
The hash name is the same one HASH accepts.

@{
*/

/**
 * @brief HKDF-Extract
 * @param[in] hash A name of HASH, "SHA256" or "SHA-512"
 * @param[in] salt Salt, ignored when saltlen is 0
 * @param[in] saltlen Salt length; 0 selects HashLen zero bytes
 * @param[in] ikm Input keying material
 * @param[in] ikmlen Length of ikm
 * @param[out] prk Digest-sized pseudorandom key
 * @return The PRK length, or 0 when hash is absent
 */
unsigned int mp_crypto_hkdf_extract(const char *hash, const void *salt, unsigned int saltlen, const void *ikm, unsigned int ikmlen, uint8_t *prk) {
	const mp_crypto_hash_algo_t *algo;
	uint8_t salt0[MP_CRYPTO_HASH_SIZE_MAX];
	uint8_t empty = 0;
	unsigned int n;

	algo = mp_crypto_hash_find(hash);
	if(!algo || !prk || (ikmlen && !ikm) || (saltlen && !salt))
		return(0);
	if(!ikmlen)
		ikm = &empty;
	if(saltlen)
		return(mp_crypto_hmac(hash, salt, saltlen, ikm, ikmlen, prk));
	memset(salt0, 0, algo->size);
	n = mp_crypto_hmac(hash, salt0, algo->size, ikm, ikmlen, prk);
	memset(salt0, 0, sizeof(salt0));
	return(n);
}

/**
 * @brief HKDF-Expand
 * @param[in] hash A name of HASH
 * @param[in] prk Pseudorandom key, normally the extract output
 * @param[in] prklen Length of prk
 * @param[in] info Context, ignored when infolen is 0
 * @param[in] infolen Length of info
 * @param[out] out First L bytes of the expansion
 * @param[in] L Output length, from 1 to 255 digests
 * @return L, or 0 when hash is absent or L does not fit
 */
unsigned int mp_crypto_hkdf_expand(const char *hash, const void *prk, unsigned int prklen, const void *info, unsigned int infolen, uint8_t *out, unsigned int L) {
	const mp_crypto_hash_algo_t *algo;
	mp_crypto_hmac_t ctx;
	uint8_t prev[MP_CRYPTO_HASH_SIZE_MAX];
	uint8_t block[MP_CRYPTO_HASH_SIZE_MAX];
	unsigned int nblocks, i, off, take;
	uint8_t counter;

	algo = mp_crypto_hash_find(hash);
	if(!algo || !prk || !prklen || !out || !L || (infolen && !info))
		return(0);
	if(L > 255u * algo->size)
		return(0);
	nblocks = (L + algo->size - 1) / algo->size;
	off = 0;
	for(i = 1; i <= nblocks; i++) {
		counter = (uint8_t)i;
		if(mp_crypto_hmac_init(&ctx, hash, prk, prklen) != TRUE) {
			memset(prev, 0, sizeof(prev));
			memset(block, 0, sizeof(block));
			return(0);
		}
		if(i > 1)
			mp_crypto_hmac_update(&ctx, prev, algo->size);
		if(infolen)
			mp_crypto_hmac_update(&ctx, info, infolen);
		mp_crypto_hmac_update(&ctx, &counter, 1);
		if(mp_crypto_hmac_final(&ctx, block) != algo->size) {
			memset(prev, 0, sizeof(prev));
			memset(block, 0, sizeof(block));
			return(0);
		}
		take = algo->size;
		if(off + take > L)
			take = L - off;
		memcpy(out + off, block, take);
		memcpy(prev, block, algo->size);
		off += take;
	}
	memset(prev, 0, sizeof(prev));
	memset(block, 0, sizeof(block));
	return(L);
}

/**
 * @brief Extract, then expand
 * @param[in] hash A name of HASH
 * @param[in] salt Salt, ignored when saltlen is 0
 * @param[in] saltlen Salt length
 * @param[in] ikm Input keying material
 * @param[in] ikmlen Length of ikm
 * @param[in] info Context, ignored when infolen is 0
 * @param[in] infolen Length of info
 * @param[out] out First L bytes
 * @param[in] L Output length
 * @return L, or 0
 */
unsigned int mp_crypto_hkdf(const char *hash, const void *salt, unsigned int saltlen, const void *ikm, unsigned int ikmlen, const void *info, unsigned int infolen, uint8_t *out, unsigned int L) {
	uint8_t prk[MP_CRYPTO_HASH_SIZE_MAX];
	unsigned int n;

	n = mp_crypto_hkdf_extract(hash, salt, saltlen, ikm, ikmlen, prk);
	if(!n)
		return(0);
	n = mp_crypto_hkdf_expand(hash, prk, n, info, infolen, out, L);
	memset(prk, 0, sizeof(prk));
	return(n);
}

/**@}*/

#endif
