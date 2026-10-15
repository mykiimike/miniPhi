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

#ifdef SUPPORT_COMMON_CRYPTO_MODE

#include <string.h>

/**
@defgroup mpCommonMode Cipher mode API

@ingroup mpCommon

@brief Any mode of the build, picked by name, wrapping a started cipher

@code
mp_cipher_t cipher;
mp_cipher_mode_t mode;
uint8_t out[32], tag[16];

if(mp_cipher_init(&cipher, "AES", key, 16) == TRUE) {
	if(mp_cipher_mode_init(&mode, "GCM", &cipher, iv, 12, TRUE) == TRUE) {
		mp_cipher_mode_aad(&mode, aad, aadlen);
		mp_cipher_mode_update(&mode, in, inlen, out);
		mp_cipher_mode_final(&mode, NULL, tag);
		mp_cipher_mode_free(&mode);
	}
	mp_cipher_free(&cipher);
}
@endcode

A new mode needs its functions and one line in the table below.

@{
*/

#ifdef SUPPORT_COMMON_CRYPTO_MODE_CBC
	extern const mp_cipher_mode_algo_t mp_cipher_mode_cbc_algo;
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MODE_CTR
	extern const mp_cipher_mode_algo_t mp_cipher_mode_ctr_algo;
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MODE_GCM
	extern const mp_cipher_mode_algo_t mp_cipher_mode_gcm_algo;
#endif

static const mp_cipher_mode_algo_t *_mp_mode_algos[] = {
#ifdef SUPPORT_COMMON_CRYPTO_MODE_CBC
	&mp_cipher_mode_cbc_algo,
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MODE_CTR
	&mp_cipher_mode_ctr_algo,
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MODE_GCM
	&mp_cipher_mode_gcm_algo,
#endif
	NULL,
};

#define _MP_MODE_COUNT ((int)(sizeof(_mp_mode_algos) / sizeof(_mp_mode_algos[0])) - 1)

static int _mp_mode_same(const char *a, const char *b) {
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
 * @brief Number of modes in the build
 */
int mp_cipher_mode_count(void) {
	return(_MP_MODE_COUNT);
}

/**
 * @brief Mode at index, NULL past the end
 *
 * @param[in] index From 0 to mp_cipher_mode_count() - 1
 */
const mp_cipher_mode_algo_t *mp_cipher_mode_at(int index) {
	if(index < 0 || index >= _MP_MODE_COUNT)
		return(NULL);
	return(_mp_mode_algos[index]);
}

/**
 * @brief Mode by name
 *
 * @param[in] name Module name or label, any case: "GCM", "gcm"
 * @return NULL when the build does not carry it
 */
const mp_cipher_mode_algo_t *mp_cipher_mode_find(const char *name) {
	if(!name)
		return(NULL);
	for(int n = 0; n < _MP_MODE_COUNT; n++) {
		if(_mp_mode_same(name, _mp_mode_algos[n]->name) || _mp_mode_same(name, _mp_mode_algos[n]->label))
			return(_mp_mode_algos[n]);
	}
	return(NULL);
}

/**
 * @brief Start a mode over a started cipher
 *
 * @param[in] m Mode context, owned by the caller
 * @param[in] name Module name or label, any case
 * @param[in] cipher A cipher the caller already started
 * @param[in] iv IV or nonce bytes
 * @param[in] ivlen IV bytes (CBC/CTR = 16, GCM = 12)
 * @param[in] encrypt TRUE for encryption, FALSE for decryption
 * @return TRUE on success, FALSE when name is unknown, the IV is wrong or the cipher is not started
 */
mp_ret_t mp_cipher_mode_init(mp_cipher_mode_t *m, const char *name, mp_cipher_t *cipher, const void *iv, unsigned int ivlen, mp_bool_t encrypt) {
	const mp_cipher_mode_algo_t *algo = mp_cipher_mode_find(name);
	if(!m)
		return(FALSE);
	memset(m, 0, sizeof(*m));
	if(!algo)
		return(FALSE);
	mp_ret_t r = algo->init(m, cipher, iv, ivlen, encrypt); /* may memset m again */
	if(r != TRUE) {
		memset(m, 0, sizeof(*m));
		return(FALSE);
	}
	m->algo = algo; /* set after the mode wiped and set up its own state */
	return(TRUE);
}

/**
 * @brief Feed data through the mode
 *
 * @param[in] m Started mode
 * @param[in] in Data bytes
 * @param[in] inlen Length of in
 * @param[out] out At least inlen bytes
 * @return Bytes written to out
 */
unsigned int mp_cipher_mode_update(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out) {
	if(!m || !m->algo)
		return(0);
	return(m->algo->update(m, in, inlen, out));
}

/**
 * @brief Feed associated data (AEAD only)
 *
 * @param[in] m Started mode, before any mp_cipher_mode_update()
 * @param[in] aad Associated data bytes
 * @param[in] len Length of aad
 * @return Bytes absorbed, 0 on a non-AEAD mode
 */
unsigned int mp_cipher_mode_aad(mp_cipher_mode_t *m, const void *aad, unsigned int len) {
	if(!m || !m->algo)
		return(0);
	return(m->algo->update_aad(m, aad, len));
}

/**
 * @brief Finish the mode
 *
 * @param[in] m Started mode
 * @param[out] out Last block bytes (CBC padding); may be NULL on a stream mode
 * @param[out] tag AEAD tag when the mode authenticates; may be NULL
 * @return Bytes written to out
 */
unsigned int mp_cipher_mode_final(mp_cipher_mode_t *m, uint8_t *out, uint8_t *tag) {
	if(!m || !m->algo)
		return(0);
	return(m->algo->final(m, out, tag));
}

/**
 * @brief Drop a mode context
 *
 * Wipes the state. Safe on a mode already freed.
 *
 * @param[in] m Mode context (or NULL)
 */
void mp_cipher_mode_free(mp_cipher_mode_t *m) {
	if(!m)
		return;
	if(m->algo)
		m->algo->free(m);
	else
		memset(m, 0, sizeof(*m));
}

/**@}*/

#endif
