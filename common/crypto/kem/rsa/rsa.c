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

#ifdef SUPPORT_COMMON_CRYPTO_KEM_RSAKEM

/**
@defgroup mpCommonRsakem RSA-KEM

@ingroup mpCommon

@brief RSA-KEM (RFC 5990) with SHAKE-256

z is the coins, one modulus of big-endian bytes, in [1, n). The ciphertext
is z^e mod n with e = 65537. The shared secret is 32 bytes of SHAKE-256
over those same bytes. Decapsulation is the CRT. Its time follows the width
of the exponents, not which bits are set. An out-of-range z still runs the
exponentiation, and a ciphertext outside [1, n) still runs the CRT. The
result is then replaced. The scratch is shared, so one operation runs at a time.

@{
*/

#define _WP MP_CRYPTO_RSA_WPRIME
#define _WM MP_CRYPTO_RSA_WMOD
#define _E 65537u

static uint32_t _p[_WP], _q[_WP], _dp[_WP], _dq[_WP], _qinv[_WP];
static uint32_t _m1[_WP], _m2[_WP], _h[_WP], _tt[_WP];
static uint32_t _n[_WM], _wide[_WM], _acc[_WM], _one[_WM];
static uint8_t _raw[MP_CRYPTO_RSA_MODULUS];
static uint8_t _ss[32], _rej[32];

static void _wipe(void) {
	memset(_p, 0, sizeof(_p));
	memset(_q, 0, sizeof(_q));
	memset(_dp, 0, sizeof(_dp));
	memset(_dq, 0, sizeof(_dq));
	memset(_qinv, 0, sizeof(_qinv));
	memset(_m1, 0, sizeof(_m1));
	memset(_m2, 0, sizeof(_m2));
	memset(_h, 0, sizeof(_h));
	memset(_tt, 0, sizeof(_tt));
	memset(_n, 0, sizeof(_n));
	memset(_wide, 0, sizeof(_wide));
	memset(_acc, 0, sizeof(_acc));
	memset(_one, 0, sizeof(_one));
	memset(_raw, 0, sizeof(_raw));
	memset(_ss, 0, sizeof(_ss));
	memset(_rej, 0, sizeof(_rej));
	mp_crypto_bn_clean();
}

static void _load_prime(uint32_t *a, const uint8_t *be) {
	mp_crypto_bn_from_be(a, _WP, be, MP_CRYPTO_RSA_PRIME);
}

/** 1 when a >= b. Every word is read. The difference replaces a comparison. */
static uint32_t _ge(const uint32_t *a, const uint32_t *b, int n) {
	uint32_t gt = 0, lt = 0;
	int i;

	for(i = n - 1; i >= 0; i--) {
		uint32_t open = 0u - (1u ^ (gt | lt));
		uint32_t a_gt = (uint32_t)(((uint64_t)b[i] - (uint64_t)a[i]) >> 63) & open;
		uint32_t a_lt = (uint32_t)(((uint64_t)a[i] - (uint64_t)b[i]) >> 63) & open;

		gt |= a_gt;
		lt |= a_lt;
	}
	return(1u ^ lt);
}

static uint32_t _is_zero(const uint32_t *a, int n) {
	uint32_t any = 0;
	int i;

	for(i = 0; i < n; i++)
		any |= a[i];
	return((uint32_t)(((any | (0u - any)) >> 31) ^ 1u));
}

/** A 0/1 the compiler must not inspect. GCC turns a known mask into a branch. */
static uint32_t _opaque(uint32_t x) {
	__asm__ volatile("" : "+r"(x));
	return(x);
}

static void _cmov(uint8_t *dst, const uint8_t *src, unsigned n, uint32_t bit) {
	unsigned i;
	uint8_t m = (uint8_t)-(uint8_t)_opaque(bit);

	for(i = 0; i < n; i++)
		dst[i] ^= (uint8_t)(m & (dst[i] ^ src[i]));
}

static void _kdf(uint8_t ss[32], const uint8_t *z) {
	mp_crypto_shake256(ss, 32, z, MP_CRYPTO_RSA_MODULUS);
}

/**
 * z = c^d mod n through the CRT. Always writes one modulus.
 * Returns 1 when a prime is even or the representative falls outside [0, n).
 */
static uint32_t _crt(uint8_t *z, const uint8_t *priv, const uint8_t *ct) {
	const uint8_t *key = priv;
	uint32_t carry, bad = 0;
	int i;

	mp_crypto_bn_from_be(_acc, _WM, ct, MP_CRYPTO_RSA_MODULUS);
	mp_crypto_bn_from_be(_n, _WM, key, MP_CRYPTO_RSA_MODULUS);
	_load_prime(_p, key + MP_CRYPTO_RSA_OFF_P);
	_load_prime(_q, key + MP_CRYPTO_RSA_OFF_Q);
	_load_prime(_dp, key + MP_CRYPTO_RSA_OFF_DP);
	_load_prime(_dq, key + MP_CRYPTO_RSA_OFF_DQ);
	_load_prime(_qinv, key + MP_CRYPTO_RSA_OFF_QINV);
	mp_crypto_bn_mod(_m1, _acc, _WM, _p, _WP);
	mp_crypto_bn_mod(_m2, _acc, _WM, _q, _WP);
	bad |= (uint32_t)(mp_crypto_bn_modexp(_m1, _m1, _dp, _WP, _p, _WP) != TRUE);
	bad |= (uint32_t)(mp_crypto_bn_modexp(_m2, _m2, _dq, _WP, _q, _WP) != TRUE);
	mp_crypto_bn_mod(_tt, _m2, _WP, _p, _WP);
	mp_crypto_bn_sub_mod(_h, _m1, _tt, _p, _WP);
	mp_crypto_bn_mul(_wide, _h, _qinv, _WP);
	mp_crypto_bn_mod(_h, _wide, _WM, _p, _WP);
	mp_crypto_bn_mul(_wide, _h, _q, _WP);
	carry = mp_crypto_bn_add(_wide, _wide, _m2, _WP);
	for(i = _WP; i < _WM; i++) {
		uint64_t s = (uint64_t)_wide[i] + carry;

		_wide[i] = (uint32_t)s;
		carry = (uint32_t)(s >> 32);
	}
	bad |= carry | _ge(_wide, _n, _WM);
	mp_crypto_bn_zero(_one, _WM);
	_one[0] = 1;
	mp_crypto_bn_cmov(_wide, _one, _WM, bad);
	mp_crypto_bn_to_be(z, MP_CRYPTO_RSA_MODULUS, _wide, _WM);
	return(bad);
}

/**
 * @brief Build an RSA key pair
 * @param[in] seed At least 32 bytes
 * @param[in] seedlen Length of seed
 * @param[out] priv The RSA private layout
 * @param[out] pub The RSA public layout
 * @return FALSE when the seed is short or no prime shows up
 */
mp_ret_t mp_crypto_rsakem_keypair(const void *seed, unsigned int seedlen, void *priv, void *pub) {
	return(mp_crypto_rsa_keypair(seed, seedlen, priv, pub));
}

/**
 * @brief Encapsulate
 * @param[in] pub n || e
 * @param[in] coins z, one modulus of big-endian bytes, in [1, n)
 * @param[in] coinslen Length of coins
 * @param[out] ss 32-byte shared secret
 * @param[out] ct z^e mod n
 * @return The modulus length, or 0 when z is out of range
 */
unsigned int mp_crypto_rsakem_encaps(const void *pub, const void *coins, unsigned int coinslen, uint8_t *ss, uint8_t *ct) {
	const uint8_t *key = pub;
	uint32_t e, bad;

	if(!pub || !coins || !ss || !ct || coinslen != MP_CRYPTO_RSA_MODULUS)
		return(0);
	mp_crypto_bn_from_be(_n, _WM, key, MP_CRYPTO_RSA_MODULUS);
	mp_crypto_bn_from_be(_acc, _WM, coins, MP_CRYPTO_RSA_MODULUS);
	bad = _is_zero(_acc, _WM) | _ge(_acc, _n, _WM);
	mp_crypto_bn_zero(_one, _WM);
	_one[0] = 2;
	mp_crypto_bn_cmov(_acc, _one, _WM, bad);
	e = _E;
	bad |= (uint32_t)(mp_crypto_bn_modexp(_wide, _acc, &e, 1, _n, _WM) != TRUE);
	mp_crypto_bn_to_be(ct, MP_CRYPTO_RSA_MODULUS, _wide, _WM);
	_kdf(ss, coins);
	memset(_raw, 0, sizeof(_raw));
	_cmov(ct, _raw, MP_CRYPTO_RSA_MODULUS, bad);
	memset(_rej, 0, sizeof(_rej));
	_cmov(ss, _rej, 32, bad);
	_wipe();
	return(MP_CRYPTO_RSA_MODULUS & (0u - (1u ^ bad)));
}

/**
 * @brief Decapsulate
 * @param[in] priv The RSA private layout
 * @param[in] ct One modulus
 * @param[in] ctlen Length of ct
 * @param[out] ss 32-byte shared secret
 * @return YES when ctlen is one modulus
 */
mp_bool_t mp_crypto_rsakem_decaps(const void *priv, const void *ct, unsigned int ctlen, uint8_t *ss) {
	const uint8_t *c = ct;
	uint32_t bad;

	if(!priv || !ct || !ss || ctlen != MP_CRYPTO_RSA_MODULUS)
		return(NO);
	mp_crypto_bn_from_be(_n, _WM, priv, MP_CRYPTO_RSA_MODULUS);
	mp_crypto_bn_from_be(_acc, _WM, c, MP_CRYPTO_RSA_MODULUS);
	bad = _is_zero(_acc, _WM) | _ge(_acc, _n, _WM);
	mp_crypto_bn_zero(_one, _WM);
	_one[0] = 2;
	mp_crypto_bn_cmov(_acc, _one, _WM, bad);
	mp_crypto_bn_to_be(_raw, MP_CRYPTO_RSA_MODULUS, _acc, _WM);
	bad |= _crt(_raw, priv, _raw);
	_kdf(_ss, _raw);
	mp_crypto_shake256(_rej, 32, c, MP_CRYPTO_RSA_MODULUS);
	_cmov(_ss, _rej, 32, bad);
	memcpy(ss, _ss, 32);
	_wipe();
	return(YES);
}

/**
 * @brief Decapsulate with a fuse slot
 * @param[in] fuse Slot of role MP_CRYPTO_FUSE_RSAKEM
 * @param[in] wrapped Blob the arch defines
 * @param[in] wrappedlen Byte length of wrapped
 * @param[in] ct Ciphertext
 * @param[in] ctlen Length of ct
 * @param[out] ss 32-byte shared secret
 * @return NO when this series has no RSA-KEM accelerator
 */
mp_bool_t mp_crypto_rsakem_decaps_fuse(const mp_crypto_fuse_t *fuse, const void *wrapped, unsigned int wrappedlen, const void *ct, unsigned int ctlen, uint8_t *ss) {
	static mp_bool_t said;

	if(!ss || (ctlen && !ct))
		return(NO);
#ifdef MP_ARCH_RSAKEM
	mp_bool_t ok;

#ifdef SUPPORT_COMMON_CRYPTO_FUSE
	if(mp_crypto_fuse_check(fuse, MP_CRYPTO_FUSE_RSAKEM) != YES)
		return(NO);
#endif
	ok = mp_arch_rsakem_decaps_fuse(fuse, wrapped, wrappedlen, ct, ctlen, ss);
	if(ok != YES && said == NO) {
		mp_printk("RSA-KEM: the accelerator refused the fuse");
		said = YES;
	}
	return(ok);
#else
	(void)fuse;
	(void)wrapped;
	(void)wrappedlen;
	if(said == NO) {
		mp_printk("RSA-KEM: this build has no fuse");
		said = YES;
	}
	return(NO);
#endif
}

/**@}*/

#endif
