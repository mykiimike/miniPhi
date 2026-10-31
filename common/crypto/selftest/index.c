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

#ifdef SUPPORT_COMMON_CRYPTO_SELFTEST

#include <crypto/selftest.h>
#include <string.h>

/**
@defgroup mpCommonSelftest Self-test

@ingroup mpCommon

@brief Known answers of the standalone algorithms

A switch per algorithm leaves the check out of the image.
The app calls mp_crypto_selftest() when it wants the check. Nothing here runs at boot.
The check calls the public API, so the accelerator is what gets checked when it accepted the operation.

@code
if(mp_crypto_selftest_find("AES") && mp_crypto_selftest("AES") != YES)
	mp_printk("AES self-test failed");
@endcode
*/

static const mp_crypto_selftest_t _table[] = {
#if defined(MP_CRYPTO_SELFTEST_AES) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES)
	{ "AES", "AES", "cipher", mp_crypto_selftest_aes },
#endif
#if defined(MP_CRYPTO_SELFTEST_CHACHA20) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_CHACHA20)
	{ "CHACHA20", "ChaCha20", "cipher", mp_crypto_selftest_chacha20 },
#endif
#if defined(MP_CRYPTO_SELFTEST_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_XTS) && defined(SUPPORT_COMMON_CRYPTO_CIPHER_AES)
	{ "XTS", "XTS", "cipher", mp_crypto_selftest_xts },
#endif
#if defined(MP_CRYPTO_SELFTEST_SHA1) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA1)
	{ "SHA1", "SHA-1", "hash", mp_crypto_selftest_sha1 },
#endif
#if defined(MP_CRYPTO_SELFTEST_SHA256) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256)
	{ "SHA256", "SHA-256", "hash", mp_crypto_selftest_sha256 },
#endif
#if defined(MP_CRYPTO_SELFTEST_SHA384) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA384)
	{ "SHA384", "SHA-384", "hash", mp_crypto_selftest_sha384 },
#endif
#if defined(MP_CRYPTO_SELFTEST_SHA512) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA512)
	{ "SHA512", "SHA-512", "hash", mp_crypto_selftest_sha512 },
#endif
#if defined(MP_CRYPTO_SELFTEST_BLAKE2S) && defined(SUPPORT_COMMON_CRYPTO_HASH_BLAKE2S)
	{ "BLAKE2S", "BLAKE2s", "hash", mp_crypto_selftest_blake2s },
#endif
#if defined(MP_CRYPTO_SELFTEST_SHAKE) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHAKE)
	{ "SHAKE", "SHA-3 / SHAKE", "hash", mp_crypto_selftest_shake },
#endif
#if defined(MP_CRYPTO_SELFTEST_HMAC) && defined(SUPPORT_COMMON_CRYPTO_MAC_HMAC) && defined(SUPPORT_COMMON_CRYPTO_HASH_SHA256)
	{ "HMAC", "HMAC", "mac", mp_crypto_selftest_hmac },
#endif
#if defined(MP_CRYPTO_SELFTEST_POLY1305) && defined(SUPPORT_COMMON_CRYPTO_MAC_POLY1305)
	{ "POLY1305", "Poly1305", "mac", mp_crypto_selftest_poly1305 },
#endif
#if defined(MP_CRYPTO_SELFTEST_MLKEM) && defined(SUPPORT_COMMON_CRYPTO_KEM_MLKEM)
	{ "MLKEM", "ML-KEM", "kem", mp_crypto_selftest_mlkem },
#endif
#if defined(MP_CRYPTO_SELFTEST_HQC) && defined(SUPPORT_COMMON_CRYPTO_KEM_HQC)
	{ "HQC", "HQC-128", "kem", mp_crypto_selftest_hqc },
#endif
#if defined(MP_CRYPTO_SELFTEST_X25519) && defined(SUPPORT_COMMON_CRYPTO_KEM_X25519)
	{ "X25519", "X25519", "kem", mp_crypto_selftest_x25519 },
#endif
#if defined(MP_CRYPTO_SELFTEST_RSAKEM) && defined(SUPPORT_COMMON_CRYPTO_KEM_RSAKEM) && defined(SUPPORT_COMMON_CRYPTO_SIGN_RSA)
	{ "RSAKEM", "RSA-KEM", "kem", mp_crypto_selftest_rsakem },
#endif
	{ NULL, NULL, NULL, NULL },
};

/**
 * @brief How many self-tests this build kept
 * @return The count, zero when every switch is off or the algorithms are absent
 */
int mp_crypto_selftest_count(void) {
	return((int)(sizeof(_table) / sizeof(_table[0])) - 1);
}

/**
 * @brief One self-test by position
 * @param[in] index From zero to the count minus one
 * @return NULL when index is outside that
 */
const mp_crypto_selftest_t *mp_crypto_selftest_at(int index) {
	if(index < 0 || index >= mp_crypto_selftest_count())
		return(NULL);
	return(&_table[index]);
}

/**
 * @brief Find a self-test by its module name
 * @param[in] name "AES", "SHA256"
 * @return NULL when this build does not carry that check
 */
const mp_crypto_selftest_t *mp_crypto_selftest_find(const char *name) {
	int n;

	if(!name)
		return(NULL);
	for(n = 0; n < mp_crypto_selftest_count(); n++)
		if(strcmp(_table[n].name, name) == 0)
			return(&_table[n]);
	return(NULL);
}

/**
 * @brief Run one known-answer check
 * @param[in] name Module name, "AES"
 * @return YES when the answer matches. NO when it does not, or the check is not in this build.
 * Prints the label, then passed or failed and the microseconds the check took.
 * A missing name stays silent.
 */
static unsigned long _us;

unsigned long mp_crypto_selftest_last_us(void) {
	return(_us);
}

mp_bool_t mp_crypto_selftest(const char *name) {
	const mp_crypto_selftest_t *test = mp_crypto_selftest_find(name);
	unsigned long t0;
	mp_bool_t ok;

	_us = 0;
	if(!test || !test->run)
		return(NO);
	mp_printk("SELFTEST: %s", test->label);
	t0 = mp_clock_us();
	ok = test->run() == YES ? YES : NO;
	_us = mp_clock_us() - t0;
	mp_printk("SELFTEST: %s %s %lu us", test->label, ok == YES ? "passed" : "failed", _us);
	return(ok);
}

#endif
