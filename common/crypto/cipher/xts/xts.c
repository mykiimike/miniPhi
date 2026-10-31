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

#ifdef SUPPORT_COMMON_CRYPTO_CIPHER_XTS

#include "xts.h"
#include <string.h>

/**
@defgroup mpCommonXts XTS

@ingroup mpCommon

@brief XTS-AES (IEEE 1619), one data unit at a time

Key1 encrypts the data and Key2 encrypts the 16-byte tweak. Each block
is XORed with the tweak, passed through AES, and XORed again. The tweak
then advances by a multiplication by x in GF(2^128), little-endian, with
the polynomial x^128 + x^7 + x^2 + x + 1. A data unit shorter than 16
bytes is refused. A unit whose length is not a multiple of 16 ends with
ciphertext stealing.

The running tweak is secret: it is AES(Key2, tweak) and then its
multiples. Multiplying by x shifts 16 bytes and, when the high bit is
set, XORs 0x87 into the first byte. That bit selects the reduction
through a mask, the same way xtime does, so the path does not branch
on it. The data-unit length is public, so the stealing step may walk
the leftover bytes.

The software path always builds. mp_crypto_xts_crypt() is the separate flash
line, and only when the series declares an XTS accelerator and a fuse.

@{
*/

/* T = T * x. The carry out of byte 15 is a bit of the secret tweak. */
static void _mul(uint8_t *t) {
	unsigned int i;
	uint8_t cin = 0;

	for(i = 0; i < 16; i++) {
		uint8_t cout = (uint8_t)(t[i] >> 7);

		t[i] = (uint8_t)((t[i] << 1) | cin);
		cin = cout;
	}
	t[0] ^= (uint8_t)(0x87 & (uint8_t)(0 - cin));
}

static void _block(mp_crypto_xts_t *ctx, const uint8_t *in, uint8_t *out, const uint8_t *T, int enc) {
	uint8_t b[16];
	unsigned int i;

	for(i = 0; i < 16; i++)
		b[i] = (uint8_t)(in[i] ^ T[i]);
	if(enc)
		mp_crypto_aes_update_block(&ctx->data, b, 16, b);
	else
		mp_crypto_aes_update_block_inv(&ctx->data, b, 16, b);
	for(i = 0; i < 16; i++)
		out[i] = (uint8_t)(b[i] ^ T[i]);
	memset(b, 0, sizeof(b));
}

static unsigned int _unit(mp_crypto_xts_t *ctx, const uint8_t *in, unsigned int len, uint8_t *out, int enc) {
	uint8_t T[16];
	uint8_t block[16];
	uint8_t prev[16];
	unsigned int left;
	unsigned int rem;
	unsigned int i;

	if(!ctx || !ctx->armed || !in || !out || len < 16)
		return(0);
	memcpy(T, ctx->T, 16);
	rem = len % 16;
	left = len;
	if(!enc && rem)
		left -= 16;
	while(left >= 16) {
		_block(ctx, in, out, T, enc);
		in += 16;
		out += 16;
		left -= 16;
		if(!left && !rem) {
			memset(T, 0, sizeof(T));
			return(len);
		}
		_mul(T);
	}
	if(enc) {
		memcpy(prev, out - 16, 16);
		for(i = 0; i < rem; i++) {
			block[i] = in[i];
			out[i] = prev[i];
		}
		for(i = rem; i < 16; i++)
			block[i] = prev[i];
		_block(ctx, block, out - 16, T, 1);
	}
	else {
		uint8_t next[16];

		memcpy(next, T, 16);
		_mul(next);
		_block(ctx, in, prev, next, 0);
		for(i = 0; i < rem; i++) {
			block[i] = in[16 + i];
			out[16 + i] = prev[i];
		}
		for(i = rem; i < 16; i++)
			block[i] = prev[i];
		_block(ctx, block, out, T, 0);
		memset(next, 0, sizeof(next));
	}
	memset(T, 0, sizeof(T));
	memset(block, 0, sizeof(block));
	memset(prev, 0, sizeof(prev));
	return(len);
}

/**
 * @brief Start an XTS-AES context
 *
 * @param[in] ctx Uninitialised context
 * @param[in] key Key1 || Key2
 * @param[in] keylen 32 or 64
 * @return FALSE when the length is neither
 */
mp_ret_t mp_crypto_xts_init(mp_crypto_xts_t *ctx, const void *key, unsigned int keylen) {
	const uint8_t *k = key;
	unsigned int half;

	if(!ctx || !key || (keylen != 32 && keylen != 64))
		return(FALSE);
	memset(ctx, 0, sizeof(*ctx));
	half = keylen / 2;
	if(mp_crypto_aes_init(&ctx->data, k, half) != TRUE || mp_crypto_aes_init(&ctx->tweak, k + half, half) != TRUE) {
		mp_crypto_xts_free(ctx);
		return(FALSE);
	}
	return(TRUE);
}

/**
 * @brief Set the tweak of the next data unit
 *
 * @param[in] ctx Started context
 * @param[in] tweak 16 bytes, little-endian data-unit number
 * @param[in] len Byte length of tweak
 * @return FALSE when len is not 16
 */
mp_ret_t mp_crypto_xts_set_tweak(mp_crypto_xts_t *ctx, const void *tweak, unsigned int len) {
	if(!ctx || !ctx->data.rounds || !tweak || len != 16)
		return(FALSE);
	mp_crypto_aes_update_block(&ctx->tweak, tweak, 16, ctx->T);
	ctx->armed = YES;
	return(TRUE);
}

/**
 * @brief Encrypt one data unit
 *
 * @param[in] ctx Started context, with a tweak
 * @param[in] in Plaintext, at least 16 bytes
 * @param[in] len Byte length
 * @param[out] out Ciphertext, len bytes
 * @return len, or 0 when the unit is shorter than a block
 */
unsigned int mp_crypto_xts_update(mp_crypto_xts_t *ctx, const void *in, unsigned int len, uint8_t *out) {
	return(_unit(ctx, in, len, out, 1));
}

/**
 * @brief Decrypt one data unit
 *
 * @param[in] ctx Started context, with a tweak
 * @param[in] in Ciphertext, at least 16 bytes
 * @param[in] len Byte length
 * @param[out] out Plaintext, len bytes
 * @return len, or 0 when the unit is shorter than a block
 */
unsigned int mp_crypto_xts_update_inv(mp_crypto_xts_t *ctx, const void *in, unsigned int len, uint8_t *out) {
	return(_unit(ctx, in, len, out, 0));
}

/**
 * @brief Drop an XTS context
 *
 * @param[in] ctx Started context, or NULL
 */
void mp_crypto_xts_free(mp_crypto_xts_t *ctx) {
	if(!ctx)
		return;
	mp_crypto_aes_free(&ctx->data);
	mp_crypto_aes_free(&ctx->tweak);
	memset(ctx, 0, sizeof(*ctx));
}

#if defined(MP_ARCH_XTS) && defined(SUPPORT_COMMON_CRYPTO_FUSE)

/**
 * @brief Encrypt one flash line
 *
 * @param[in] fuse Slot of role MP_CRYPTO_FUSE_XTS
 * @param[in] address External flash address, aligned to len
 * @param[in] in Plain line
 * @param[in] len 16, 32 or 64
 * @param[out] out Cipher line
 * @return len, or 0 when the slot or the block refuses
 */
unsigned int mp_crypto_xts_crypt(const mp_crypto_fuse_t *fuse, unsigned long address, const void *in, unsigned int len, void *out) {
	if(mp_crypto_fuse_check(fuse, MP_CRYPTO_FUSE_XTS) != YES)
		return(0);
	return(mp_arch_xts_crypt(fuse->slot, address, in, len, out));
}

#endif

/**@}*/

#endif
