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

#if defined(MP_CRYPTO_SELFTEST_HKDF) && defined(SUPPORT_COMMON_CRYPTO_KDF_HKDF) && defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256)

#include <crypto/selftest.h>
#include <string.h>

/* RFC 5869 appendix A. IKM is 22 bytes of 0x0b. Case 1 uses the 13-byte
 * salt 00..0c and the 10-byte info f0..f9, and asks for 42 bytes.
 * Case 3 uses an empty salt and an empty info. SHA-512 repeats case 1. */

static const uint8_t _ikm[22] = {
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
};
static const uint8_t _salt[13] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c,
};
static const uint8_t _info[10] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9,
};
static const uint8_t _prk256[32] = {
	0x07, 0x77, 0x09, 0x36, 0x2c, 0x2e, 0x32, 0xdf,
	0x0d, 0xdc, 0x3f, 0x0d, 0xc4, 0x7b, 0xba, 0x63,
	0x90, 0xb6, 0xc7, 0x3b, 0xb5, 0x0f, 0x9c, 0x31,
	0x22, 0xec, 0x84, 0x4a, 0xd7, 0xc2, 0xb3, 0xe5,
};
static const uint8_t _okm256[42] = {
	0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a,
	0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a,
	0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c,
	0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf,
	0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18,
	0x58, 0x65,
};
static const uint8_t _okm256_empty[42] = {
	0x8d, 0xa4, 0xe7, 0x75, 0xa5, 0x63, 0xc1, 0x8f,
	0x71, 0x5f, 0x80, 0x2a, 0x06, 0x3c, 0x5a, 0x31,
	0xb8, 0xa1, 0x1f, 0x5c, 0x5e, 0xe1, 0x87, 0x9e,
	0xc3, 0x45, 0x4e, 0x5f, 0x3c, 0x73, 0x8d, 0x2d,
	0x9d, 0x20, 0x13, 0x95, 0xfa, 0xa4, 0xb6, 0x1a,
	0x96, 0xc8,
};

#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512
static const uint8_t _okm512[42] = {
	0x83, 0x23, 0x90, 0x08, 0x6c, 0xda, 0x71, 0xfb,
	0x47, 0x62, 0x5b, 0xb5, 0xce, 0xb1, 0x68, 0xe4,
	0xc8, 0xe2, 0x6a, 0x1a, 0x16, 0xed, 0x34, 0xd9,
	0xfc, 0x7f, 0xe9, 0x2c, 0x14, 0x81, 0x57, 0x93,
	0x38, 0xda, 0x36, 0x2c, 0xb8, 0xd9, 0xf9, 0x25,
	0xd7, 0xcb,
};
#endif

mp_bool_t mp_crypto_selftest_hkdf(void) {
	uint8_t prk[64], out[42];
	unsigned int n;
	mp_bool_t ok = NO;

	n = mp_crypto_hkdf_extract("SHA256", _salt, sizeof(_salt), _ikm, sizeof(_ikm), prk);
	if(n != 32 || mp_crypto_selftest_equal(prk, _prk256, 32) != YES)
		goto done;
	n = mp_crypto_hkdf("SHA256", _salt, sizeof(_salt), _ikm, sizeof(_ikm), _info, sizeof(_info), out, 42);
	if(n != 42 || mp_crypto_selftest_equal(out, _okm256, 42) != YES)
		goto done;
	n = mp_crypto_hkdf("SHA256", NULL, 0, _ikm, sizeof(_ikm), NULL, 0, out, 42);
	if(n != 42 || mp_crypto_selftest_equal(out, _okm256_empty, 42) != YES)
		goto done;
#ifdef SUPPORT_COMMON_CRYPTO_KDF
	n = mp_crypto_kdf("HKDF-SHA256", _salt, sizeof(_salt), _ikm, sizeof(_ikm), _info, sizeof(_info), out, 42);
	if(n != 42 || mp_crypto_selftest_equal(out, _okm256, 42) != YES)
		goto done;
#endif
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHA512
	n = mp_crypto_hkdf("SHA512", _salt, sizeof(_salt), _ikm, sizeof(_ikm), _info, sizeof(_info), out, 42);
	if(n != 42 || mp_crypto_selftest_equal(out, _okm512, 42) != YES)
		goto done;
#ifdef SUPPORT_COMMON_CRYPTO_KDF
	n = mp_crypto_kdf("hkdf-sha-512", _salt, sizeof(_salt), _ikm, sizeof(_ikm), _info, sizeof(_info), out, 42);
	if(n != 42 || mp_crypto_selftest_equal(out, _okm512, 42) != YES)
		goto done;
#endif
#endif
	ok = YES;
done:
	memset(prk, 0, sizeof(prk));
	memset(out, 0, sizeof(out));
	return(ok);
}

#endif
