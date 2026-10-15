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

#ifdef SUPPORT_COMMON_CRYPTO_MODE_CTR

#include <string.h>

/**
@defgroup mpCommonModeCtr CTR

@ingroup mpCommon

@brief Counter mode, a stream over a block cipher

The IV is the initial 128-bit counter. Each keystream block is the
encryption of the counter, then the counter is incremented as a big-endian
128-bit integer. The keystream is XORed with the data, so encryption and
decryption are the same operation.

Constant time with respect to the key: the cipher does that, and the mode
only XORs, copies and increments.

@{
*/

static void _mp_ctr_inc(uint8_t *c) {
	for(int i = MP_MODE_BLOCK_MAX - 1; i >= 0; i--) {
		if(++c[i])
			break;
	}
}

static mp_ret_t _mp_ctr_init(mp_cipher_mode_t *m, mp_cipher_t *cipher, const void *iv, unsigned int ivlen, mp_bool_t encrypt) {
	if(!m || !cipher || !iv)
		return(FALSE);
	if(ivlen != MP_MODE_BLOCK_MAX)
		return(FALSE);
	if(!cipher->algo)
		return(FALSE);
	(void)encrypt; /* CTR is symmetric */
	memset(m, 0, sizeof(*m));
	m->cipher = cipher;
	memcpy(m->iv, iv, MP_MODE_BLOCK_MAX); /* counter */
	mp_cipher_update_block(m->cipher, m->iv, MP_MODE_BLOCK_MAX, m->auth); /* first keystream */
	_mp_ctr_inc(m->iv);
	m->leftover = MP_MODE_BLOCK_MAX; /* keystream bytes left in auth */
	return(TRUE);
}

static unsigned int _mp_ctr_update(mp_cipher_mode_t *m, const void *in, unsigned int inlen, uint8_t *out) {
	if(!m || !m->cipher || !m->cipher->algo || (!in && inlen))
		return(0);
	const uint8_t *p = (const uint8_t *)in;
	uint8_t *o = out;
	unsigned int written = 0;
	unsigned int k = MP_MODE_BLOCK_MAX - m->leftover; /* index of next keystream byte */

	while(inlen--) {
		if(m->leftover == 0) {
			mp_cipher_update_block(m->cipher, m->iv, MP_MODE_BLOCK_MAX, m->auth);
			_mp_ctr_inc(m->iv);
			m->leftover = MP_MODE_BLOCK_MAX;
			k = 0;
		}
		*o++ = *p++ ^ m->auth[k++];
		m->leftover--;
		written++;
	}
	return(written);
}

static unsigned int _mp_ctr_update_aad(mp_cipher_mode_t *m, const void *aad, unsigned int len) {
	(void)m; (void)aad; (void)len;
	return(0); /* CTR is not AEAD */
}

static unsigned int _mp_ctr_final(mp_cipher_mode_t *m, uint8_t *out, uint8_t *tag) {
	(void)out; (void)tag;
	/* the keystream tail is dropped, never reused with a new IV */
	m->leftover = 0;
	return(0);
}

static void _mp_ctr_free(mp_cipher_mode_t *m) {
	if(!m)
		return;
	memset(m, 0, sizeof(*m));
}

const mp_cipher_mode_algo_t mp_cipher_mode_ctr_algo = {
	"CTR", "CTR",
	MP_MODE_BLOCK_MAX, 0,
	_mp_ctr_init,
	_mp_ctr_update,
	_mp_ctr_update_aad,
	_mp_ctr_final,
	_mp_ctr_free,
};

/**@}*/

#endif
