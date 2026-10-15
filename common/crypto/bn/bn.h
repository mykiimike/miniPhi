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
#ifdef SUPPORT_COMMON_CRYPTO_BN

#ifndef _HAVE_MP_COMMON_BN_H
	#define _HAVE_MP_COMMON_BN_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonBn
	 * @{
	 */

	/**
	 * Words of 32 bits. With RSA this is the modulus width (MP_RSA_BITS / 32).
	 * Without it, 16 words covers a 512-bit scalar.
	 */
	#ifdef MP_RSA_BITS
		#define MP_BN_MAX (MP_RSA_BITS / 32)
	#else
		#define MP_BN_MAX 16
	#endif
	/** Miller-Rabin is only asked of an RSA prime, half a modulus */
	#if MP_BN_MAX > 32
		#define MP_BN_PRIME_MAX (MP_BN_MAX / 2)
	#else
		#define MP_BN_PRIME_MAX MP_BN_MAX
	#endif

	/** @} */

	/**
	 * Little-endian words: w[0] is the least significant.
	 * n is the word count, at most MP_BN_MAX. A result may not overlap its inputs
	 * unless the call says so. The module keeps one scratch, so these are not reentrant.
	 */

	void mp_bn_zero(uint32_t *a, int n);
	void mp_bn_copy(uint32_t *dst, const uint32_t *src, int n);
	void mp_bn_clean(void);
	/** 1, 0 or -1 */
	int mp_bn_cmp(const uint32_t *a, const uint32_t *b, int n);
	int mp_bn_is_zero(const uint32_t *a, int n);
	/** returns the carry */
	uint32_t mp_bn_add(uint32_t *r, const uint32_t *a, const uint32_t *b, int n);
	/** returns the borrow; the words are the wrapped result either way */
	uint32_t mp_bn_sub(uint32_t *r, const uint32_t *a, const uint32_t *b, int n);
	uint32_t mp_bn_shl1(uint32_t *r, const uint32_t *a, int n);
	/** returns the bit that fell off */
	uint32_t mp_bn_shr1(uint32_t *r, const uint32_t *a, int n);
	/** bit is 0 or 1; copies src over dst when it is 1, in constant time */
	void mp_bn_cmov(uint32_t *dst, const uint32_t *src, int n, uint32_t bit);

	/** big-endian bytes, the last byte least significant; extra high words are 0 */
	void mp_bn_from_be(uint32_t *a, int n, const uint8_t *be, unsigned int len);
	void mp_bn_to_be(uint8_t *be, unsigned int len, const uint32_t *a, int n);
	/** little-endian bytes */
	void mp_bn_from_le(uint32_t *a, int n, const uint8_t *le, unsigned int len);
	void mp_bn_to_le(uint8_t *le, unsigned int len, const uint32_t *a, int n);

	/** r has 2n words and does not overlap a or b */
	void mp_bn_mul(uint32_t *r, const uint32_t *a, const uint32_t *b, int n);
	/**
	 * r = num mod den. num has nn words, den and r have n, den != 0.
	 * r does not overlap num or den.
	 */
	void mp_bn_mod(uint32_t *r, const uint32_t *num, int nn, const uint32_t *den, int n);
	/** a and b both reduced; r = (a + b) mod m */
	void mp_bn_add_mod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *m, int n);
	/** a and b both reduced; r = (a - b) mod m */
	void mp_bn_sub_mod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *m, int n);
	/** a mod m, m fitting in 32 bits */
	uint32_t mp_bn_mod_u32(const uint32_t *a, int n, uint32_t m);

	/**
	 * r = base^exp mod m, m odd, n words. exp has en words.
	 * The time follows en, not the value of exp. r, base and m are n words.
	 * FALSE when m is even or a length is out of range; r is then unchanged.
	 */
	mp_ret_t mp_bn_modexp(uint32_t *r, const uint32_t *base, const uint32_t *exp, int en, const uint32_t *m, int n);
	/**
	 * r = a^{-1} mod m. FALSE when gcd(a, m) is not 1.
	 */
	mp_ret_t mp_bn_modinv(uint32_t *r, const uint32_t *a, const uint32_t *m, int n);

	/**
	 * Miller-Rabin. fill writes len random bytes; rounds witnesses are drawn from it.
	 * FALSE when a is composite, even, or shorter than 2 bits. A prime can still
	 * fail a round only if fill is not random; 8 rounds is the caller's usual ask.
	 */
	typedef void (*mp_bn_fill_t)(void *user, uint8_t *dst, unsigned int len);
	mp_bool_t mp_bn_prime(const uint32_t *a, int n, int rounds, mp_bn_fill_t fill, void *user);

#endif
#endif
