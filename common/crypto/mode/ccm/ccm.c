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

#ifdef SUPPORT_COMMON_CRYPTO_MODE_CCM

#include "ccm.h"
#include <string.h>

/**
@defgroup mpCommonCcm CCM

@ingroup mpCommon

@brief AES-CCM AEAD (RFC 3610 / NIST SP 800-38C)

CCM = CBC-MAC over (B0 || aad-format || plaintext) for the tag, then CTR
over the plaintext for the ciphertext, the tag finally XORed with
AES(CTR_0). Only the AES forward cipher is used. Constant time: the tag
compare reads every byte; the only secret that touches a branch is the
tag-mismatch flag.

@{
*/

#define _CCM_BLOCK 16

static void _ccm_put_be(uint8_t *p, uint64_t v, unsigned int n) {
	for(unsigned int i = 0; i < n; i++)
		p[i] = (uint8_t)(v >> (8 * (n - 1 - i)));
}

/* format a CTR block: flags | nonce | counter (L bytes big-endian) */
static void _ccm_ctr_block(uint8_t *b, const uint8_t *nonce, unsigned int nlen, unsigned int L, unsigned int counter) {
	b[0] = (uint8_t)(L - 1);
	memcpy(b + 1, nonce, nlen);
	_ccm_put_be(b + 1 + nlen, counter, L);
}

/* CBC-MAC: process one 16-byte block (X = AES(X ^ block)) */
static void _ccm_mac_block(mp_cipher_t *aes, uint8_t *X, const uint8_t *block) {
	uint8_t t[_CCM_BLOCK];
	for(int i = 0; i < _CCM_BLOCK; i++)
		t[i] = X[i] ^ block[i];
	mp_cipher_update_block(aes, t, _CCM_BLOCK, X);
	memset(t, 0, sizeof(t));
}

/* run the CCM authentication over (B0 || aad-fmt || plaintext), write S */
static void _ccm_mac(mp_cipher_t *aes,
		const uint8_t *nonce, unsigned int nlen,
		const void *aad, unsigned int aadlen,
		const void *pt, unsigned int ptlen,
		unsigned int L, unsigned int T, uint8_t *S) {
	uint8_t X[_CCM_BLOCK];
	uint8_t blk[_CCM_BLOCK];
	const uint8_t *a = (const uint8_t *)aad;
	const uint8_t *p = (const uint8_t *)pt;
	unsigned int aoff = 0, poff = 0;

	/* B0: flags | nonce | ptlen */
	memset(blk, 0, _CCM_BLOCK);
	blk[0] = (uint8_t)(((aadlen ? 0x40 : 0) | (((T - 2) / 2) << 3) | (L - 1)));
	memcpy(blk + 1, nonce, nlen);
	_ccm_put_be(blk + 1 + nlen, ptlen, L);
	mp_cipher_update_block(aes, blk, _CCM_BLOCK, X);

	/* aad: length prefix then aad, padded to 16 */
	if(aadlen) {
		memset(blk, 0, _CCM_BLOCK);
		blk[0] = (uint8_t)(aadlen >> 8);
		blk[1] = (uint8_t)aadlen;
		unsigned int take = aadlen < 14 ? aadlen : 14;
		memcpy(blk + 2, a, take);
		_ccm_mac_block(aes, X, blk);
		aoff = take;
		while(aoff < aadlen) {
			unsigned int want = aadlen - aoff < _CCM_BLOCK ? aadlen - aoff : _CCM_BLOCK;
			memset(blk, 0, _CCM_BLOCK);
			memcpy(blk, a + aoff, want);
			_ccm_mac_block(aes, X, blk);
			aoff += want;
		}
	}

	/* plaintext, padded to 16 */
	while(poff < ptlen) {
		unsigned int want = ptlen - poff < _CCM_BLOCK ? ptlen - poff : _CCM_BLOCK;
		memset(blk, 0, _CCM_BLOCK);
		memcpy(blk, p + poff, want);
		_ccm_mac_block(aes, X, blk);
		poff += want;
	}

	memcpy(S, X, T);
	memset(X, 0, sizeof(X));
	memset(blk, 0, sizeof(blk));
}

/* CTR: XOR in[start..] with AES(CTR_start+i), write out */
static void _ccm_ctr(mp_cipher_t *aes,
		const uint8_t *nonce, unsigned int nlen, unsigned int L,
		const void *in, unsigned int inlen, uint8_t *out, unsigned int start) {
	const uint8_t *p = (const uint8_t *)in;
	uint8_t *o = out;
	uint8_t A[_CCM_BLOCK], S[_CCM_BLOCK];
	unsigned int off = 0, counter = start;
	while(off < inlen) {
		_ccm_ctr_block(A, nonce, nlen, L, counter++);
		mp_cipher_update_block(aes, A, _CCM_BLOCK, S);
		unsigned int want = inlen - off < _CCM_BLOCK ? inlen - off : _CCM_BLOCK;
		for(unsigned int i = 0; i < want; i++)
			o[off + i] = p[off + i] ^ S[i];
		off += want;
	}
	memset(A, 0, sizeof(A));
	memset(S, 0, sizeof(S));
}

/**
 * @brief Encrypt and authenticate
 */
mp_ret_t mp_ccm_seal(mp_cipher_t *aes,
		const void *nonce, unsigned int nlen,
		const void *aad, unsigned int aadlen,
		const void *pt, unsigned int ptlen,
		uint8_t *ct, uint8_t *tag, unsigned int T) {
	if(!aes || !nonce || !tag)
		return(FALSE);
	if(nlen < 7 || nlen > 13)
		return(FALSE);
	if(T != 4 && T != 6 && T != 8 && T != 10 && T != 12 && T != 14 && T != 16)
		return(FALSE);
	if(aadlen >= 0xFF00)
		return(FALSE);
	if((ptlen && !ct) || (ptlen && !pt))
		return(FALSE);
	unsigned int L = 15 - nlen;
	/* ptlen is 32-bit, so for L >= 4 (max >= 2^32) it always fits; only
	 * check the small-L bound, where the shift stays under 32 bits */
	if(L <= 3 && ptlen >= (1u << (8 * L)))
		return(FALSE);

	uint8_t S[_CCM_BLOCK];
	uint8_t A0[_CCM_BLOCK], U[_CCM_BLOCK];

	_ccm_mac(aes, nonce, nlen, aad, aadlen, pt, ptlen, L, T, S);
	if(ptlen)
		_ccm_ctr(aes, nonce, nlen, L, pt, ptlen, ct, 1);

	/* tag = S XOR AES(CTR_0) */
	_ccm_ctr_block(A0, nonce, nlen, L, 0);
	mp_cipher_update_block(aes, A0, _CCM_BLOCK, U);
	for(unsigned int i = 0; i < T; i++)
		tag[i] = S[i] ^ U[i];

	memset(S, 0, sizeof(S));
	memset(A0, 0, sizeof(A0));
	memset(U, 0, sizeof(U));
	return(TRUE);
}

/**
 * @brief Verify and decrypt
 */
mp_ret_t mp_ccm_open(mp_cipher_t *aes,
		const void *nonce, unsigned int nlen,
		const void *aad, unsigned int aadlen,
		const void *ct, unsigned int ctlen,
		const void *tag, unsigned int T,
		uint8_t *pt) {
	if(!aes || !nonce || !tag)
		return(FALSE);
	if(nlen < 7 || nlen > 13)
		return(FALSE);
	if(T != 4 && T != 6 && T != 8 && T != 10 && T != 12 && T != 14 && T != 16)
		return(FALSE);
	if(aadlen >= 0xFF00)
		return(FALSE);
	if((ctlen && !ct) || (ctlen && !pt))
		return(FALSE);
	unsigned int L = 15 - nlen;
	if(L <= 3 && ctlen >= (1u << (8 * L)))
		return(FALSE);

	/* decrypt first */
	if(ctlen)
		_ccm_ctr(aes, nonce, nlen, L, ct, ctlen, pt, 1);

	uint8_t S[_CCM_BLOCK];
	uint8_t A0[_CCM_BLOCK], U[_CCM_BLOCK];
	uint8_t want[MP_CCM_TAG_MAX];

	_ccm_mac(aes, nonce, nlen, aad, aadlen, pt, ctlen, L, T, S);
	_ccm_ctr_block(A0, nonce, nlen, L, 0);
	mp_cipher_update_block(aes, A0, _CCM_BLOCK, U);
	for(unsigned int i = 0; i < T; i++)
		want[i] = S[i] ^ U[i];

	if(mp_mac_equal(want, tag, T) != YES) {
		memset(S, 0, sizeof(S));
		memset(A0, 0, sizeof(A0));
		memset(U, 0, sizeof(U));
		memset(want, 0, sizeof(want));
		if(pt && ctlen)
			memset(pt, 0, ctlen);
		return(FALSE);
	}

	memset(S, 0, sizeof(S));
	memset(A0, 0, sizeof(A0));
	memset(U, 0, sizeof(U));
	memset(want, 0, sizeof(want));
	return(TRUE);
}

/**@}*/

#endif
