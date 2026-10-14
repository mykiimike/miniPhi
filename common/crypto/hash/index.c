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

#ifdef SUPPORT_COMMON_CRYPTO_HASH

/**
@defgroup mpCommonHash Hash API

@ingroup mpCommon

@brief Any hash of the build, picked by name

@code
mp_hash_t hash;
uint8_t digest[MP_HASH_SIZE_MAX];

if(mp_hash_init(&hash, "SHA256") == TRUE) {
	mp_hash_update(&hash, "abc", 3);
	mp_hash_digest(&hash, digest);
}
@endcode

A new algorithm needs its wrappers and one line in the table below,
and a member in the union of hash.h.

@{
*/

#define _MP_HASH_WRAP(id, type) \
	static void _mp_hash_##id##_init(void *ctx) { mp_##id##_init((type *)ctx); } \
	static void _mp_hash_##id##_update(void *ctx, const void *data, unsigned int size) { mp_##id##_update((type *)ctx, data, size); } \
	static void _mp_hash_##id##_final(void *ctx, uint8_t *digest) { mp_##id##_final((type *)ctx, digest); }

#define _MP_HASH_ENTRY(id, name, label, size, block) \
	{ name, label, size, block, _mp_hash_##id##_init, _mp_hash_##id##_update, _mp_hash_##id##_final }

#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
_MP_HASH_WRAP(blake2s, mp_blake2s_t)
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA1
_MP_HASH_WRAP(sha1, mp_sha1_t)
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA256
_MP_HASH_WRAP(sha256, mp_sha256_t)
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA384
_MP_HASH_WRAP(sha384, mp_sha384_t)
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512
_MP_HASH_WRAP(sha512, mp_sha512_t)
#endif

static const mp_hash_algo_t _mp_hash_algos[] = {
#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
	_MP_HASH_ENTRY(blake2s, "BLAKE2S", "BLAKE2s", MP_BLAKE2S_SIZE, MP_BLAKE2S_BLOCK),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA1
	_MP_HASH_ENTRY(sha1, "SHA1", "SHA-1", MP_SHA1_SIZE, MP_SHA1_BLOCK),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA256
	_MP_HASH_ENTRY(sha256, "SHA256", "SHA-256", MP_SHA256_SIZE, MP_SHA256_BLOCK),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA384
	_MP_HASH_ENTRY(sha384, "SHA384", "SHA-384", MP_SHA384_SIZE, MP_SHA512_BLOCK),
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512
	_MP_HASH_ENTRY(sha512, "SHA512", "SHA-512", MP_SHA512_SIZE, MP_SHA512_BLOCK),
#endif
	{ NULL, NULL, 0, 0, NULL, NULL, NULL },
};

#define _MP_HASH_COUNT ((int)(sizeof(_mp_hash_algos) / sizeof(_mp_hash_algos[0])) - 1)

static int _mp_hash_same(const char *a, const char *b) {
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
int mp_hash_count(void) {
	return(_MP_HASH_COUNT);
}

/**
 * @brief Algorithm at index, NULL past the end
 *
 * @param[in] index From 0 to mp_hash_count() - 1
 */
const mp_hash_algo_t *mp_hash_at(int index) {
	if(index < 0 || index >= _MP_HASH_COUNT)
		return(NULL);
	return(&_mp_hash_algos[index]);
}

/**
 * @brief Algorithm by name
 *
 * @param[in] name Module name or label, any case: "SHA256", "sha-256"
 * @return NULL when the build does not carry it
 */
const mp_hash_algo_t *mp_hash_find(const char *name) {
	int n;

	if(!name)
		return(NULL);
	for(n = 0; n < _MP_HASH_COUNT; n++) {
		if(_mp_hash_same(name, _mp_hash_algos[n].name) || _mp_hash_same(name, _mp_hash_algos[n].label))
			return(&_mp_hash_algos[n]);
	}
	return(NULL);
}

/**
 * @brief Start a digest
 *
 * @param[in] hash Context, owned by the caller
 * @param[in] name Module name or label, any case
 * @return FALSE when the build does not carry name
 */
mp_ret_t mp_hash_init(mp_hash_t *hash, const char *name) {
	const mp_hash_algo_t *algo = mp_hash_find(name);

	memset(hash, 0, sizeof(*hash));
	if(!algo)
		return(FALSE);
	hash->algo = algo;
	algo->init(&hash->ctx);
	return(TRUE);
}

/**
 * @brief Hash more bytes
 *
 * Does nothing on a hash that is not started.
 *
 * @param[in] hash Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_hash_update(mp_hash_t *hash, const void *data, unsigned int size) {
	if(!hash->algo)
		return;
	hash->algo->update(&hash->ctx, data, size);
}

/**
 * @brief End a digest
 *
 * The context is wiped afterwards.
 *
 * @param[in] hash Context
 * @param[out] digest At least MP_HASH_SIZE_MAX bytes, or mp_hash_size()
 * @return Bytes written, 0 when hash was not started
 */
unsigned int mp_hash_digest(mp_hash_t *hash, uint8_t *digest) {
	unsigned int size;

	if(!hash->algo)
		return(0);
	size = hash->algo->size;
	hash->algo->final(&hash->ctx, digest);
	mp_hash_free(hash);
	return(size);
}

/**
 * @brief Drop a digest
 *
 * Wipes the context. Safe on a hash already ended.
 *
 * @param[in] hash Context
 */
void mp_hash_free(mp_hash_t *hash) {
	memset(hash, 0, sizeof(*hash));
}

/**
 * @brief Digest size of a started hash
 *
 * @param[in] hash Context
 * @return Bytes, 0 when not started
 */
unsigned int mp_hash_size(const mp_hash_t *hash) {
	return(hash->algo ? hash->algo->size : 0);
}

/**@}*/

#endif
