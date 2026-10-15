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

#ifdef SUPPORT_COMMON_CRYPTO_SIGN_SLH

/**
@defgroup mpCommonSlh SLH-DSA

@ingroup mpCommon

@brief Stateless hash-based post-quantum signatures, FIPS 205, SLH-DSA-SHA2-128s

Category 1 (n=16): n=16, h=63, d=7, h'=9, a=12, k=14, lg_w=4, m=30. The
hash family is SHA-2 and, at n=16, every tweakable hash call is SHA-256
(F, H, T_l, PRF), H_msg is MGF1-SHA-256 and PRF_msg is HMAC-SHA-256, so
this needs SHA-256 and HMAC but not SHAKE or SHA-512. The private key is
64 bytes (SK.seed, SK.prf, PK.seed, PK.root), the public key is 32 bytes
(PK.seed, PK.root) and the signature is 7856 bytes.

Constant time with respect to the secret: SK.seed and SK.prf only ever
enter a SHA-256 or HMAC input. No branch, table index or memory address is
ever derived from a secret. The message and signature are public, so
data-dependent timing on them is acceptable. The page only verifies a
stored signature; signing is slow on this class of CPU. One operation at
a time.

@{
*/

#include "slh.h"
#include <string.h>

/* ---- ADRS (32 bytes, 8 big-endian words) -------------------------------- *
 * word0: layer address, word1: (unused), word2-3: tree address,
 * word4: type, word5: keypair address, word6: chain/treeheight,
 * word7: hash address / tree index. */

typedef struct {
	uint8_t b[32];
} mp_slh_adrs_t;

static void adrs_set_u32(uint8_t *w, uint32_t v) {
	w[0] = (uint8_t)(v >> 24);
	w[1] = (uint8_t)(v >> 16);
	w[2] = (uint8_t)(v >> 8);
	w[3] = (uint8_t)(v);
}

static uint32_t adrs_get_u32(const uint8_t *w) {
	return ((uint32_t)w[0] << 24) | ((uint32_t)w[1] << 16) |
	       ((uint32_t)w[2] << 8) | (uint32_t)w[3];
}

static void adrs_zero(mp_slh_adrs_t *a) { memset(a->b, 0, 32); }
static void adrs_set_layer(mp_slh_adrs_t *a, uint32_t l) { adrs_set_u32(a->b + 0, l); }
static void adrs_set_tree(mp_slh_adrs_t *a, uint64_t t) {
	/* tree address is 64-bit, words 2..3 */
	adrs_set_u32(a->b + 8, (uint32_t)(t >> 32));
	adrs_set_u32(a->b + 12, (uint32_t)t);
}
static void adrs_set_type(mp_slh_adrs_t *a, uint32_t t) { adrs_set_u32(a->b + 16, t); }
/* set type and clear the final 12 bytes (keypair/chain/hash) */
static void adrs_set_type_clear(mp_slh_adrs_t *a, uint32_t t) {
	adrs_set_u32(a->b + 16, t);
	memset(a->b + 20, 0, 12);
}
static void adrs_set_keypair(mp_slh_adrs_t *a, uint32_t k) { adrs_set_u32(a->b + 20, k); }
static void adrs_set_chain(mp_slh_adrs_t *a, uint32_t c) { adrs_set_u32(a->b + 24, c); }
static void adrs_set_treeheight(mp_slh_adrs_t *a, uint32_t h) { adrs_set_u32(a->b + 24, h); }
static void adrs_set_hash(mp_slh_adrs_t *a, uint32_t h) { adrs_set_u32(a->b + 28, h); }
static void adrs_set_treeindex(mp_slh_adrs_t *a, uint32_t i) { adrs_set_u32(a->b + 28, i); }
static uint32_t adrs_get_keypair(const mp_slh_adrs_t *a) { return adrs_get_u32(a->b + 20); }
static uint32_t adrs_get_treeindex(const mp_slh_adrs_t *a) { return adrs_get_u32(a->b + 28); }

/* ---- base_2b: read `total` values of `bits` bits each from `in` --------- */
static void slh_base_2b(const uint8_t *in, unsigned bits, uint32_t *out, unsigned total) {
	unsigned inbit = 0;
	unsigned i;
	for (i = 0; i < total; i++) {
		uint32_t v = 0;
		unsigned j;
		for (j = 0; j < bits; j++) {
			unsigned byte = inbit >> 3;
			unsigned bit = 7 - (inbit & 7);
			v = (v << 1) | ((in[byte] >> bit) & 1);
			inbit++;
		}
		out[i] = v;
	}
}

/* ---- thash (category 1, n=16): Trunc16(SHA-256(PK.seed || 0^48 || ADRS_c(22) || msg))
 * ADRS_c = ADRS[3] || ADRS[8:16] || ADRS[19] || ADRS[20:32]  (22 bytes)      */
static void slh_thash(uint8_t out[MP_SLH_N], const uint8_t *msg, unsigned msglen,
		const uint8_t pkseed[MP_SLH_N], const mp_slh_adrs_t *adrs) {
	static const uint8_t zero48[48] = {0};
	uint8_t adrs_c[22];
	mp_sha256_t ctx;

	adrs_c[0] = adrs->b[3];
	memcpy(adrs_c + 1, adrs->b + 8, 8);
	adrs_c[9] = adrs->b[19];
	memcpy(adrs_c + 10, adrs->b + 20, 12);

	mp_sha256_init(&ctx);
	mp_sha256_update(&ctx, pkseed, MP_SLH_N);
	mp_sha256_update(&ctx, zero48, 48);
	mp_sha256_update(&ctx, adrs_c, 22);
	if (msglen) mp_sha256_update(&ctx, msg, msglen);
	uint8_t d[MP_SHA256_SIZE];
	mp_sha256_final(&ctx, d);
	memcpy(out, d, MP_SLH_N);
}

/* ---- MGF1-SHA-256 ------------------------------------------------------ */
static void slh_mgf1(const uint8_t *seed, unsigned seedlen, uint8_t *mask, unsigned masklen) {
	uint8_t ctr[4];
	uint8_t d[MP_SHA256_SIZE];
	unsigned pos = 0;
	uint32_t c = 0;
	while (pos < masklen) {
		adrs_set_u32(ctr, c);
		mp_sha256_t ctx;
		mp_sha256_init(&ctx);
		mp_sha256_update(&ctx, seed, seedlen);
		mp_sha256_update(&ctx, ctr, 4);
		mp_sha256_final(&ctx, d);
		unsigned n = masklen - pos;
		if (n > MP_SHA256_SIZE) n = MP_SHA256_SIZE;
		memcpy(mask + pos, d, n);
		pos += n;
		c++;
	}
}

/* ---- PRF_msg: R = Trunc16(HMAC-SHA-256(SK.prf, opt || M)) -------------- *
 * Streamed so the message may be arbitrarily large (no alloca).            */
static void slh_prf_msg(uint8_t out[MP_SLH_N], const uint8_t skprf[MP_SLH_N],
		const uint8_t opt[MP_SLH_N], const uint8_t *m, unsigned mlen) {
	uint8_t ipad[MP_SHA256_BLOCK];
	uint8_t opad[MP_SHA256_BLOCK];
	unsigned i;
	for (i = 0; i < MP_SLH_N; i++) {
		ipad[i] = skprf[i] ^ 0x36;
		opad[i] = skprf[i] ^ 0x5c;
	}
	for (; i < MP_SHA256_BLOCK; i++) {
		ipad[i] = 0x36;
		opad[i] = 0x5c;
	}
	uint8_t inner[MP_SHA256_SIZE];
	mp_sha256_t c;
	mp_sha256_init(&c);
	mp_sha256_update(&c, ipad, MP_SHA256_BLOCK);
	mp_sha256_update(&c, opt, MP_SLH_N);
	if (mlen) mp_sha256_update(&c, m, mlen);
	mp_sha256_final(&c, inner);

	mp_sha256_init(&c);
	mp_sha256_update(&c, opad, MP_SHA256_BLOCK);
	mp_sha256_update(&c, inner, MP_SHA256_SIZE);
	mp_sha256_final(&c, inner);
	memcpy(out, inner, MP_SLH_N);
}

/* ---- H_msg: MGF1-SHA-256(R || PK.seed || SHA-256(R || PK.seed || PK.root || M), m) */
static void slh_h_msg(uint8_t *digest, const uint8_t r[MP_SLH_N],
		const uint8_t pkseed[MP_SLH_N], const uint8_t pkroot[MP_SLH_N],
		const uint8_t *m, unsigned mlen) {
	/* inner = SHA-256(R || PK.seed || PK.root || M) */
	mp_sha256_t c;
	uint8_t inner[MP_SHA256_SIZE];
	mp_sha256_init(&c);
	mp_sha256_update(&c, r, MP_SLH_N);
	mp_sha256_update(&c, pkseed, MP_SLH_N);
	mp_sha256_update(&c, pkroot, MP_SLH_N);
	if (mlen) mp_sha256_update(&c, m, mlen);
	mp_sha256_final(&c, inner);

	/* seed = R || PK.seed || inner  (16 + 16 + 32 = 64 bytes) */
	uint8_t seed[MP_SLH_N + MP_SLH_N + MP_SHA256_SIZE];
	memcpy(seed, r, MP_SLH_N);
	memcpy(seed + MP_SLH_N, pkseed, MP_SLH_N);
	memcpy(seed + 2*MP_SLH_N, inner, MP_SHA256_SIZE);
	slh_mgf1(seed, sizeof(seed), digest, MP_SLH_M);
}

/* ---- PRF: Trunc16(SHA-256(PK.seed || 0^48 || ADRS_c || SK.seed)) ------- *
 * This is thash with msg = SK.seed.                                       */
static void slh_prf(uint8_t out[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		const uint8_t skseed[MP_SLH_N], const mp_slh_adrs_t *adrs) {
	slh_thash(out, skseed, MP_SLH_N, pkseed, adrs);
}

/* apply thash `steps` times, hash address running from `start` */
static void slh_chain(uint8_t *out, const uint8_t *x, uint32_t start, uint32_t steps,
		const uint8_t pkseed[MP_SLH_N], mp_slh_adrs_t *adrs) {
	memcpy(out, x, MP_SLH_N);
	uint32_t i;
	for (i = 0; i < steps; i++) {
		adrs_set_hash(adrs, start + i);
		uint8_t tmp[MP_SLH_N];
		slh_thash(tmp, out, MP_SLH_N, pkseed, adrs);
		memcpy(out, tmp, MP_SLH_N);
	}
}

/* len1 base-w values from m, plus len2 checksum values */
static void slh_wots_lengths(uint32_t lengths[MP_SLH_LEN], const uint8_t m[MP_SLH_N]) {
	slh_base_2b(m, MP_SLH_LG_W, lengths, MP_SLH_LEN1);
	uint32_t csum = 0;
	unsigned i;
	for (i = 0; i < MP_SLH_LEN1; i++)
		csum += (MP_SLH_W - 1) - lengths[i];
	unsigned shift = (8 - (MP_SLH_LEN2 * MP_SLH_LG_W) % 8) % 8;
	csum <<= shift;
	uint8_t cb[2] = { (uint8_t)(csum >> 8), (uint8_t)(csum & 0xff) };
	slh_base_2b(cb, MP_SLH_LG_W, lengths + MP_SLH_LEN1, MP_SLH_LEN2);
}

/* wots_pkGen: derive sk_i via PRF (type WOTS_PRF), chain w-1 steps (type
 * WOTS_HASH), compress with T_l (type WOTS_PK). kp = keypair address. */
static void slh_wots_pkGen(uint8_t pk[MP_SLH_N], uint32_t kp,
		const uint8_t skseed[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		const mp_slh_adrs_t *base) {
	uint8_t buf[MP_SLH_LEN * MP_SLH_N];
	unsigned i;
	for (i = 0; i < MP_SLH_LEN; i++) {
		mp_slh_adrs_t a = *base;
		adrs_set_type(&a, MP_SLH_ADRS_WOTS_PRF);
		adrs_set_keypair(&a, kp);
		adrs_set_chain(&a, i);
		adrs_set_hash(&a, 0);
		uint8_t ski[MP_SLH_N];
		slh_prf(ski, pkseed, skseed, &a);

		mp_slh_adrs_t c = *base;
		adrs_set_type(&c, MP_SLH_ADRS_WOTS_HASH);
		adrs_set_keypair(&c, kp);
		adrs_set_chain(&c, i);
		adrs_set_hash(&c, 0);
		slh_chain(buf + i * MP_SLH_N, ski, 0, MP_SLH_W - 1, pkseed, &c);
	}
	mp_slh_adrs_t p = *base;
	adrs_set_type(&p, MP_SLH_ADRS_WOTS_PK);
	adrs_set_keypair(&p, kp);
	slh_thash(pk, buf, MP_SLH_LEN * MP_SLH_N, pkseed, &p);
}

/* wots_sign: chain each sk_i `lengths[i]` steps; output len*n bytes */
static void slh_wots_sign(uint8_t *sig, const uint8_t m[MP_SLH_N], uint32_t kp,
		const uint8_t skseed[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		const mp_slh_adrs_t *base) {
	uint32_t lengths[MP_SLH_LEN];
	slh_wots_lengths(lengths, m);
	unsigned i;
	for (i = 0; i < MP_SLH_LEN; i++) {
		mp_slh_adrs_t a = *base;
		adrs_set_type(&a, MP_SLH_ADRS_WOTS_PRF);
		adrs_set_keypair(&a, kp);
		adrs_set_chain(&a, i);
		adrs_set_hash(&a, 0);
		uint8_t ski[MP_SLH_N];
		slh_prf(ski, pkseed, skseed, &a);

		mp_slh_adrs_t c = *base;
		adrs_set_type(&c, MP_SLH_ADRS_WOTS_HASH);
		adrs_set_keypair(&c, kp);
		adrs_set_chain(&c, i);
		adrs_set_hash(&c, 0);
		slh_chain(sig + i * MP_SLH_N, ski, 0, lengths[i], pkseed, &c);
	}
}

/* wots_pkFromSig: chain each sig value (w-1 - lengths[i]) steps, then T_l */
static void slh_wots_pkFromSig(uint8_t pk[MP_SLH_N], const uint8_t *sig,
		const uint8_t m[MP_SLH_N], uint32_t kp,
		const uint8_t pkseed[MP_SLH_N], const mp_slh_adrs_t *base) {
	uint32_t lengths[MP_SLH_LEN];
	slh_wots_lengths(lengths, m);
	uint8_t buf[MP_SLH_LEN * MP_SLH_N];
	unsigned i;
	for (i = 0; i < MP_SLH_LEN; i++) {
		mp_slh_adrs_t c = *base;
		adrs_set_type(&c, MP_SLH_ADRS_WOTS_HASH);
		adrs_set_keypair(&c, kp);
		adrs_set_chain(&c, i);
		adrs_set_hash(&c, 0);
		slh_chain(buf + i * MP_SLH_N, sig + i * MP_SLH_N, lengths[i],
			(MP_SLH_W - 1) - lengths[i], pkseed, &c);
	}
	mp_slh_adrs_t p = *base;
	adrs_set_type(&p, MP_SLH_ADRS_WOTS_PK);
	adrs_set_keypair(&p, kp);
	slh_thash(pk, buf, MP_SLH_LEN * MP_SLH_N, pkseed, &p);
}

/* ---- XMSS (subtree height h') ------------------------------------------ */

/* recursive: root of subtree at (index i, height h) */
static void slh_xmss_node(uint8_t out[MP_SLH_N], uint32_t i, uint32_t h,
		const uint8_t skseed[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		mp_slh_adrs_t *adrs) {
	if (h == 0) {
		adrs_set_type_clear(adrs, MP_SLH_ADRS_WOTS_HASH);
		adrs_set_keypair(adrs, i);
		slh_wots_pkGen(out, i, skseed, pkseed, adrs);
		return;
	}
	uint8_t left[MP_SLH_N], right[MP_SLH_N];
	slh_xmss_node(left, 2*i, h-1, skseed, pkseed, adrs);
	slh_xmss_node(right, 2*i+1, h-1, skseed, pkseed, adrs);
	adrs_set_type_clear(adrs, MP_SLH_ADRS_TREE);
	adrs_set_treeheight(adrs, h);
	adrs_set_treeindex(adrs, i);
	uint8_t msg[2*MP_SLH_N];
	memcpy(msg, left, MP_SLH_N);
	memcpy(msg + MP_SLH_N, right, MP_SLH_N);
	slh_thash(out, msg, 2*MP_SLH_N, pkseed, adrs);
}

/* xmss_sign: writes (h'+len)*n bytes into sig */
static void slh_xmss_sign(uint8_t *sig, const uint8_t m[MP_SLH_N], uint32_t idx,
		const uint8_t skseed[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		mp_slh_adrs_t *adrs) {
	uint32_t j;
	for (j = 0; j < MP_SLH_HP; j++) {
		uint32_t k = (idx >> j) ^ 1;
		slh_xmss_node(sig + MP_SLH_LEN*MP_SLH_N + j*MP_SLH_N,
			k, j, skseed, pkseed, adrs);
	}
	adrs_set_type_clear(adrs, MP_SLH_ADRS_WOTS_HASH);
	adrs_set_keypair(adrs, idx);
	slh_wots_sign(sig, m, idx, skseed, pkseed, adrs);
}

/* xmss_pkFromSig: sig = wots(len*n) || AUTH(h'*n); returns root */
static void slh_xmss_pkFromSig(uint8_t root[MP_SLH_N], uint32_t idx,
		const uint8_t *sig, const uint8_t m[MP_SLH_N],
		const uint8_t pkseed[MP_SLH_N], mp_slh_adrs_t *adrs) {
	const uint8_t *wots = sig;
	const uint8_t *auth = sig + MP_SLH_LEN*MP_SLH_N;
	adrs_set_type_clear(adrs, MP_SLH_ADRS_WOTS_HASH);
	adrs_set_keypair(adrs, idx);
	uint8_t node[2][MP_SLH_N];
	slh_wots_pkFromSig(node[0], wots, m, idx, pkseed, adrs);
	adrs_set_type_clear(adrs, MP_SLH_ADRS_TREE);
	uint32_t treeidx = idx;
	adrs_set_treeindex(adrs, treeidx);
	uint32_t j;
	for (j = 0; j < MP_SLH_HP; j++) {
		adrs_set_treeheight(adrs, j+1);
		uint8_t msg[2*MP_SLH_N];
		if (((idx >> j) & 1) == 0) {
			treeidx = treeidx / 2;
			adrs_set_treeindex(adrs, treeidx);
			memcpy(msg, node[0], MP_SLH_N);
			memcpy(msg + MP_SLH_N, auth + j*MP_SLH_N, MP_SLH_N);
		} else {
			treeidx = (treeidx - 1) / 2;
			adrs_set_treeindex(adrs, treeidx);
			memcpy(msg, auth + j*MP_SLH_N, MP_SLH_N);
			memcpy(msg + MP_SLH_N, node[0], MP_SLH_N);
		}
		slh_thash(node[1], msg, 2*MP_SLH_N, pkseed, adrs);
		memcpy(node[0], node[1], MP_SLH_N);
	}
	memcpy(root, node[0], MP_SLH_N);
}

/* ---- Hypertree --------------------------------------------------------- */

/* ht_sign: writes (h + d*len)*n = d*(h'+len)*n bytes into sig */
static void slh_ht_sign(uint8_t *sig, const uint8_t m[MP_SLH_N],
		const uint8_t skseed[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		uint64_t idx_tree, uint32_t idx_leaf) {
	mp_slh_adrs_t adrs;
	adrs_zero(&adrs);
	adrs_set_tree(&adrs, idx_tree);
	slh_xmss_sign(sig, m, idx_leaf, skseed, pkseed, &adrs);
	uint8_t pk[MP_SLH_N];
	slh_xmss_pkFromSig(pk, idx_leaf, sig, m, pkseed, &adrs);
	uint32_t layer;
	for (layer = 1; layer < MP_SLH_D; layer++) {
		idx_leaf = idx_tree & ((1u << MP_SLH_HP) - 1);
		idx_tree = idx_tree >> MP_SLH_HP;
		adrs_set_layer(&adrs, layer);
		adrs_set_tree(&adrs, idx_tree);
		uint8_t *sl = sig + layer * (MP_SLH_HP + MP_SLH_LEN) * MP_SLH_N;
		slh_xmss_sign(sl, pk, idx_leaf, skseed, pkseed, &adrs);
		if (layer < MP_SLH_D - 1)
			slh_xmss_pkFromSig(pk, idx_leaf, sl, pk, pkseed, &adrs);
	}
}

/* ht_verify: returns 1 if the recomputed top-layer root equals pkroot */
static int slh_ht_verify(const uint8_t m[MP_SLH_N], const uint8_t *sig,
		const uint8_t pkseed[MP_SLH_N], const uint8_t pkroot[MP_SLH_N],
		uint64_t idx_tree, uint32_t idx_leaf) {
	mp_slh_adrs_t adrs;
	adrs_zero(&adrs);
	adrs_set_tree(&adrs, idx_tree);
	const uint8_t *sl = sig;
	uint8_t pk[MP_SLH_N];
	slh_xmss_pkFromSig(pk, idx_leaf, sl, m, pkseed, &adrs);
	uint32_t layer;
	for (layer = 1; layer < MP_SLH_D; layer++) {
		idx_leaf = idx_tree & ((1u << MP_SLH_HP) - 1);
		idx_tree = idx_tree >> MP_SLH_HP;
		adrs_set_layer(&adrs, layer);
		adrs_set_tree(&adrs, idx_tree);
		sl = sig + layer * (MP_SLH_HP + MP_SLH_LEN) * MP_SLH_N;
		slh_xmss_pkFromSig(pk, idx_leaf, sl, pk, pkseed, &adrs);
	}
	return memcmp(pk, pkroot, MP_SLH_N) == 0;
}

/* ---- FORS -------------------------------------------------------------- */

static void slh_fors_skGen(uint8_t out[MP_SLH_N], const uint8_t skseed[MP_SLH_N],
		const uint8_t pkseed[MP_SLH_N], const mp_slh_adrs_t *adrs, uint32_t idx) {
	mp_slh_adrs_t a = *adrs;
	adrs_set_type_clear(&a, MP_SLH_ADRS_FORS_PRF);
	adrs_set_keypair(&a, adrs_get_keypair(adrs));
	adrs_set_treeindex(&a, idx);
	slh_prf(out, pkseed, skseed, &a);
}

static void slh_fors_node(uint8_t out[MP_SLH_N], uint32_t i, uint32_t h,
		const uint8_t skseed[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		mp_slh_adrs_t *adrs) {
	if (h == 0) {
		uint8_t sk[MP_SLH_N];
		slh_fors_skGen(sk, skseed, pkseed, adrs, i);
		adrs_set_treeheight(adrs, 0);
		adrs_set_treeindex(adrs, i);
		slh_thash(out, sk, MP_SLH_N, pkseed, adrs);
		return;
	}
	uint8_t left[MP_SLH_N], right[MP_SLH_N];
	slh_fors_node(left, 2*i, h-1, skseed, pkseed, adrs);
	slh_fors_node(right, 2*i+1, h-1, skseed, pkseed, adrs);
	adrs_set_treeheight(adrs, h);
	adrs_set_treeindex(adrs, i);
	uint8_t msg[2*MP_SLH_N];
	memcpy(msg, left, MP_SLH_N);
	memcpy(msg + MP_SLH_N, right, MP_SLH_N);
	slh_thash(out, msg, 2*MP_SLH_N, pkseed, adrs);
}

/* fors_sign: writes k*(a+1)*n bytes into sig */
static void slh_fors_sign(uint8_t *sig, const uint8_t *md,
		const uint8_t skseed[MP_SLH_N], const uint8_t pkseed[MP_SLH_N],
		mp_slh_adrs_t *adrs) {
	uint32_t idx[MP_SLH_K];
	slh_base_2b(md, MP_SLH_A, idx, MP_SLH_K);
	unsigned i;
	for (i = 0; i < MP_SLH_K; i++) {
		uint8_t *skp = sig + i*(MP_SLH_A+1)*MP_SLH_N;
		slh_fors_skGen(skp, skseed, pkseed, adrs, i*(1u<<MP_SLH_A) + idx[i]);
		unsigned j;
		for (j = 0; j < MP_SLH_A; j++) {
			uint32_t sib = i*(1u << (MP_SLH_A - j)) + ((idx[i] >> j) ^ 1);
			slh_fors_node(skp + (j+1)*MP_SLH_N, sib, j, skseed, pkseed, adrs);
		}
	}
}

/* fors_pkFromSig: sig is k*(a+1)*n bytes; returns the FORS public key (n) */
static void slh_fors_pkFromSig(uint8_t pk[MP_SLH_N], const uint8_t *sig,
		const uint8_t *md, const uint8_t pkseed[MP_SLH_N], mp_slh_adrs_t *adrs) {
	uint32_t idx[MP_SLH_K];
	slh_base_2b(md, MP_SLH_A, idx, MP_SLH_K);
	uint8_t roots[MP_SLH_K * MP_SLH_N];
	unsigned i;
	for (i = 0; i < MP_SLH_K; i++) {
		const uint8_t *sk = sig + i*(MP_SLH_A+1)*MP_SLH_N;
		const uint8_t *auth = sk + MP_SLH_N;
		adrs_set_treeheight(adrs, 0);
		adrs_set_treeindex(adrs, i*(1u<<MP_SLH_A) + idx[i]);
		uint8_t node[2][MP_SLH_N];
		slh_thash(node[0], sk, MP_SLH_N, pkseed, adrs);
		uint32_t treeidx = i*(1u<<MP_SLH_A) + idx[i];
		unsigned j;
		for (j = 0; j < MP_SLH_A; j++) {
			adrs_set_treeheight(adrs, j+1);
			uint8_t msg[2*MP_SLH_N];
			if (((idx[i] >> j) & 1) == 0) {
				treeidx = treeidx / 2;
				adrs_set_treeindex(adrs, treeidx);
				memcpy(msg, node[0], MP_SLH_N);
				memcpy(msg + MP_SLH_N, auth + j*MP_SLH_N, MP_SLH_N);
			} else {
				treeidx = (treeidx - 1) / 2;
				adrs_set_treeindex(adrs, treeidx);
				memcpy(msg, auth + j*MP_SLH_N, MP_SLH_N);
				memcpy(msg + MP_SLH_N, node[0], MP_SLH_N);
			}
			slh_thash(node[1], msg, 2*MP_SLH_N, pkseed, adrs);
			memcpy(node[0], node[1], MP_SLH_N);
		}
		memcpy(roots + i*MP_SLH_N, node[0], MP_SLH_N);
	}
	mp_slh_adrs_t p = *adrs;
	adrs_set_type_clear(&p, MP_SLH_ADRS_FORS_ROOTS);
	adrs_set_keypair(&p, adrs_get_keypair(adrs));
	slh_thash(pk, roots, MP_SLH_K * MP_SLH_N, pkseed, &p);
}

/* ---- SLH-DSA top level ------------------------------------------------- */

/* message-digest + index split (shared by sign and verify) */
static void slh_split_digest(uint8_t *md, uint64_t *idx_tree, uint32_t *idx_leaf,
		const uint8_t digest[MP_SLH_M]) {
	memcpy(md, digest, 21);
	const uint8_t *tt = digest + 21;
	const uint8_t *tl = digest + 21 + 7;
	uint64_t tr = ((uint64_t)tt[0]<<48)|((uint64_t)tt[1]<<40)|((uint64_t)tt[2]<<32)|
	             ((uint64_t)tt[3]<<24)|((uint64_t)tt[4]<<16)|((uint64_t)tt[5]<<8)|tt[6];
	tr >>= 8*7 - (MP_SLH_H - MP_SLH_HP); /* first (h-h') bits = MSB */
	*idx_tree = tr;
	uint32_t lf = ((uint32_t)tl[0] << 8) | tl[1];
	lf >>= 8*2 - MP_SLH_HP; /* first h' bits = MSB */
	*idx_leaf = lf;
}

mp_ret_t mp_slh_keypair(const void *seedv, unsigned int seedlen, void *privv, void *pubv) {
	if (seedlen < 48)
		return FALSE;
	const uint8_t *seed = seedv;
	uint8_t *sk = privv;
	uint8_t *pk = pubv;
	const uint8_t *skseed = sk;
	const uint8_t *pkseed = sk + 2*MP_SLH_N;
	uint8_t *pkroot = sk + 3*MP_SLH_N;
	memcpy(sk, seed, 48);
	memcpy(pk, seed + 2*MP_SLH_N, MP_SLH_N); /* PK.seed */
	mp_slh_adrs_t adrs;
	adrs_zero(&adrs);
	adrs_set_layer(&adrs, MP_SLH_D - 1);
	slh_xmss_node(pkroot, 0, MP_SLH_HP, skseed, pkseed, &adrs);
	memcpy(pk + MP_SLH_N, pkroot, MP_SLH_N);
	return TRUE;
}

unsigned int mp_slh_sign(const void *privv, const void *msgv, unsigned int mlen, uint8_t *sig) {
	const uint8_t *sk = privv;
	const uint8_t *m = msgv;
	const uint8_t *skseed = sk;
	const uint8_t *skprf = sk + MP_SLH_N;
	const uint8_t *pkseed = sk + 2*MP_SLH_N;
	const uint8_t *pkroot = sk + 3*MP_SLH_N;
	uint8_t optrand[MP_SLH_N];
	memset(optrand, 0, MP_SLH_N); /* deterministic */
	uint8_t *R = sig;
	slh_prf_msg(R, skprf, optrand, m, mlen);
	uint8_t digest[MP_SLH_M];
	slh_h_msg(digest, R, pkseed, pkroot, m, mlen);
	uint8_t md[21];
	uint32_t idx_leaf;
	uint64_t idx_tree;
	slh_split_digest(md, &idx_tree, &idx_leaf, digest);
	uint8_t *sig_fors = sig + MP_SLH_N;
	uint8_t *sig_ht = sig_fors + MP_SLH_K*(MP_SLH_A+1)*MP_SLH_N;
	mp_slh_adrs_t adrs;
	adrs_zero(&adrs);
	adrs_set_layer(&adrs, 0);
	adrs_set_tree(&adrs, idx_tree);
	adrs_set_type_clear(&adrs, MP_SLH_ADRS_FORS_TREE);
	adrs_set_keypair(&adrs, idx_leaf);
	slh_fors_sign(sig_fors, md, skseed, pkseed, &adrs);
	uint8_t pk_fors[MP_SLH_N];
	slh_fors_pkFromSig(pk_fors, sig_fors, md, pkseed, &adrs);
	slh_ht_sign(sig_ht, pk_fors, skseed, pkseed, idx_tree, idx_leaf);
	return MP_SLH_SIG_SIZE;
}

mp_bool_t mp_slh_verify(const void *pubv, const void *msgv, unsigned int mlen,
		const void *sigv, unsigned int siglen) {
	if (siglen != MP_SLH_SIG_SIZE)
		return NO;
	const uint8_t *pk = pubv;
	const uint8_t *m = msgv;
	const uint8_t *sig = sigv;
	const uint8_t *pkseed = pk;
	const uint8_t *pkroot = pk + MP_SLH_N;
	const uint8_t *R = sig;
	const uint8_t *sig_fors = sig + MP_SLH_N;
	const uint8_t *sig_ht = sig_fors + MP_SLH_K*(MP_SLH_A+1)*MP_SLH_N;
	uint8_t digest[MP_SLH_M];
	slh_h_msg(digest, R, pkseed, pkroot, m, mlen);
	uint8_t md[21];
	uint32_t idx_leaf;
	uint64_t idx_tree;
	slh_split_digest(md, &idx_tree, &idx_leaf, digest);
	mp_slh_adrs_t adrs;
	adrs_zero(&adrs);
	adrs_set_layer(&adrs, 0);
	adrs_set_tree(&adrs, idx_tree);
	adrs_set_type_clear(&adrs, MP_SLH_ADRS_FORS_TREE);
	adrs_set_keypair(&adrs, idx_leaf);
	uint8_t pk_fors[MP_SLH_N];
	slh_fors_pkFromSig(pk_fors, sig_fors, md, pkseed, &adrs);
	return slh_ht_verify(pk_fors, sig_ht, pkseed, pkroot, idx_tree, idx_leaf) ? YES : NO;
}

/**@}*/

#endif /* SUPPORT_COMMON_CRYPTO_SIGN_SLH */
