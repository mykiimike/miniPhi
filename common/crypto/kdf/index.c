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

#ifdef SUPPORT_COMMON_CRYPTO_KDF

/**
@defgroup mpCommonKdf KDF API

@ingroup mpCommon

@brief Any key derivation of the build, picked by name

@code
uint8_t out[42];

mp_crypto_kdf("HKDF-SHA256", salt, 13, ikm, 22, info, 10, out, 42);
@endcode

A new derivation adds one branch below. The caller owns every buffer.

@{
*/

static char _upper(char c) {
	return(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
}

/** the rest of name after prefix, any case; NULL when name does not start with it */
static const char *_after(const char *name, const char *prefix) {
	while(*prefix) {
		if(_upper(*name) != *prefix)
			return(NULL);
		name++;
		prefix++;
	}
	return(name);
}

/**
 * @brief Derive L bytes
 * @param[in] name "HKDF-" and a hash of the build, any case
 * @param[in] salt Salt, ignored when saltlen is 0
 * @param[in] saltlen Salt length
 * @param[in] ikm Input keying material
 * @param[in] ikmlen Length of ikm
 * @param[in] info Context, ignored when infolen is 0
 * @param[in] infolen Length of info
 * @param[out] out First L bytes
 * @param[in] L Output length
 * @return L, or 0 when the build does not carry name or L does not fit
 */
unsigned int mp_crypto_kdf(const char *name, const void *salt, unsigned int saltlen, const void *ikm, unsigned int ikmlen, const void *info, unsigned int infolen, uint8_t *out, unsigned int L) {
	if(!name)
		return(0);
#ifdef SUPPORT_COMMON_CRYPTO_KDF_HKDF
	{
		const char *hash = _after(name, "HKDF-");

		if(hash && *hash)
			return(mp_crypto_hkdf(hash, salt, saltlen, ikm, ikmlen, info, infolen, out, L));
	}
#else
	(void)salt;
	(void)saltlen;
	(void)ikm;
	(void)ikmlen;
	(void)info;
	(void)infolen;
	(void)out;
	(void)L;
#endif
	return(0);
}

/**@}*/

#endif
