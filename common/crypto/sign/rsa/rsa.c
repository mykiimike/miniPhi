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

#ifdef SUPPORT_COMMON_CRYPTO_SIGN_RSA

/**
@defgroup mpCommonRsa RSA

@ingroup mpCommon

@brief PKCS#1 v1.5 SHA-256, signed with the CRT

The exponent is 65537. MP_RSA_BITS is the modulus. A private key is n, e, d, p, q, d mod (p-1),
d mod (q-1) and q^{-1} mod p. Signing is two exponentiations, one per prime.
Key generation draws the primes from HMAC-DRBG and is far too slow for
the screen; the API is there for a host. The buffers below are shared,
so one RSA operation runs at a time.

@{
*/

#define _WP MP_RSA_WPRIME
#define _WM MP_RSA_WMOD
#define _E 65537u

/** PKCS#1 DigestInfo for SHA-256, nineteen bytes, the hash follows */
static const uint8_t _di[19] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
	0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};

/** odd primes below 1024, for the trial division before Miller-Rabin */
static const uint16_t _small[] = {
	3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59,
	61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137,
	139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227,
	229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313,
	317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419,
	421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509,
	521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617,
	619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727,
	733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829,
	839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947,
	953, 967, 971, 977, 983, 991, 997, 1009, 1013, 1019, 1021
};

static uint32_t _p[_WP], _q[_WP], _dp[_WP], _dq[_WP], _qinv[_WP];
static uint32_t _m1[_WP], _m2[_WP], _h[_WP], _t[_WP];
static uint32_t _n[_WM], _d[_WM], _wide[_WM], _acc[_WM];
static uint8_t _raw[MP_RSA_MODULUS];

static void _wipe(void) {
	memset(_p, 0, sizeof(_p));
	memset(_q, 0, sizeof(_q));
	memset(_dp, 0, sizeof(_dp));
	memset(_dq, 0, sizeof(_dq));
	memset(_qinv, 0, sizeof(_qinv));
	memset(_m1, 0, sizeof(_m1));
	memset(_m2, 0, sizeof(_m2));
	memset(_h, 0, sizeof(_h));
	memset(_t, 0, sizeof(_t));
	memset(_n, 0, sizeof(_n));
	memset(_d, 0, sizeof(_d));
	memset(_wide, 0, sizeof(_wide));
	memset(_acc, 0, sizeof(_acc));
	memset(_raw, 0, sizeof(_raw));
	mp_bn_clean();
}

/** EM = 00 01 FF..FF 00 DigestInfo H(msg), 256 bytes */
static void _encode(const void *msg, unsigned int len) {
	unsigned int ps = MP_RSA_MODULUS - 3 - sizeof(_di) - MP_SHA256_SIZE;

	mp_sha256(msg, len, _raw + MP_RSA_MODULUS - MP_SHA256_SIZE);
	memset(_raw, 0xff, ps + 2);
	_raw[0] = 0x00;
	_raw[1] = 0x01;
	_raw[2 + ps] = 0x00;
	memcpy(_raw + 3 + ps, _di, sizeof(_di));
}

static int _same(const uint8_t *a, const uint8_t *b, unsigned int n) {
	unsigned int i;
	uint8_t d = 0;

	for(i = 0; i < n; i++)
		d |= a[i] ^ b[i];
	return(d == 0);
}

static void _load32(uint32_t *a, const uint8_t *be) {
	mp_bn_from_be(a, _WP, be, MP_RSA_PRIME);
}

unsigned int mp_rsa_sign(const void *priv, const void *msg, unsigned int len, uint8_t *sig) {
	const uint8_t *key = priv;
	uint32_t carry;
	int i;

	if(!priv || !sig || (!msg && len))
		return(0);
	_encode(msg, len);
	mp_bn_from_be(_acc, _WM, _raw, MP_RSA_MODULUS);
	mp_bn_from_be(_n, _WM, key, MP_RSA_MODULUS);
	_load32(_p, key + MP_RSA_OFF_P);
	_load32(_q, key + MP_RSA_OFF_Q);
	_load32(_dp, key + MP_RSA_OFF_DP);
	_load32(_dq, key + MP_RSA_OFF_DQ);
	_load32(_qinv, key + MP_RSA_OFF_QINV);

	mp_bn_mod(_m1, _acc, _WM, _p, _WP);
	mp_bn_mod(_m2, _acc, _WM, _q, _WP);
	if(mp_bn_modexp(_m1, _m1, _dp, _WP, _p, _WP) != TRUE)
		goto fail;
	if(mp_bn_modexp(_m2, _m2, _dq, _WP, _q, _WP) != TRUE)
		goto fail;

	/* h = qinv * (m1 - m2) mod p, then s = m2 + h*q */
	mp_bn_mod(_t, _m2, _WP, _p, _WP);
	mp_bn_sub_mod(_h, _m1, _t, _p, _WP);
	mp_bn_mul(_wide, _h, _qinv, _WP);
	mp_bn_mod(_h, _wide, _WM, _p, _WP);
	mp_bn_mul(_wide, _h, _q, _WP);
	carry = mp_bn_add(_wide, _wide, _m2, _WP);
	/* fixed range: the carry runs over every high word so the work does not
	 * depend on whether a carry was produced. s = m2 + h*q is below n for a
	 * valid key, so carry settles to 0; the sanity check below still catches a bad key. */
	for(i = _WP; i < _WM; i++) {
		uint64_t s = (uint64_t)_wide[i] + carry;

		_wide[i] = (uint32_t)s;
		carry = (uint32_t)(s >> 32);
	}
	if(carry || mp_bn_cmp(_wide, _n, _WM) >= 0)
		goto fail;
	mp_bn_to_be(sig, MP_RSA_SIG, _wide, _WM);
	_wipe();
	return(MP_RSA_SIG);
fail:
	_wipe();
	return(0);
}

mp_bool_t mp_rsa_verify(const void *pub, const void *msg, unsigned int len, const void *sig, unsigned int siglen) {
	const uint8_t *key = pub;
	uint32_t e;
	mp_bool_t ok = NO;

	if(!pub || !sig || siglen != MP_RSA_SIG || (!msg && len))
		return(NO);
	_encode(msg, len);
	mp_bn_from_be(_n, _WM, key, MP_RSA_MODULUS);
	mp_bn_from_be(&e, 1, key + MP_RSA_MODULUS, MP_RSA_E);
	mp_bn_from_be(_acc, _WM, sig, MP_RSA_SIG);
	if(mp_bn_is_zero(_acc, _WM) || mp_bn_cmp(_acc, _n, _WM) >= 0)
		goto out;
	if(mp_bn_modexp(_wide, _acc, &e, 1, _n, _WM) != TRUE)
		goto out;
	{
		uint8_t got[MP_RSA_MODULUS];

		/* _raw still holds the expected EM from _encode */
		mp_bn_to_be(got, MP_RSA_MODULUS, _wide, _WM);
		ok = _same(got, _raw, MP_RSA_MODULUS) ? YES : NO;
		memset(got, 0, sizeof(got));
	}
out:
	_wipe();
	return(ok);
}

static void _fill(void *user, uint8_t *dst, unsigned int len) {
	if(mp_hmac_drbg_generate(user, dst, len, NULL, 0) != TRUE)
		memset(dst, 0, len);
}

/** YES when no prime below 1024 divides a, and 65537 does not divide a-1 */
static int _trial(const uint32_t *a) {
	unsigned int i;

	if((a[0] & 1u) == 0)
		return(0);
	for(i = 0; i < sizeof(_small) / sizeof(_small[0]); i++) {
		if(mp_bn_mod_u32(a, _WP, _small[i]) == 0)
			return(0);
	}
	return(mp_bn_mod_u32(a, _WP, _E) != 1);
}

static int _prime(uint32_t *out, mp_hmac_drbg_t *drbg) {
	uint32_t step;
	int draw;

	mp_bn_zero(_t, _WP);
	_t[0] = 2;
	for(draw = 0; draw < 16; draw++) {
		if(mp_hmac_drbg_generate(drbg, _raw, MP_RSA_PRIME, NULL, 0) != TRUE)
			return(0);
		mp_bn_from_be(out, _WP, _raw, MP_RSA_PRIME);
		out[_WP - 1] |= 0x80000000u;
		out[0] |= 1u;
		for(step = 0; step < 4096; step++) {
			if(_trial(out) && mp_bn_prime(out, _WP, 8, _fill, drbg) == YES)
				return(1);
			if(mp_bn_add(out, out, _t, _WP))
				break;
		}
	}
	return(0);
}

mp_ret_t mp_rsa_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	mp_hmac_drbg_t drbg;
	uint8_t *key = priv;

	if(!seed || seedlen < 32 || !priv || !pub)
		return(FALSE);
	if(mp_hmac_drbg_init(&drbg, "SHA256", seed, seedlen, "RSA", 3, NULL, 0) != TRUE)
		return(FALSE);
	if(!_prime(_p, &drbg))
		goto fail;
	do {
		if(!_prime(_q, &drbg))
			goto fail;
	} while(mp_bn_cmp(_p, _q, _WP) == 0);

	mp_bn_zero(_t, _WP);
	_t[0] = 1;
	mp_bn_sub(_m1, _p, _t, _WP);
	mp_bn_sub(_m2, _q, _t, _WP);
	mp_bn_mul(_wide, _m1, _m2, _WP);
	mp_bn_zero(_acc, _WM);
	_acc[0] = _E;
	if(mp_bn_modinv(_d, _acc, _wide, _WM) != TRUE)
		goto fail;
	mp_bn_mod(_dp, _d, _WM, _m1, _WP);
	mp_bn_mod(_dq, _d, _WM, _m2, _WP);
	if(mp_bn_modinv(_qinv, _q, _p, _WP) != TRUE)
		goto fail;
	mp_bn_mul(_n, _p, _q, _WP);

	mp_bn_to_be(key, MP_RSA_MODULUS, _n, _WM);
	key[MP_RSA_OFF_E] = 0x00;
	key[MP_RSA_OFF_E + 1] = 0x01;
	key[MP_RSA_OFF_E + 2] = 0x00;
	key[MP_RSA_OFF_E + 3] = 0x01;
	mp_bn_to_be(key + MP_RSA_OFF_D, MP_RSA_MODULUS, _d, _WM);
	mp_bn_to_be(key + MP_RSA_OFF_P, MP_RSA_PRIME, _p, _WP);
	mp_bn_to_be(key + MP_RSA_OFF_Q, MP_RSA_PRIME, _q, _WP);
	mp_bn_to_be(key + MP_RSA_OFF_DP, MP_RSA_PRIME, _dp, _WP);
	mp_bn_to_be(key + MP_RSA_OFF_DQ, MP_RSA_PRIME, _dq, _WP);
	mp_bn_to_be(key + MP_RSA_OFF_QINV, MP_RSA_PRIME, _qinv, _WP);
	memcpy(pub, key, MP_RSA_PUB);
	mp_hmac_drbg_free(&drbg);
	_wipe();
	return(TRUE);
fail:
	mp_hmac_drbg_free(&drbg);
	_wipe();
	return(FALSE);
}

/**@}*/

#endif
