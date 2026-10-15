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

#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_SIGN

/**
@defgroup mpCommonSign Signature API

@ingroup mpCommon

@brief Any signature of the build, picked by name

@code
uint8_t priv[MP_SIGN_PRIV_MAX];
uint8_t pub[MP_SIGN_PUB_MAX];
uint8_t sig[MP_SIGN_SIG_MAX];
unsigned int n;

if(mp_sign_keypair("Ed25519", seed, 32, priv, pub) == TRUE) {
	n = mp_sign("Ed25519", priv, msg, len, sig);
	if(n && mp_sign_verify("Ed25519", pub, msg, len, sig, n) == YES)
		;
}
@endcode

The caller owns every buffer. An RSA private key is MP_SIGN_PRIV_MAX bytes,
so this module keeps no context of its own. A new algorithm needs one line
in the table below.

@{
*/

#define _MP_SIGN_ENTRY(id, name, label, priv, pub, sig) \
	{ name, label, priv, pub, sig, mp_##id##_keypair, mp_##id##_sign, mp_##id##_verify }

static const mp_sign_algo_t _mp_sign_algos[] = {
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ED25519
	_MP_SIGN_ENTRY(ed25519, "ED25519", "Ed25519", MP_ED25519_PRIV, MP_ED25519_PUB, MP_ED25519_SIG),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_ECDSA_P256
	_MP_SIGN_ENTRY(ecdsa_p256, "ECDSA_P256", "ECDSA P-256", MP_ECDSA_P256_PRIV, MP_ECDSA_P256_PUB, MP_ECDSA_P256_SIG),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_RSA
	_MP_SIGN_ENTRY(rsa, "RSA", "RSA", MP_RSA_PRIV, MP_RSA_PUB, MP_RSA_SIG),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_LMS
	_MP_SIGN_ENTRY(lms, "LMS", "LMS (RFC 8554)", MP_LMS_PRIV, MP_LMS_PUB, MP_LMS_SIG),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_SLH
	_MP_SIGN_ENTRY(slh, "SLH", "SLH-DSA (FIPS 205)", MP_SLH_SK_SIZE, MP_SLH_PK_SIZE, MP_SLH_SIG_SIZE),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_SIGN_MLDSA
	_MP_SIGN_ENTRY(mldsa, "MLDSA", "ML-DSA (FIPS 204)", MP_MLDSA_SK_SIZE, MP_MLDSA_PK_SIZE, MP_MLDSA_SIG_SIZE),
#endif
	{ NULL, NULL, 0, 0, 0, NULL, NULL, NULL },
};

#define _MP_SIGN_COUNT ((int)(sizeof(_mp_sign_algos) / sizeof(_mp_sign_algos[0])) - 1)

static int _mp_sign_same(const char *a, const char *b) {
	char x, y;

	while(*a && *b) {
		x = *a++;
		y = *b++;
		if(x >= 'a' && x <= 'z')
			x -= 'a' - 'A';
		if(y >= 'a' && y <= 'z')
			y -= 'a' - 'A';
		if(x != y)
			return(0);
	}
	return(*a == *b);
}

/**
 * @brief Number of algorithms in the build
 */
int mp_sign_count(void) {
	return(_MP_SIGN_COUNT);
}

/**
 * @brief Algorithm at index, NULL past the end
 *
 * @param[in] index From 0 to mp_sign_count() - 1
 */
const mp_sign_algo_t *mp_sign_at(int index) {
	if(index < 0 || index >= _MP_SIGN_COUNT)
		return(NULL);
	return(&_mp_sign_algos[index]);
}

/**
 * @brief Algorithm by name
 *
 * @param[in] name Module name or label, any case: "ED25519", "ed25519"
 * @return NULL when the build does not carry it
 */
const mp_sign_algo_t *mp_sign_find(const char *name) {
	int n;

	if(!name)
		return(NULL);
	for(n = 0; n < _MP_SIGN_COUNT; n++) {
		if(_mp_sign_same(name, _mp_sign_algos[n].name) || _mp_sign_same(name, _mp_sign_algos[n].label))
			return(&_mp_sign_algos[n]);
	}
	return(NULL);
}

/**
 * @brief Build a key pair
 *
 * @param[in] name Module name or label, any case
 * @param[in] seed Entropy; each algorithm says how many bytes it needs
 * @param[out] priv At least the algorithm's privSize
 * @param[out] pub At least the algorithm's pubSize
 * @return FALSE when name is unknown or the seed is too short
 */
mp_ret_t mp_sign_keypair(const char *name, const void *seed, unsigned int seedlen, void *priv, void *pub) {
	const mp_sign_algo_t *algo = mp_sign_find(name);

	if(!algo)
		return(FALSE);
	return(algo->keypair(seed, seedlen, priv, pub));
}

/**
 * @brief Sign
 *
 * @param[in] name Module name or label, any case
 * @param[out] sig At least the algorithm's sigSize
 * @return Bytes written, 0 when name is unknown or the key cannot sign
 */
unsigned int mp_sign(const char *name, const void *priv, const void *msg, unsigned int len, uint8_t *sig) {
	const mp_sign_algo_t *algo = mp_sign_find(name);

	if(!algo)
		return(0);
	return(algo->sign(priv, msg, len, sig));
}

/**
 * @brief Check a signature
 *
 * @param[in] name Module name or label, any case
 * @return YES when it checks, NO otherwise
 */
mp_bool_t mp_sign_verify(const char *name, const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen) {
	const mp_sign_algo_t *algo = mp_sign_find(name);

	if(!algo)
		return(NO);
	return(algo->verify(pub, msg, len, sig, siglen));
}

/**@}*/

#endif
