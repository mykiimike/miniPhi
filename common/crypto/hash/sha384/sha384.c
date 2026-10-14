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

#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA384

/**
@defgroup mpCommonSha384 SHA-384

@ingroup mpCommon

@brief SHA-384 digest (FIPS 180-4)

Same compression as SHA-512. The initial value differs and the result is the first 48 bytes.

@{
*/

static const uint64_t _sha384_iv[8] = {
	0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
	0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL, 0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL,
};

/**
 * @brief Start a digest
 *
 * @param[in] ctx Context, owned by the caller
 */
void mp_sha384_init(mp_sha384_t *ctx) {
	mp_sha512_init_iv(&ctx->raw, _sha384_iv, MP_SHA384_SIZE);
#ifdef MP_ARCH_SHA384
	ctx->onHardware = mp_arch_sha384_init(&ctx->hw);
#endif
}

/**
 * @brief Hash more bytes
 *
 * @param[in] ctx Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_sha384_update(mp_sha384_t *ctx, const void *data, unsigned int size) {
#ifdef MP_ARCH_SHA384
	if(ctx->onHardware) {
		mp_arch_sha384_update(&ctx->hw, data, size);
		return;
	}
#endif
	mp_sha512_update(&ctx->raw, data, size);
}

/**
 * @brief End a digest
 *
 * The context is wiped, start again with mp_sha384_init().
 *
 * @param[in] ctx Context
 * @param[out] digest MP_SHA384_SIZE bytes
 */
void mp_sha384_final(mp_sha384_t *ctx, uint8_t digest[MP_SHA384_SIZE]) {
#ifdef MP_ARCH_SHA384
	if(ctx->onHardware)
		mp_arch_sha384_final(&ctx->hw, digest);
	else
#endif
	mp_sha512_final(&ctx->raw, digest);
	memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief Digest a buffer in one call
 *
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 * @param[out] digest MP_SHA384_SIZE bytes
 */
void mp_sha384(const void *data, unsigned int size, uint8_t digest[MP_SHA384_SIZE]) {
	mp_sha384_t ctx;

	mp_sha384_init(&ctx);
	mp_sha384_update(&ctx, data, size);
	mp_sha384_final(&ctx, digest);
}

/**@}*/

#endif
