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

#ifdef SUPPORT_COMMON_CRYPTO_MAC

/**
@defgroup mpCommonMac MAC API

@ingroup mpCommon

@brief Any MAC of the build, picked by name

@code
mp_mac_t mac;
uint8_t tag[MP_MAC_SIZE_MAX];

if(mp_mac_init(&mac, "HMAC-SHA256", key, keylen) == TRUE) {
	mp_mac_update(&mac, data, size);
	mp_mac_final(&mac, tag);
}
if(mp_mac_equal(tag, received, 32) == YES)
	...
@endcode

A new MAC adds a kind, its branches below, and a member in the union of mac.h.

@{
*/

enum {
	_MP_MAC_NONE = 0,
	_MP_MAC_HMAC,
	_MP_MAC_BLAKE2S,
	_MP_MAC_POLY1305,
};

static char _mp_mac_upper(char c) {
	return(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
}

/* the rest of name after prefix, any case; NULL when name does not start with it */
static const char *_mp_mac_after(const char *name, const char *prefix) {
	while(*prefix) {
		if(_mp_mac_upper(*name) != *prefix)
			return(NULL);
		name++;
		prefix++;
	}
	return(name);
}

/**
 * @brief Start a MAC
 *
 * @param[in] mac Context, owned by the caller
 * @param[in] name "HMAC-" and a hash of the build, or "BLAKE2S"; any case
 * @param[in] key Key bytes
 * @param[in] keylen Key length; BLAKE2s takes at most MP_BLAKE2S_KEY
 * @return FALSE when the build does not carry name or the key does not fit
 */
mp_ret_t mp_mac_init(mp_mac_t *mac, const char *name, const void *key, unsigned int keylen) {
	const char *rest;

	memset(mac, 0, sizeof(*mac));
	if(!name)
		return(FALSE);
	if(!key)
		keylen = 0;

#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC
	rest = _mp_mac_after(name, "HMAC-");
	if(rest && mp_hmac_init(&mac->ctx.hmac, rest, key, keylen) == TRUE) {
		mac->kind = _MP_MAC_HMAC;
		mac->size = mac->ctx.hmac.algo->size;
		return(TRUE);
	}
#endif

#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
	rest = _mp_mac_after(name, "BLAKE2S");
	/* without a key it is a hash, not a MAC */
	if(rest && !*rest && keylen > 0 && keylen <= MP_BLAKE2S_KEY) {
		mp_blake2s_init_key(&mac->ctx.blake2s, key, keylen);
		mac->kind = _MP_MAC_BLAKE2S;
		mac->size = MP_BLAKE2S_SIZE;
		return(TRUE);
	}
#endif

#ifdef SUPPORT_COMMON_CRYPTO_MAC_POLY1305
	rest = _mp_mac_after(name, "POLY1305");
	if(rest && !*rest && mp_poly1305_init(&mac->ctx.poly1305, key, keylen) == TRUE) {
		mac->kind = _MP_MAC_POLY1305;
		mac->size = MP_POLY1305_SIZE;
		return(TRUE);
	}
#endif

	(void)rest;
	memset(mac, 0, sizeof(*mac));
	return(FALSE);
}

/**
 * @brief MAC more bytes
 *
 * Does nothing on a MAC that is not started.
 *
 * @param[in] mac Context
 * @param[in] data Bytes
 * @param[in] size Number of bytes
 */
void mp_mac_update(mp_mac_t *mac, const void *data, unsigned int size) {
	switch(mac->kind) {
#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC
		case _MP_MAC_HMAC:
			mp_hmac_update(&mac->ctx.hmac, data, size);
			break;
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
		case _MP_MAC_BLAKE2S:
			mp_blake2s_update(&mac->ctx.blake2s, data, size);
			break;
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MAC_POLY1305
		case _MP_MAC_POLY1305:
			mp_poly1305_update(&mac->ctx.poly1305, data, size);
			break;
#endif
		default:
			(void)data;
			(void)size;
			break;
	}
}

/**
 * @brief End a MAC
 *
 * The context is wiped afterwards.
 *
 * @param[in] mac Context
 * @param[out] tag At least MP_MAC_SIZE_MAX bytes, or mp_mac_size()
 * @return Bytes written, 0 when mac was not started
 */
unsigned int mp_mac_final(mp_mac_t *mac, uint8_t *tag) {
	unsigned int size = mac->size;

	switch(mac->kind) {
#ifdef SUPPORT_COMMON_CRYPTO_MAC_HMAC
		case _MP_MAC_HMAC:
			mp_hmac_final(&mac->ctx.hmac, tag);
			break;
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S
		case _MP_MAC_BLAKE2S:
			mp_blake2s_final(&mac->ctx.blake2s, tag);
			break;
#endif
#ifdef SUPPORT_COMMON_CRYPTO_MAC_POLY1305
		case _MP_MAC_POLY1305:
			size = mp_poly1305_final(&mac->ctx.poly1305, tag);
			break;
#endif
		default:
			(void)tag;
			size = 0;
			break;
	}
	mp_mac_free(mac);
	return(size);
}

/**
 * @brief Drop a MAC
 *
 * Wipes the context, the key with it. Safe on a MAC already ended.
 *
 * @param[in] mac Context
 */
void mp_mac_free(mp_mac_t *mac) {
	memset(mac, 0, sizeof(*mac));
}

/**
 * @brief Tag size of a started MAC
 *
 * @param[in] mac Context
 * @return Bytes, 0 when not started
 */
unsigned int mp_mac_size(const mp_mac_t *mac) {
	return(mac->kind != _MP_MAC_NONE ? mac->size : 0);
}

/**
 * @brief Compare two tags
 *
 * Reads every byte whatever they hold, so the time tells nothing
 * about how much of a forged tag was right.
 *
 * @param[in] a First tag
 * @param[in] b Second tag
 * @param[in] size Bytes to compare
 * @return YES when they match
 */
mp_bool_t mp_mac_equal(const void *a, const void *b, unsigned int size) {
	const volatile uint8_t *x = a;
	const volatile uint8_t *y = b;
	uint8_t diff = 0;
	unsigned int n;

	for(n = 0; n < size; n++)
		diff |= x[n] ^ y[n];
	return(diff == 0 ? YES : NO);
}

/**@}*/

#endif
