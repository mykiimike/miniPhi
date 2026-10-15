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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_COMMON_CRYPTO_SIGN_LMS

/**
@defgroup mpCommonLms LMS

@ingroup mpCommon

@brief Stateful hash-based signatures, RFC 8554, LMS_SHA256_M32_H5

The private key is the 16-byte identifier I and a 32-byte seed; the LMOTS
secrets x[i] and the randomizer C are derived from them by SHA-256, so the
whole Merkle tree is rebuilt on demand. The public key is the root and the
signature is q, the LM-OTS signature and the authentication path.

Constant time with respect to the secret: the seed only ever enters a
SHA-256 input. The Winternitz chain lengths come from the message hash and
the public randomizer C, both of which travel in the signature, so the
number of hash rounds is public. No branch or memory access reads the
secret. Signing is stateful: the caller must persist and advance q, and
must never reuse a leaf. The page only verifies.

@{
*/

#define D_PBLC 0x8080
#define D_MESG 0x8181
#define D_LEAF 0x8282
#define D_INTR 0x8383

static uint8_t _ch[MP_LMS_N];
static uint8_t _y[MP_LMS_N * MP_LMS_P];
static uint8_t _Q[MP_LMS_N + 2];
static uint8_t _cur[MP_LMS_LEAVES][MP_LMS_M];
static uint8_t _nxt[MP_LMS_LEAVES / 2][MP_LMS_M];

static void _u32(uint8_t *o, uint32_t v) {
	o[0] = (uint8_t)(v >> 24);
	o[1] = (uint8_t)(v >> 16);
	o[2] = (uint8_t)(v >> 8);
	o[3] = (uint8_t)v;
}

static void _u16(uint8_t *o, uint16_t v) {
	o[0] = (uint8_t)(v >> 8);
	o[1] = (uint8_t)v;
}

/* x[i] for leaf q, chain i, from the seed (RFC 8554 Appendix A style) */
static void _lmots_x(uint8_t out[MP_LMS_N], const uint8_t I[16], uint32_t q, unsigned int i, const uint8_t seed[32]) {
	uint8_t q4[4], i2[2], one[1];
	mp_sha256_t ctx;

	_u32(q4, q);
	_u16(i2, (uint16_t)i);
	one[0] = 0xff;
	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, I, 16);
	mp_sha256_update(&ctx, q4, 4);
	mp_sha256_update(&ctx, i2, 2);
	mp_sha256_update(&ctx, one, 1);
	mp_sha256_update(&ctx, seed, 32);
	mp_sha256_final(&ctx, out);
}

/* the randomizer C for leaf q from the seed; varies with q */
static void _lmots_c(uint8_t out[MP_LMS_N], const uint8_t I[16], uint32_t q, const uint8_t seed[32]) {
	uint8_t q4[4], tag[2];
	mp_sha256_t ctx;

	_u32(q4, q);
	_u16(tag, 0xfffe);
	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, I, 16);
	mp_sha256_update(&ctx, q4, 4);
	mp_sha256_update(&ctx, tag, 2);
	mp_sha256_update(&ctx, seed, 32);
	mp_sha256_final(&ctx, out);
}

/* out = chain(in, start, count): count rounds of the Winternitz hash */
static void _chain(uint8_t out[MP_LMS_N], const uint8_t in[MP_LMS_N], unsigned int start, unsigned int count, const uint8_t I[16], uint32_t q, unsigned int i) {
	uint8_t q4[4], i2[2], j1[1];
	unsigned int k;
	mp_sha256_t ctx;

	memcpy(_ch, in, MP_LMS_N);
	_u32(q4, q);
	_u16(i2, (uint16_t)i);
	for(k = 0; k < count; k++) {
		j1[0] = (uint8_t)(start + k);
		mp_sha256_init(&ctx);
		mp_sha256_update(&ctx, I, 16);
		mp_sha256_update(&ctx, q4, 4);
		mp_sha256_update(&ctx, i2, 2);
		mp_sha256_update(&ctx, j1, 1);
		mp_sha256_update(&ctx, _ch, MP_LMS_N);
		mp_sha256_final(&ctx, _ch);
	}
	memcpy(out, _ch, MP_LMS_N);
}

/* Q = H(I || u32str(q) || u16str(D_MESG) || C || message); _Q holds Q||Cksm */
static void _msg_hash(const uint8_t I[16], uint32_t q, const uint8_t C[MP_LMS_N], const void *msg, unsigned int len) {
	uint8_t q4[4], d[2];
	mp_sha256_t ctx;

	_u32(q4, q);
	_u16(d, D_MESG);
	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, I, 16);
	mp_sha256_update(&ctx, q4, 4);
	mp_sha256_update(&ctx, d, 2);
	mp_sha256_update(&ctx, C, MP_LMS_N);
	if(len)
		mp_sha256_update(&ctx, msg, len);
	mp_sha256_final(&ctx, _Q);
}

/* checksum: sum of (255 - byte) over Q, big-endian into _Q[32..33] */
static void _cksm(void) {
	unsigned int i;
	uint32_t sum = 0;

	for(i = 0; i < MP_LMS_N; i++)
		sum += 0xff - _Q[i];
	_Q[MP_LMS_N] = (uint8_t)(sum >> 8);
	_Q[MP_LMS_N + 1] = (uint8_t)sum;
}

/* K = H(I || u32str(q) || u16str(D_PBLC) || y[0]..y[p-1]); y held in _y */
static void _pblc(uint8_t out[MP_LMS_N], const uint8_t I[16], uint32_t q) {
	uint8_t q4[4], d[2];
	mp_sha256_t ctx;

	_u32(q4, q);
	_u16(d, D_PBLC);
	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, I, 16);
	mp_sha256_update(&ctx, q4, 4);
	mp_sha256_update(&ctx, d, 2);
	mp_sha256_update(&ctx, _y, MP_LMS_N * MP_LMS_P);
	mp_sha256_final(&ctx, out);
}

static void _leaf(uint8_t out[MP_LMS_M], const uint8_t I[16], uint32_t r, const uint8_t K[MP_LMS_N]) {
	uint8_t r4[4], d[2];
	mp_sha256_t ctx;

	_u32(r4, r);
	_u16(d, D_LEAF);
	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, I, 16);
	mp_sha256_update(&ctx, r4, 4);
	mp_sha256_update(&ctx, d, 2);
	mp_sha256_update(&ctx, K, MP_LMS_N);
	mp_sha256_final(&ctx, out);
}

static void _intr(uint8_t out[MP_LMS_M], const uint8_t I[16], uint32_t r, const uint8_t a[MP_LMS_M], const uint8_t b[MP_LMS_M]) {
	uint8_t r4[4], d[2];
	mp_sha256_t ctx;

	_u32(r4, r);
	_u16(d, D_INTR);
	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, I, 16);
	mp_sha256_update(&ctx, r4, 4);
	mp_sha256_update(&ctx, d, 2);
	mp_sha256_update(&ctx, a, MP_LMS_M);
	mp_sha256_update(&ctx, b, MP_LMS_M);
	mp_sha256_final(&ctx, out);
}

/* LMOTS public key K for leaf q: chain every x[i] 255 times, then D_PBLC */
static void _lmots_pub(uint8_t K[MP_LMS_N], const uint8_t I[16], uint32_t q, const uint8_t seed[32]) {
	uint8_t x[MP_LMS_N];
	unsigned int i;

	for(i = 0; i < MP_LMS_P; i++) {
		_lmots_x(x, I, q, i, seed);
		_chain(_y + i * MP_LMS_N, x, 0, 0xff, I, q, i);
	}
	_pblc(K, I, q);
	memset(x, 0, sizeof(x));
}

static void _wipe(void) {
	memset(_ch, 0, sizeof(_ch));
	memset(_y, 0, sizeof(_y));
	memset(_Q, 0, sizeof(_Q));
	memset(_cur, 0, sizeof(_cur));
	memset(_nxt, 0, sizeof(_nxt));
}

mp_ret_t mp_lms_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	mp_lms_priv_t *sk = priv;
	uint8_t *pk = pub;
	uint8_t K[MP_LMS_N];
	unsigned int q, level, n, k;
	uint32_t r;

	if(!seed || !priv || !pub || seedlen < 32)
		return(FALSE);
	memcpy(sk->seed, seed, 32);
	memcpy(sk->I, seed, 16);
	sk->q = 0;
	for(q = 0; q < MP_LMS_LEAVES; q++) {
		_lmots_pub(K, sk->I, q, sk->seed);
		_leaf(_cur[q], sk->I, (uint32_t)MP_LMS_LEAVES + q, K);
	}
	n = MP_LMS_LEAVES;
	level = 0;
	while(n > 1) {
		for(k = 0; k < n / 2; k++) {
			r = (1u << (MP_LMS_H - level - 1)) + k;
			_intr(_nxt[k], sk->I, r, _cur[2 * k], _cur[2 * k + 1]);
		}
		memcpy(_cur, _nxt, (n / 2) * MP_LMS_M);
		n /= 2;
		level++;
	}
	_u32(pk, MP_LMS_TYPE);
	_u32(pk + 4, MP_LMS_OTS_TYPE);
	memcpy(pk + 8, sk->I, 16);
	memcpy(pk + 24, _cur[0], MP_LMS_M);
	_wipe();
	memset(K, 0, sizeof(K));
	return(TRUE);
}

unsigned int mp_lms_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig) {
	const mp_lms_priv_t *sk = priv;
	uint8_t *s = sig;
	uint8_t C[MP_LMS_N], x[MP_LMS_N], K[MP_LMS_N];
	uint32_t q, node, base;
	unsigned int i, p, cnt, k;
	uint32_t r;

	if(!priv || !sig || (!msg && len) || sk->q >= MP_LMS_LEAVES)
		return(0);
	q = sk->q;
	_u32(s, q);
	_u32(s + 4, MP_LMS_OTS_TYPE);
	_lmots_c(C, sk->I, q, sk->seed);
	memcpy(s + 8, C, MP_LMS_N);
	_msg_hash(sk->I, q, C, msg, len);
	_cksm();
	for(i = 0; i < MP_LMS_P; i++) {
		_lmots_x(x, sk->I, q, i, sk->seed);
		_chain(s + 8 + MP_LMS_N + i * MP_LMS_N, x, 0, _Q[i], sk->I, q, i);
	}
	/* authentication path: rebuild each level, take the sibling of the path */
	for(i = 0; i < MP_LMS_LEAVES; i++) {
		_lmots_pub(K, sk->I, i, sk->seed);
		_leaf(_cur[i], sk->I, (uint32_t)MP_LMS_LEAVES + i, K);
	}
	node = (uint32_t)MP_LMS_LEAVES + q;
	base = (uint32_t)MP_LMS_LEAVES;
	cnt = MP_LMS_LEAVES;
	for(p = 0; p < MP_LMS_H; p++) {
		k = (unsigned int)(node - base) ^ 1u;
		memcpy(s + 8 + MP_LMS_N + MP_LMS_P * MP_LMS_N + 4 + p * MP_LMS_M,
			_cur[k], MP_LMS_M);
		for(k = 0; k < cnt / 2; k++) {
			r = base / 2 + k;
			_intr(_nxt[k], sk->I, r, _cur[2 * k], _cur[2 * k + 1]);
		}
		memcpy(_cur, _nxt, (cnt / 2) * MP_LMS_M);
		cnt /= 2;
		base /= 2;
		node /= 2;
	}
	_u32(s + 8 + MP_LMS_N + MP_LMS_P * MP_LMS_N, MP_LMS_TYPE);
	/* stateful: the caller advances sk->q after using the returned signature */
	_wipe();
	memset(C, 0, sizeof(C));
	memset(x, 0, sizeof(x));
	memset(K, 0, sizeof(K));
	return(MP_LMS_SIG);
}

mp_bool_t mp_lms_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen) {
	const uint8_t *pk = pub;
	const uint8_t *s = sig;
	uint8_t I[16], Kc[MP_LMS_N], tmp[MP_LMS_M], T1[MP_LMS_M];
	uint32_t q, node;
	unsigned int i, p;
	mp_bool_t ok = NO;

	if(!pub || !sig || siglen != MP_LMS_SIG || (!msg && len))
		return(NO);
	if(((uint32_t)pk[0] << 24 | (uint32_t)pk[1] << 16 | (uint32_t)pk[2] << 8 | pk[3]) != MP_LMS_TYPE)
		return(NO);
	if(((uint32_t)pk[4] << 24 | (uint32_t)pk[5] << 16 | (uint32_t)pk[6] << 8 | pk[7]) != MP_LMS_OTS_TYPE)
		return(NO);
	memcpy(I, pk + 8, 16);
	memcpy(T1, pk + 24, MP_LMS_M);
	q = (uint32_t)s[0] << 24 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 8 | s[3];
	if(((uint32_t)s[4] << 24 | (uint32_t)s[5] << 16 | (uint32_t)s[6] << 8 | s[7]) != MP_LMS_OTS_TYPE)
		return(NO);
	if(q >= MP_LMS_LEAVES)
		return(NO);
	if(((uint32_t)s[8 + MP_LMS_N + MP_LMS_P * MP_LMS_N] << 24 |
	    (uint32_t)s[9 + MP_LMS_N + MP_LMS_P * MP_LMS_N] << 16 |
	    (uint32_t)s[10 + MP_LMS_N + MP_LMS_P * MP_LMS_N] << 8 |
	    s[11 + MP_LMS_N + MP_LMS_P * MP_LMS_N]) != MP_LMS_TYPE)
		return(NO);
	/* LMOTS candidate public key Kc */
	_msg_hash(I, q, s + 8, msg, len);
	_cksm();
	for(i = 0; i < MP_LMS_P; i++)
		_chain(_y + i * MP_LMS_N, s + 8 + MP_LMS_N + i * MP_LMS_N, _Q[i], 0xff - _Q[i], I, q, i);
	_pblc(Kc, I, q);
	/* walk the authentication path up to the candidate root */
	_leaf(tmp, I, (uint32_t)MP_LMS_LEAVES + q, Kc);
	node = (uint32_t)MP_LMS_LEAVES + q;
	p = 0;
	while(node > 1) {
		const uint8_t *path = s + 8 + MP_LMS_N + MP_LMS_P * MP_LMS_N + 4 + p * MP_LMS_M;
		if(node & 1u)
			_intr(tmp, I, node / 2, path, tmp);
		else
			_intr(tmp, I, node / 2, tmp, path);
		node /= 2;
		p++;
	}
	ok = (memcmp(tmp, T1, MP_LMS_M) == 0) ? YES : NO;
	_wipe();
	memset(I, 0, sizeof(I));
	memset(Kc, 0, sizeof(Kc));
	memset(tmp, 0, sizeof(tmp));
	memset(T1, 0, sizeof(T1));
	return(ok);
}

/**@}*/

#endif
