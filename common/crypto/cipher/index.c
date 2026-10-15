/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER

/**
@defgroup mpCommonCipher Cipher API

@ingroup mpCommon

@brief Any cipher of the build, picked by name

@code
mp_cipher_t cipher;
uint8_t out[MP_CIPHER_BLOCK_MAX];

if(mp_cipher_init(&cipher, "AES", key, 16) == TRUE) {
	unsigned int n = mp_cipher_update_block(&cipher, in, inlen, out);
	mp_cipher_free(&cipher);
}
@endcode

A new algorithm needs its wrappers and one line in the table below,
and a member in the union of cipher.h.

@{
*/

#define _MP_CIPHER_WRAP(id, type) \
	static mp_ret_t _mp_cipher_##id##_init(void *ctx, const void *key, unsigned int keylen) { return mp_##id##_init((type *)ctx, key, keylen); } \
	static unsigned int _mp_cipher_##id##_update_block(void *ctx, const void *in, unsigned int inlen, uint8_t *out) { return mp_##id##_update_block((type *)ctx, in, inlen, out); } \
	static unsigned int _mp_cipher_##id##_update_block_inv(void *ctx, const void *in, unsigned int inlen, uint8_t *out) { return mp_##id##_update_block_inv((type *)ctx, in, inlen, out); } \
	static void _mp_cipher_##id##_free(void *ctx) { mp_##id##_free((type *)ctx); }

#define _MP_CIPHER_ENTRY(id, name, label, block, key) \
	{ name, label, block, key, _mp_cipher_##id##_init, _mp_cipher_##id##_update_block, _mp_cipher_##id##_update_block_inv, _mp_cipher_##id##_free }

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_AES
	_MP_CIPHER_WRAP(aes, mp_aes_t)
#endif

static const mp_cipher_algo_t _mp_cipher_algos[] = {
#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_AES
	_MP_CIPHER_ENTRY(aes, "AES", "AES", MP_AES_BLOCK, MP_AES_KEY),
#endif
	{ NULL, NULL, 0, 0, NULL, NULL, NULL, NULL },
};

#define _MP_CIPHER_COUNT ((int)(sizeof(_mp_cipher_algos) / sizeof(_mp_cipher_algos[0])) - 1)

static int _mp_cipher_same(const char *a, const char *b) {
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
 * @brief Number of ciphers in the build
 */
int mp_cipher_count(void) {
	return(_MP_CIPHER_COUNT);
}

/**
 * @brief Cipher at index, NULL past the end
 *
 * @param[in] index From 0 to mp_cipher_count() - 1
 */
const mp_cipher_algo_t *mp_cipher_at(int index) {
	if(index < 0 || index >= _MP_CIPHER_COUNT)
		return(NULL);
	return(&_mp_cipher_algos[index]);
}

/**
 * @brief Cipher by name
 *
 * @param[in] name Module name or label, any case: "AES", "aes"
 * @return NULL when the build does not carry it
 */
const mp_cipher_algo_t *mp_cipher_find(const char *name) {
	int n;

	if(!name)
		return(NULL);
	for(n = 0; n < _MP_CIPHER_COUNT; n++) {
		if(_mp_cipher_same(name, _mp_cipher_algos[n].name) || _mp_cipher_same(name, _mp_cipher_algos[n].label))
			return(&_mp_cipher_algos[n]);
	}
	return(NULL);
}

/**
 * @brief Start a cipher (key schedule)
 *
 * @param[in] cipher Context, owned by the caller
 * @param[in] name Module name or label, any case
 * @param[in] key Key bytes
 * @param[in] keylen Key bytes (AES-128 = 16, AES-192 = 24, AES-256 = 32)
 * @return FALSE when name is not in the build or the key length is wrong
 */
mp_ret_t mp_cipher_init(mp_cipher_t *ctx, const char *name, const void *key, unsigned int keylen) {
	const mp_cipher_algo_t *algo = mp_cipher_find(name);

	memset(ctx, 0, sizeof(*ctx));
	if(!algo)
		return(FALSE);
	if(keylen != algo->keySize)
		return(FALSE);
	ctx->algo = algo;
	algo->init(&ctx->ctx, key, keylen);
	return(TRUE);
}

/**
 * @brief Encrypt one block (or less)
 *
 * Does nothing on a cipher that is not started.
 *
 * @param[in] cipher Context
 * @param[in] in Bytes, at most the block
 * @param[in] inlen Bytes to take from in
 * @param[out] out At least the block bytes
 * @return Bytes written, 0 when the cipher was not started
 */
unsigned int mp_cipher_update_block(mp_cipher_t *ctx, const void *in, unsigned int inlen, uint8_t *out) {
	if(!ctx->algo)
		return(0);
	return(ctx->algo->update_block(&ctx->ctx, in, inlen, out));
}

/**
 * @brief Decrypt one block (or less)
 *
 * Does nothing on a cipher that is not started.
 *
 * @param[in] cipher Context
 * @param[in] in Bytes, at most the block
 * @param[in] inlen Bytes to take from in
 * @param[out] out At least the block bytes
 * @return Bytes written, 0 when the cipher was not started
 */
unsigned int mp_cipher_update_block_inv(mp_cipher_t *ctx, const void *in, unsigned int inlen, uint8_t *out) {
	if(!ctx->algo)
		return(0);
	return(ctx->algo->update_block_inv(&ctx->ctx, in, inlen, out));
}

/**
 * @brief Drop a cipher
 *
 * Wipes the context. Safe on a cipher already freed.
 *
 * @param[in] cipher Context
 */
void mp_cipher_free(mp_cipher_t *ctx) {
	memset(ctx, 0, sizeof(*ctx));
}

/**@}*/

#endif
