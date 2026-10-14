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

#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC

/**
@defgroup mpCommonHmac HMAC

@ingroup mpCommon

@brief HMAC (RFC 2104) over any hash of the build

H((K ^ opad) || H((K ^ ipad) || message)), K the key padded to one block,
or its digest when it is longer than a block. The inner hash runs while
the message comes; the outer one only at the end, from the padded key
kept in the context.

@{
*/

/**
 * @brief Start a MAC
 *
 * @param[in] ctx Context, owned by the caller
 * @param[in] hash Name of the hash, as mp_hash_find() takes it: "SHA256"
 * @param[in] key Key bytes, NULL for an empty key
 * @param[in] keylen Key length; longer than the block, it is hashed first
 * @return FALSE when the build does not carry hash
 */
mp_ret_t mp_hmac_init(mp_hmac_t *ctx, const char *hash, const void *key, unsigned int keylen) {
	const mp_hash_algo_t *algo = mp_hash_find(hash);
	uint8_t pad[MP_HASH_BLOCK_MAX];
	unsigned int n;

	memset(ctx, 0, sizeof(*ctx));
	if(!algo || algo->block > MP_HASH_BLOCK_MAX)
		return(FALSE);
	if(!key)
		keylen = 0;

#ifdef MP_ARCH_HMAC
	if(mp_arch_hmac_init(&ctx->hw, algo, key, keylen) == YES) {
		ctx->algo = algo;
		ctx->onHardware = YES;
		return(TRUE);
	}
#endif

	memset(pad, 0, sizeof(pad));
	if(keylen > algo->block) {
		mp_hash_init(&ctx->inner, algo->name);
		mp_hash_update(&ctx->inner, key, keylen);
		mp_hash_digest(&ctx->inner, pad);
	}
	else if(keylen > 0)
		memcpy(pad, key, keylen);

	for(n = 0; n < algo->block; n++) {
		ctx->outer[n] = pad[n] ^ 0x5c;
		pad[n] ^= 0x36;
	}
	mp_hash_init(&ctx->inner, algo->name);
	mp_hash_update(&ctx->inner, pad, algo->block);
	memset(pad, 0, sizeof(pad));

	ctx->algo = algo;
	return(TRUE);
}

/**
 * @brief MAC more bytes
 *
 * Does nothing on a context that is not started.
 *
 * @param[in] ctx Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_hmac_update(mp_hmac_t *ctx, const void *data, unsigned int size) {
	if(!ctx->algo)
		return;
#ifdef MP_ARCH_HMAC
	if(ctx->onHardware == YES) {
		mp_arch_hmac_update(&ctx->hw, data, size);
		return;
	}
#endif
	mp_hash_update(&ctx->inner, data, size);
}

/**
 * @brief End a MAC
 *
 * The context is wiped afterwards.
 *
 * @param[in] ctx Context
 * @param[out] tag Digest size of the hash, at most MP_HMAC_SIZE_MAX bytes
 * @return Bytes written, 0 when ctx was not started
 */
unsigned int mp_hmac_final(mp_hmac_t *ctx, uint8_t *tag) {
	uint8_t inner[MP_HASH_SIZE_MAX];
	unsigned int size;

	if(!ctx->algo)
		return(0);
	size = ctx->algo->size;

#ifdef MP_ARCH_HMAC
	if(ctx->onHardware == YES) {
		mp_arch_hmac_final(&ctx->hw, tag);
		mp_hmac_free(ctx);
		return(size);
	}
#endif

	/* the inner context is wiped by its digest, so the outer hash runs in it */
	mp_hash_digest(&ctx->inner, inner);
	mp_hash_init(&ctx->inner, ctx->algo->name);
	mp_hash_update(&ctx->inner, ctx->outer, ctx->algo->block);
	mp_hash_update(&ctx->inner, inner, size);
	mp_hash_digest(&ctx->inner, tag);
	memset(inner, 0, sizeof(inner));
	mp_hmac_free(ctx);
	return(size);
}

/**
 * @brief Drop a MAC
 *
 * Wipes the context, the padded key with it. Safe on a context already ended.
 *
 * @param[in] ctx Context
 */
void mp_hmac_free(mp_hmac_t *ctx) {
	memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief One-shot MAC
 *
 * @param[in] hash Name of the hash
 * @param[in] key Key bytes
 * @param[in] keylen Key length
 * @param[in] data Message
 * @param[in] size Message length
 * @param[out] tag At most MP_HMAC_SIZE_MAX bytes
 * @return Bytes written, 0 when the build does not carry hash
 */
unsigned int mp_hmac(const char *hash, const void *key, unsigned int keylen, const void *data, unsigned int size, uint8_t *tag) {
	mp_hmac_t ctx;

	if(mp_hmac_init(&ctx, hash, key, keylen) != TRUE)
		return(0);
	mp_hmac_update(&ctx, data, size);
	return(mp_hmac_final(&ctx, tag));
}

/**@}*/

#endif
