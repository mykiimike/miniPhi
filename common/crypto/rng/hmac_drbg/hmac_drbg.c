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

#ifdef SUPPORT_COMMON_CRYPTO_RNG_HMAC_DRBG

/**
@defgroup mpCommonHmacDrbg HMAC-DRBG

@ingroup mpCommon

@brief HMAC_DRBG (NIST SP 800-90A section 10.1.2)

K and V are the digest size of the hash. The seed material is the
concatenation of the entropy, the nonce and the personalization string,
fed to HMAC without ever sitting in one buffer. A NULL entropy pointer
reads the HSM when that module is in the build: one digest of entropy,
and half a digest of nonce when the caller passed none. A build without
the accelerator prints and returns FALSE. An entropy buffer is a
deterministic seed.

NIST SP 800-90A approves this for SHA-1, SHA-256, SHA-384 and SHA-512.
Another hash of the build runs the same steps and is not a NIST DRBG.

@{
*/

struct _mp_hmac_drbg_span {
	const uint8_t *data;
	unsigned int size;
};

/* a length with no pointer is a caller error; a pointer with length 0 is empty */
static int _mp_hmac_drbg_span_ok(const void *data, unsigned int size) {
	return(size == 0 || data != NULL);
}

/* NULL entropy is the HSM. Without the accelerator the call is a programming error. */
static mp_ret_t _mp_hmac_drbg_hsm(uint8_t *out, unsigned int size) {
#ifdef SUPPORT_COMMON_CRYPTO_RNG_HSM
	return(mp_hsm_entropy(out, size));
#else
	static mp_bool_t said;

	(void)out;
	(void)size;
	if(said == NO) {
		mp_printk("HMAC-DRBG: this build has no HSM accelerator");
		said = YES;
	}
	return(FALSE);
#endif
}

/* HMAC(key, V || extra || spans); extra is the 0x00 or 0x01 byte, or nothing */
static void _mp_hmac_drbg_mac(mp_hmac_drbg_t *ctx, const uint8_t *extra, const struct _mp_hmac_drbg_span *span, int count, uint8_t *out) {
	mp_hmac_t hmac;
	int n;

	mp_hmac_init(&hmac, ctx->algo->name, ctx->key, ctx->algo->size);
	mp_hmac_update(&hmac, ctx->v, ctx->algo->size);
	if(extra)
		mp_hmac_update(&hmac, extra, 1);
	for(n = 0; n < count; n++)
		if(span[n].size)
			mp_hmac_update(&hmac, span[n].data, span[n].size);
	mp_hmac_final(&hmac, out);
}

/* NIST HMAC_DRBG_Update. No span, or only empty ones, is provided_data = Null: the 0x01 step is skipped */
static void _mp_hmac_drbg_update(mp_hmac_drbg_t *ctx, const struct _mp_hmac_drbg_span *span, int count) {
	uint8_t sep;
	int n, any;

	any = 0;
	for(n = 0; n < count; n++)
		if(span[n].size)
			any = 1;

	sep = 0x00;
	_mp_hmac_drbg_mac(ctx, &sep, any ? span : NULL, any ? count : 0, ctx->key);
	_mp_hmac_drbg_mac(ctx, NULL, NULL, 0, ctx->v);
	if(!any)
		return;
	sep = 0x01;
	_mp_hmac_drbg_mac(ctx, &sep, span, count, ctx->key);
	_mp_hmac_drbg_mac(ctx, NULL, NULL, 0, ctx->v);
}

/**
 * @brief Seed a generator
 *
 * @param[in] ctx Context, owned by the caller
 * @param[in] hash Name of the hash, as mp_hash_find() takes it
 * @param[in] entropy Entropy bytes, or NULL to read the HSM
 * @param[in] entropylen Entropy length; 0 with a NULL pointer reads the HSM
 * @param[in] nonce Nonce, or NULL. With a NULL entropy pointer, a NULL nonce is also read from the HSM
 * @param[in] noncelen Nonce length
 * @param[in] personal Personalization string, or NULL
 * @param[in] personlen Personalization length
 * @return FALSE when the build does not carry hash, the HSM is absent or returned nothing, or a supplied entropy is empty
 */
mp_ret_t mp_hmac_drbg_init(mp_hmac_drbg_t *ctx, const char *hash, const void *entropy, unsigned int entropylen, const void *nonce, unsigned int noncelen, const void *personal, unsigned int personlen) {
	const mp_hash_algo_t *algo;
	struct _mp_hmac_drbg_span span[3];
	uint8_t drawn_entropy[MP_HASH_SIZE_MAX];
	uint8_t drawn_nonce[MP_HASH_SIZE_MAX];
	unsigned int drawn_elen = 0;
	unsigned int drawn_nlen = 0;

	memset(ctx, 0, sizeof(*ctx));
	algo = mp_hash_find(hash);
	if(!algo || !_mp_hmac_drbg_span_ok(entropy, entropylen) || !_mp_hmac_drbg_span_ok(nonce, noncelen) || !_mp_hmac_drbg_span_ok(personal, personlen))
		return(FALSE);
	if(!entropy) {
		if(entropylen)
			return(FALSE);
		drawn_elen = algo->size;
		if(_mp_hmac_drbg_hsm(drawn_entropy, drawn_elen) != TRUE)
			return(FALSE);
		entropy = drawn_entropy;
		entropylen = drawn_elen;
		if(!nonce) {
			drawn_nlen = algo->size / 2;
			if(!drawn_nlen)
				drawn_nlen = algo->size;
			if(_mp_hmac_drbg_hsm(drawn_nonce, drawn_nlen) != TRUE) {
				memset(drawn_entropy, 0, drawn_elen);
				return(FALSE);
			}
			nonce = drawn_nonce;
			noncelen = drawn_nlen;
		}
	}
	else if(!entropylen)
		return(FALSE);

	ctx->algo = algo;
	memset(ctx->key, 0x00, algo->size);
	memset(ctx->v, 0x01, algo->size);
	span[0].data = entropy;
	span[0].size = entropylen;
	span[1].data = nonce;
	span[1].size = nonce ? noncelen : 0;
	span[2].data = personal;
	span[2].size = personal ? personlen : 0;
	_mp_hmac_drbg_update(ctx, span, 3);
	ctx->reseed = 1;
	if(drawn_elen)
		memset(drawn_entropy, 0, drawn_elen);
	if(drawn_nlen)
		memset(drawn_nonce, 0, drawn_nlen);
	return(TRUE);
}

/**
 * @brief Mix new entropy in
 *
 * @param[in] ctx Context
 * @param[in] entropy Entropy bytes, or NULL to read the HSM
 * @param[in] entropylen Entropy length; 0 with a NULL pointer reads the HSM
 * @param[in] additional Extra bytes, or NULL
 * @param[in] addlen Extra length
 * @return FALSE when ctx is not started, the HSM is absent or returned nothing, or a supplied entropy is empty; ctx is unchanged
 */
mp_ret_t mp_hmac_drbg_reseed(mp_hmac_drbg_t *ctx, const void *entropy, unsigned int entropylen, const void *additional, unsigned int addlen) {
	struct _mp_hmac_drbg_span span[2];
	uint8_t drawn[MP_HASH_SIZE_MAX];
	unsigned int drawn_len = 0;

	if(!ctx->algo || !_mp_hmac_drbg_span_ok(entropy, entropylen) || !_mp_hmac_drbg_span_ok(additional, addlen))
		return(FALSE);
	if(!entropy) {
		if(entropylen)
			return(FALSE);
		drawn_len = ctx->algo->size;
		if(_mp_hmac_drbg_hsm(drawn, drawn_len) != TRUE)
			return(FALSE);
		entropy = drawn;
		entropylen = drawn_len;
	}
	else if(!entropylen)
		return(FALSE);
	span[0].data = entropy;
	span[0].size = entropylen;
	span[1].data = additional;
	span[1].size = additional ? addlen : 0;
	_mp_hmac_drbg_update(ctx, span, 2);
	ctx->reseed = 1;
	if(drawn_len)
		memset(drawn, 0, drawn_len);
	return(TRUE);
}

/**
 * @brief Draw random bytes
 *
 * @param[in] ctx Context
 * @param[out] out Buffer of size bytes
 * @param[in] size 1 to MP_HMAC_DRBG_MAX
 * @param[in] additional Extra bytes mixed in, or NULL
 * @param[in] addlen Extra length
 * @return FALSE when a reseed is due or the request is empty; out is unchanged
 */
mp_ret_t mp_hmac_drbg_generate(mp_hmac_drbg_t *ctx, void *out, unsigned int size, const void *additional, unsigned int addlen) {
	struct _mp_hmac_drbg_span span;
	uint8_t *bytes = out;
	unsigned int n;

	if(!ctx->algo || !out || !size || size > MP_HMAC_DRBG_MAX || !_mp_hmac_drbg_span_ok(additional, addlen))
		return(FALSE);
	if(ctx->reseed > MP_HMAC_DRBG_RESEED)
		return(FALSE);

	span.data = additional;
	span.size = additional ? addlen : 0;
	if(span.size)
		_mp_hmac_drbg_update(ctx, &span, 1);

	while(size) {
		_mp_hmac_drbg_mac(ctx, NULL, NULL, 0, ctx->v);
		n = size < ctx->algo->size ? size : ctx->algo->size;
		memcpy(bytes, ctx->v, n);
		bytes += n;
		size -= n;
	}

	_mp_hmac_drbg_update(ctx, &span, span.size ? 1 : 0);
	ctx->reseed++;
	return(TRUE);
}

/**
 * @brief Drop a generator
 *
 * Wipes the context. Safe on a context already freed.
 *
 * @param[in] ctx Context
 */
void mp_hmac_drbg_free(mp_hmac_drbg_t *ctx) {
	memset(ctx, 0, sizeof(*ctx));
}

/**@}*/

#endif
