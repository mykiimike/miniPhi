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

#ifdef SUPPORT_COMMON_CRYPTO_MODE_GCM

#include <string.h>

/**
@defgroup mpCommonModeGcm GCM

@ingroup mpCommon

@brief Galois/Counter Mode, an AEAD over a 128-bit block cipher

The IV is 12 bytes: J0 = IV || 0x00000001. The hash subkey H is the
encryption of the zero block. The ciphertext is CTR (inc32 on the counter)
and the tag is GHASH over the AAD and the ciphertext, encrypted with J0.

Constant time with respect to the key: H is the only secret. It is passed
to the GF(2^128) multiply as the shifted operand, and the running GHASH
accumulator (the other operand, which transitively depends on H) is read
bit by bit; both reads go through masks, not branches, and the reduction
is a mask too, so no branch, table index or address depends on H.

@{
*/

/* X * Y in GF(2^128) with the NIST bit order; V starts as Y, bits of X are
 * tested with masks (constant time with respect to both operands) */
static void _mp_gmul(const uint8_t *X, const uint8_t *Y, uint8_t *Z) {
	uint8_t V[16], z[16];
	memcpy(V, Y, 16);
	memset(z, 0, 16);
	for(int i = 0; i < 128; i++) {
		uint8_t bit = (uint8_t)((X[i / 8] >> (7 - (i & 7))) & 1);
		uint8_t mask = (uint8_t)(0 - bit);
		for(int b = 0; b < 16; b++)
			z[b] ^= (uint8_t)(V[b] & mask);
		uint8_t lsb = (uint8_t)(V[15] & 1);
		for(int b = 15; b > 0; b--)
			V[b] = (uint8_t)((V[b] >> 1) | (V[b - 1] << 7));
		V[0] >>= 1;
		V[0] ^= (uint8_t)(0xE1 & (0 - lsb));
	}
	memcpy(Z, z, 16);
	memset(V, 0, sizeof(V));
	memset(z, 0, sizeof(z));
}

/* S = (S XOR B) * H */
static void _mp_ghash_block(mp_cipher_mode_t *m, const uint8_t *B) {
	for(unsigned int i = 0; i < MP_MODE_BLOCK_MAX; i++)
		m->auth[i] ^= B[i];
	_mp_gmul(m->auth, m->tag, m->auth);
}

/* increment the rightmost 32 bits of the counter (big-endian) */
static void _mp_gcm_inc32(uint8_t *c) {
	for(int i = 15; i >= 12; i--) {
		if(++c[i])
			break;
	}
}

static mp_ret_t _mp_gcm_init(mp_cipher_mode_t *m, mp_cipher_t *cipher, const void *iv, unsigned int ivlen, mp_bool_t encrypt) {
	if(!m || !cipher || !iv)
		return(FALSE);
	if(ivlen != 12) /* the 96-bit IV case, the only one this mode takes */
		return(FALSE);
	if(!cipher->algo)
		return(FALSE);
	memset(m, 0, sizeof(*m));
	m->cipher = cipher;
	m->encrypt = encrypt;
	m->phase = 0; /* AAD */

	/* H = E(0) */
	memset(m->tag, 0, MP_MODE_BLOCK_MAX);
	mp_cipher_update_block(cipher, m->tag, MP_MODE_BLOCK_MAX, m->tag);

	/* J0 = IV || 0x00000001 */
	uint8_t j0[MP_MODE_BLOCK_MAX];
	memcpy(j0, iv, 12);
	j0[12] = 0; j0[13] = 0; j0[14] = 0; j0[15] = 1;
	/* tag mask = E(J0) */
	mp_cipher_update_block(cipher, j0, MP_MODE_BLOCK_MAX, m->j0);
	/* working counter = inc32(J0) */
	memcpy(m->iv, j0, MP_MODE_BLOCK_MAX);
	_mp_gcm_inc32(m->iv);
	memset(j0, 0, sizeof(j0));
	return(TRUE);
}

static unsigned int _mp_gcm_update_aad(mp_cipher_mode_t *m, const void *aad, unsigned int len) {
	if(!m || m->phase != 0 || (!aad && len))
		return(0);
	const uint8_t *p = (const uint8_t *)aad;
	unsigned int n = len;
	while(n--) {
		m->buf[m->leftover++] = *p++;
		if(m->leftover == MP_MODE_BLOCK_MAX) {
			_mp_ghash_block(m, m->buf);
			m->leftover = 0;
		}
	}
	m->aadLen += (unsigned long)len * 8;
	return(len);
}

static unsigned int _mp_gcm_update(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out) {
	if(!m || !m->cipher || !m->cipher->algo || (!in && inlen))
		return(0);
	/* first data: close the AAD by padding its partial block */
	if(m->phase == 0) {
		if(m->leftover) {
			while(m->leftover < MP_MODE_BLOCK_MAX)
				m->buf[m->leftover++] = 0;
			_mp_ghash_block(m, m->buf);
			m->leftover = 0;
		}
		m->phase = 1;
	}
	const uint8_t *p = (const uint8_t *)in;
	uint8_t *o = out;
	unsigned int written = 0;
	unsigned int k = MP_MODE_BLOCK_MAX - m->ksLeft; /* next keystream byte */

	while(inlen--) {
		if(m->ksLeft == 0) {
			mp_cipher_update_block(m->cipher, m->iv, MP_MODE_BLOCK_MAX, m->ks);
			_mp_gcm_inc32(m->iv);
			m->ksLeft = MP_MODE_BLOCK_MAX;
			k = 0;
		}
		uint8_t inb = *p++;
		uint8_t outb = (uint8_t)(inb ^ m->ks[k++]);
		*o++ = outb;
		/* GHASH absorbs the ciphertext: for encrypt it is the output, for
		 * decrypt it is the input we just read */
		uint8_t ct = m->encrypt ? outb : inb;
		m->buf[m->leftover++] = ct;
		if(m->leftover == MP_MODE_BLOCK_MAX) {
			_mp_ghash_block(m, m->buf);
			m->leftover = 0;
		}
		m->ksLeft--;
		written++;
	}
	m->ctrLen += (unsigned long)written * 8;
	return(written);
}

static unsigned int _mp_gcm_final(mp_cipher_mode_t *m, uint8_t *out, uint8_t *tag) {
	if(!m)
		return(0);
	(void)out;
	/* if we never left the AAD phase, close it now */
	if(m->phase == 0) {
		if(m->leftover) {
			while(m->leftover < MP_MODE_BLOCK_MAX)
				m->buf[m->leftover++] = 0;
			_mp_ghash_block(m, m->buf);
			m->leftover = 0;
		}
		m->phase = 1;
	}
	/* pad the last ciphertext block */
	if(m->leftover) {
		while(m->leftover < MP_MODE_BLOCK_MAX)
			m->buf[m->leftover++] = 0;
		_mp_ghash_block(m, m->buf);
		m->leftover = 0;
	}
	/* length block: [aadLen]_64 || [ctrLen]_64 */
	uint8_t lb[MP_MODE_BLOCK_MAX];
	for(int i = 7; i >= 0; i--)
		lb[7 - i] = (uint8_t)(m->aadLen >> (i * 8));
	for(int i = 7; i >= 0; i--)
		lb[15 - i] = (uint8_t)(m->ctrLen >> (i * 8));
	_mp_ghash_block(m, lb);
	memset(lb, 0, sizeof(lb));
	/* tag = E(J0) XOR S */
	if(tag) {
		for(unsigned int i = 0; i < MP_MODE_BLOCK_MAX; i++)
			tag[i] = (uint8_t)(m->j0[i] ^ m->auth[i]);
	}
	m->phase = 2;
	return(0);
}

static void _mp_gcm_free(mp_cipher_mode_t *m) {
	if(!m)
		return;
	memset(m, 0, sizeof(*m));
}

const mp_cipher_mode_algo_t mp_cipher_mode_gcm_algo = {
	"GCM", "GCM",
	12, MP_MODE_BLOCK_MAX,
	_mp_gcm_init,
	_mp_gcm_update,
	_mp_gcm_update_aad,
	_mp_gcm_final,
	_mp_gcm_free,
};

/**@}*/

#endif



