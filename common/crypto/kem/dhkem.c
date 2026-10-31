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

#if defined(SUPPORT_COMMON_CRYPTO_KEM) && defined(SUPPORT_COMMON_CRYPTO_KDF_HKDF)

/**
@defgroup mpCommonDhkem DHKEM

@ingroup mpCommonKem

@brief ExtractAndExpand from RFC 9180

The labels stay here. A curve passes its hash name, its five-byte suite and
the raw Diffie-Hellman shared secret. X25519 uses SHA-256 and suite 0x0020.
X448 uses SHA-512 and suite 0x0021. The suite is "KEM" and the identifier.
An empty salt is what ExtractAndExpand specifies, so HKDF supplies HashLen
zero bytes.

@{
*/

/** I2OSP(L, 2) || "HPKE-v1" || suite || "shared_secret" || kem_context. Extract's input is shorter. */
#define _LABELED_MAX (2u + 7u + MP_CRYPTO_DHKEM_SUITE + 13u + MP_CRYPTO_DHKEM_CONTEXT_MAX)

/**
 * @brief ExtractAndExpand
 * @param[out] ss Shared secret, ssLen bytes
 * @param[in] ssLen Output length, the suite's Nsecret
 * @param[in] hash A name of HASH, "SHA256" or "SHA512"
 * @param[in] suite Five bytes, "KEM" and the identifier
 * @param[in] dh Diffie-Hellman shared secret
 * @param[in] dhLen Length of dh, from 1 to MP_CRYPTO_DHKEM_DH_MAX
 * @param[in] kem_context Ephemeral public key concatenated with the recipient public key
 * @param[in] contextLen Length of kem_context, at most MP_CRYPTO_DHKEM_CONTEXT_MAX
 * @return ssLen, or 0 when hash is absent or a length does not fit
 */
unsigned int mp_crypto_dhkem(uint8_t *ss, unsigned int ssLen, const char *hash, const uint8_t suite[MP_CRYPTO_DHKEM_SUITE], const uint8_t *dh, unsigned int dhLen, const uint8_t *kem_context, unsigned int contextLen) {
	uint8_t labeled[_LABELED_MAX];
	uint8_t prk[MP_CRYPTO_HASH_SIZE_MAX];
	unsigned int n, prkLen;

	if(!ss || !ssLen || ssLen > 65535u || !hash || !suite || !dh || !dhLen || dhLen > MP_CRYPTO_DHKEM_DH_MAX)
		return(0);
	if(contextLen > MP_CRYPTO_DHKEM_CONTEXT_MAX || (contextLen && !kem_context))
		return(0);
	n = 0;
	memcpy(labeled + n, "HPKE-v1", 7);
	n += 7;
	memcpy(labeled + n, suite, MP_CRYPTO_DHKEM_SUITE);
	n += MP_CRYPTO_DHKEM_SUITE;
	memcpy(labeled + n, "eae_prk", 7);
	n += 7;
	memcpy(labeled + n, dh, dhLen);
	n += dhLen;
	prkLen = mp_crypto_hkdf_extract(hash, NULL, 0, labeled, n, prk);
	if(!prkLen) {
		memset(labeled, 0, sizeof(labeled));
		memset(prk, 0, sizeof(prk));
		return(0);
	}
	memset(labeled, 0, sizeof(labeled));
	labeled[0] = (uint8_t)(ssLen >> 8);
	labeled[1] = (uint8_t)ssLen;
	n = 2;
	memcpy(labeled + n, "HPKE-v1", 7);
	n += 7;
	memcpy(labeled + n, suite, MP_CRYPTO_DHKEM_SUITE);
	n += MP_CRYPTO_DHKEM_SUITE;
	memcpy(labeled + n, "shared_secret", 13);
	n += 13;
	if(contextLen)
		memcpy(labeled + n, kem_context, contextLen);
	n += contextLen;
	n = mp_crypto_hkdf_expand(hash, prk, prkLen, labeled, n, ss, ssLen);
	memset(labeled, 0, sizeof(labeled));
	memset(prk, 0, sizeof(prk));
	if(n != ssLen) {
		memset(ss, 0, ssLen);
		return(0);
	}
	return(ssLen);
}

/**@}*/

#endif
