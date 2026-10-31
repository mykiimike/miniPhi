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

#ifdef SUPPORT_COMMON_CRYPTO_KEM

/**
@defgroup mpCommonKem KEM API

@ingroup mpCommon

@brief Any key encapsulation of the build, picked by name

@code
uint8_t priv[MP_CRYPTO_KEM_PRIV_MAX];
uint8_t pub[MP_CRYPTO_KEM_PUB_MAX];
uint8_t ct[MP_CRYPTO_KEM_CT_MAX];
uint8_t ss[MP_CRYPTO_KEM_SS_MAX];
uint8_t back[MP_CRYPTO_KEM_SS_MAX];
unsigned int n;

if(mp_crypto_kem_keypair("MLKEM", seed, 64, priv, pub) == TRUE) {
	n = mp_crypto_kem_encaps("MLKEM", pub, coins, 32, ss, ct);
	if(n && mp_crypto_kem_decaps("MLKEM", priv, ct, n, back) == YES)
		;
}
@endcode

The caller owns every buffer. One algorithm keeps its own scratch, so one
encapsulation runs at a time. A new algorithm needs one line in the table
below and mp_crypto_<algo>_decaps_fuse().

@{
*/

#define _MP_CRYPTO_KEM_ENTRY(id, name, label, priv, pub, ct, ss, seed, coins) \
	{ name, label, priv, pub, ct, ss, seed, coins, \
	  mp_crypto_##id##_keypair, mp_crypto_##id##_encaps, mp_crypto_##id##_decaps, mp_crypto_##id##_decaps_fuse }

static const mp_crypto_kem_algo_t _mp_crypto_kem_algos[] = {
#ifdef SUPPORT_COMMON_CRYPTO_KEM_MLKEM
	_MP_CRYPTO_KEM_ENTRY(mlkem, "MLKEM", "ML-KEM", MP_CRYPTO_MLKEM_PRIV, MP_CRYPTO_MLKEM_PUB, MP_CRYPTO_MLKEM_CT, MP_CRYPTO_MLKEM_SS, MP_CRYPTO_MLKEM_SEED, MP_CRYPTO_MLKEM_COINS),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_KEM_HQC
	_MP_CRYPTO_KEM_ENTRY(hqc, "HQC", "HQC-128", MP_CRYPTO_HQC_PRIV, MP_CRYPTO_HQC_PUB, MP_CRYPTO_HQC_CT, MP_CRYPTO_HQC_SS, MP_CRYPTO_HQC_SEED, MP_CRYPTO_HQC_COINS),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_KEM_X25519
	_MP_CRYPTO_KEM_ENTRY(x25519, "X25519", "X25519", MP_CRYPTO_X25519_PRIV, MP_CRYPTO_X25519_PUB, MP_CRYPTO_X25519_CT, MP_CRYPTO_X25519_SS, MP_CRYPTO_X25519_SEED, MP_CRYPTO_X25519_COINS),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_KEM_X448
	_MP_CRYPTO_KEM_ENTRY(x448, "X448", "X448", MP_CRYPTO_X448_PRIV, MP_CRYPTO_X448_PUB, MP_CRYPTO_X448_CT, MP_CRYPTO_X448_SS, MP_CRYPTO_X448_SEED, MP_CRYPTO_X448_COINS),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_KEM_RSAKEM
	_MP_CRYPTO_KEM_ENTRY(rsakem, "RSAKEM", "RSA-KEM", MP_CRYPTO_RSAKEM_PRIV, MP_CRYPTO_RSAKEM_PUB, MP_CRYPTO_RSAKEM_CT, MP_CRYPTO_RSAKEM_SS, MP_CRYPTO_RSAKEM_SEED, MP_CRYPTO_RSAKEM_COINS),
#endif
	{ NULL, NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
};

#define _MP_CRYPTO_KEM_COUNT ((int)(sizeof(_mp_crypto_kem_algos) / sizeof(_mp_crypto_kem_algos[0])) - 1)

static int _mp_crypto_kem_same(const char *a, const char *b) {
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
 * @return The count
 */
int mp_crypto_kem_count(void) {
	return(_MP_CRYPTO_KEM_COUNT);
}

/**
 * @brief Algorithm at index, NULL past the end
 * @param[in] index From 0 to mp_crypto_kem_count() - 1
 * @return The algorithm, or NULL
 */
const mp_crypto_kem_algo_t *mp_crypto_kem_at(int index) {
	if(index < 0 || index >= _MP_CRYPTO_KEM_COUNT)
		return(NULL);
	return(&_mp_crypto_kem_algos[index]);
}

/**
 * @brief Algorithm by name
 * @param[in] name Module name or label, any case
 * @return NULL when the build does not carry it
 */
const mp_crypto_kem_algo_t *mp_crypto_kem_find(const char *name) {
	int n;

	if(!name)
		return(NULL);
	for(n = 0; n < _MP_CRYPTO_KEM_COUNT; n++) {
		if(_mp_crypto_kem_same(name, _mp_crypto_kem_algos[n].name) || _mp_crypto_kem_same(name, _mp_crypto_kem_algos[n].label))
			return(&_mp_crypto_kem_algos[n]);
	}
	return(NULL);
}

/**
 * @brief Build a key pair
 * @param[in] name Module name or label, any case
 * @param[in] seed Entropy; each algorithm says how many bytes it needs
 * @param[in] seedlen Length of seed
 * @param[out] priv At least the algorithm's privSize
 * @param[out] pub At least the algorithm's pubSize
 * @return FALSE when name is unknown or the seed is too short
 */
mp_ret_t mp_crypto_kem_keypair(const char *name, const void *seed, unsigned int seedlen, void *priv, void *pub) {
	const mp_crypto_kem_algo_t *algo = mp_crypto_kem_find(name);

	if(!algo)
		return(FALSE);
	return(algo->keypair(seed, seedlen, priv, pub));
}

/**
 * @brief Encapsulate
 * @param[in] name Module name or label, any case
 * @param[in] pub The public key
 * @param[in] coins Explicit randomness
 * @param[in] coinslen Length of coins
 * @param[out] ss The shared secret, ssSize bytes
 * @param[out] ct The ciphertext, ctSize bytes
 * @return Bytes written into ct, or 0
 */
unsigned int mp_crypto_kem_encaps(const char *name, const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct) {
	const mp_crypto_kem_algo_t *algo = mp_crypto_kem_find(name);

	if(!algo)
		return(0);
	return(algo->encaps(pub, coins, coinslen, ss, ct));
}

/**
 * @brief Decapsulate
 * @param[in] name Module name or label, any case
 * @param[in] priv The private key
 * @param[in] ct The ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss The shared secret, ssSize bytes
 * @return YES when ctlen is right. NO when the name is unknown or the length is not.
 */
mp_bool_t mp_crypto_kem_decaps(const char *name, const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss) {
	const mp_crypto_kem_algo_t *algo = mp_crypto_kem_find(name);

	if(!algo)
		return(NO);
	return(algo->decaps(priv, ct, ctlen, ss));
}

/**
 * @brief Decapsulate with a fuse slot
 * @param[in] name Module name or label, any case
 * @param[in] fuse Slot the algorithm names
 * @param[in] wrapped That algorithm's wrapped key
 * @param[in] wrappedlen Byte length of wrapped
 * @param[in] ct The ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss The shared secret
 * @return YES when the accelerator wrote ss. NO when it has no such path.
 */
mp_bool_t mp_crypto_kem_decaps_fuse(const char *name, const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss) {
	const mp_crypto_kem_algo_t *algo = mp_crypto_kem_find(name);

	if(!algo || !algo->decaps_fuse)
		return(NO);
	return(algo->decaps_fuse(fuse, wrapped, wrappedlen, ct, ctlen, ss));
}

/**@}*/

#endif
