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

#ifdef SUPPORT_COMMON_CRYPTO_MODE_CBC

#include <string.h>

/**
@defgroup mpCommonModeCbc CBC

@ingroup mpCommon

@brief Cipher block chaining, raw block mode

The chain value starts at the IV. Each plaintext block is XORed with the
chain before encryption, and each ciphertext block becomes the next chain.
Decryption is the inverse: the block is decrypted, then XORed with the
previous ciphertext block.

This is a raw block mode: it carries no padding. The caller must feed
block-aligned data; a partial block left at mp_cipher_mode_final() is
dropped (the mode returns 0). PKCS#7 or any other padding is the caller
job, on top of this mode.

Constant time with respect to the key: the cipher does that, and the mode
only XORs and copies.

@{
*/

static mp_ret_t _mp_cbc_init(mp_cipher_mode_t *m, mp_cipher_t *cipher, const void *iv, unsigned int ivlen, mp_bool_t encrypt) {
	if(!m || !cipher || !iv)
		return(FALSE);
	if(ivlen != MP_MODE_BLOCK_MAX)
		return(FALSE);
	if(!cipher->algo)
		return(FALSE);
	memset(m, 0, sizeof(*m));
	m->cipher = cipher;
	m->encrypt = encrypt;
	memcpy(m->iv, iv, MP_MODE_BLOCK_MAX); /* chain = IV */
	return(TRUE);
}

static unsigned int _mp_cbc_enc(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out) {
	const uint8_t *p = (const uint8_t *)in;
	uint8_t *o = out;
	unsigned int written = 0;
	uint8_t tmp[MP_MODE_BLOCK_MAX];

	while(m->leftover && inlen) {
		m->buf[m->leftover++] = *p++;
		inlen--;
		if(m->leftover == MP_MODE_BLOCK_MAX) {
			for(unsigned int i = 0; i < MP_MODE_BLOCK_MAX; i++)
				tmp[i] = m->buf[i] ^ m->iv[i];
			mp_cipher_update_block(m->cipher, tmp, MP_MODE_BLOCK_MAX, m->iv);
			memcpy(o, m->iv, MP_MODE_BLOCK_MAX);
			o += MP_MODE_BLOCK_MAX;
			written += MP_MODE_BLOCK_MAX;
			m->leftover = 0;
		}
	}
	while(inlen >= MP_MODE_BLOCK_MAX) {
		for(unsigned int i = 0; i < MP_MODE_BLOCK_MAX; i++)
			tmp[i] = p[i] ^ m->iv[i];
		mp_cipher_update_block(m->cipher, tmp, MP_MODE_BLOCK_MAX, m->iv);
		memcpy(o, m->iv, MP_MODE_BLOCK_MAX);
		p += MP_MODE_BLOCK_MAX;
		o += MP_MODE_BLOCK_MAX;
		inlen -= MP_MODE_BLOCK_MAX;
		written += MP_MODE_BLOCK_MAX;
	}
	while(inlen--)
		m->buf[m->leftover++] = *p++;

	memset(tmp, 0, sizeof(tmp));
	return(written);
}

static unsigned int _mp_cbc_dec(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out) {
	const uint8_t *p = (const uint8_t *)in;
	uint8_t *o = out;
	unsigned int written = 0;
	uint8_t tmp[MP_MODE_BLOCK_MAX];
	uint8_t chain[MP_MODE_BLOCK_MAX];

	while(m->leftover && inlen) {
		m->buf[m->leftover++] = *p++;
		inlen--;
		if(m->leftover == MP_MODE_BLOCK_MAX) {
			memcpy(chain, m->iv, MP_MODE_BLOCK_MAX);
			mp_cipher_update_block_inv(m->cipher, m->buf, MP_MODE_BLOCK_MAX, tmp);
			for(unsigned int i = 0; i < MP_MODE_BLOCK_MAX; i++)
				o[i] = tmp[i] ^ chain[i];
			memcpy(m->iv, m->buf, MP_MODE_BLOCK_MAX);
			o += MP_MODE_BLOCK_MAX;
			written += MP_MODE_BLOCK_MAX;
			m->leftover = 0;
		}
	}
	while(inlen >= MP_MODE_BLOCK_MAX) {
		memcpy(chain, m->iv, MP_MODE_BLOCK_MAX);
		mp_cipher_update_block_inv(m->cipher, p, MP_MODE_BLOCK_MAX, tmp);
		for(unsigned int i = 0; i < MP_MODE_BLOCK_MAX; i++)
			o[i] = tmp[i] ^ chain[i];
		memcpy(m->iv, p, MP_MODE_BLOCK_MAX);
		p += MP_MODE_BLOCK_MAX;
		o += MP_MODE_BLOCK_MAX;
		inlen -= MP_MODE_BLOCK_MAX;
		written += MP_MODE_BLOCK_MAX;
	}
	while(inlen--)
		m->buf[m->leftover++] = *p++;

	memset(tmp, 0, sizeof(tmp));
	memset(chain, 0, sizeof(chain));
	return(written);
}

static unsigned int _mp_cbc_update(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out) {
	if(!m || !m->cipher || !m->cipher->algo || (!in && inlen))
		return(0);
	return(m->encrypt ? _mp_cbc_enc(m, in, inlen, out) : _mp_cbc_dec(m, in, inlen, out));
}

static unsigned int _mp_cbc_update_aad(mp_cipher_mode_t *m, const void *aad, unsigned int len) {
	(void)m; (void)aad; (void)len;
	return(0); /* CBC is not AEAD */
}

static unsigned int _mp_cbc_final(mp_cipher_mode_t *m, uint8_t *out, uint8_t *tag) {
	(void)out; (void)tag;
	/* raw mode: a partial block cannot be emitted */
	if(m->leftover)
		m->leftover = 0;
	return(0);
}

static void _mp_cbc_free(mp_cipher_mode_t *m) {
	if(!m)
		return;
	memset(m, 0, sizeof(*m));
}

const mp_cipher_mode_algo_t mp_cipher_mode_cbc_algo = {
	"CBC", "CBC",
	MP_MODE_BLOCK_MAX, 0,
	_mp_cbc_init,
	_mp_cbc_update,
	_mp_cbc_update_aad,
	_mp_cbc_final,
	_mp_cbc_free,
};

/**@}*/

#endif
