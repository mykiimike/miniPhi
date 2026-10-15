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

#ifdef SUPPORT_COMMON_CRYPTO_BN

/**
@defgroup mpCommonBn Bignum

@ingroup mpCommon

@brief Unsigned words for the signature algorithms

Little-endian 32-bit words, at most MP_BN_MAX of them. Multiplication is
schoolbook, division is restoring, and the exponentiation is Montgomery
(CIOS) so a 2048-bit power does not divide. The scratch below is the
reason the calls are not reentrant: the signature code runs from a task,
and the scheduler does not preempt it.

@{
*/

static uint32_t _div_q[MP_BN_MAX * 2];
static uint32_t _div_num[MP_BN_MAX * 2];
static uint32_t _div_rem[MP_BN_MAX + 2];
static uint32_t _div_den[MP_BN_MAX + 2];
/** constant-time subtract target for the restoring division; never the quotient */
static uint32_t _div_sub[MP_BN_MAX + 2];
/** shared by exp, inv and the prime test; they do not nest */
static uint32_t _w[8][MP_BN_MAX];
static uint32_t _wide[MP_BN_MAX * 2];

void mp_bn_zero(uint32_t *a, int n) {
	memset(a, 0, (unsigned int)n * sizeof(uint32_t));
}

void mp_bn_copy(uint32_t *dst, const uint32_t *src, int n) {
	memcpy(dst, src, (unsigned int)n * sizeof(uint32_t));
}

void mp_bn_clean(void) {
	memset(_div_q, 0, sizeof(_div_q));
	memset(_div_num, 0, sizeof(_div_num));
	memset(_div_rem, 0, sizeof(_div_rem));
	memset(_div_den, 0, sizeof(_div_den));
	memset(_div_sub, 0, sizeof(_div_sub));
	memset(_w, 0, sizeof(_w));
	memset(_wide, 0, sizeof(_wide));
}

int mp_bn_cmp(const uint32_t *a, const uint32_t *b, int n) {
	int i;

	for(i = n - 1; i >= 0; i--) {
		if(a[i] > b[i])
			return(1);
		if(a[i] < b[i])
			return(-1);
	}
	return(0);
}

int mp_bn_is_zero(const uint32_t *a, int n) {
	int i;

	for(i = 0; i < n; i++)
		if(a[i])
			return(0);
	return(1);
}

uint32_t mp_bn_add(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	uint64_t c = 0;
	int i;

	for(i = 0; i < n; i++) {
		c += (uint64_t)a[i] + b[i];
		r[i] = (uint32_t)c;
		c >>= 32;
	}
	return((uint32_t)c);
}

uint32_t mp_bn_sub(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	uint64_t br = 0;
	int i;

	for(i = 0; i < n; i++) {
		uint64_t v = (uint64_t)a[i] - b[i] - br;

		r[i] = (uint32_t)v;
		br = (v >> 32) ? 1 : 0;
	}
	return((uint32_t)br);
}

uint32_t mp_bn_shl1(uint32_t *r, const uint32_t *a, int n) {
	uint32_t c = 0;
	int i;

	for(i = 0; i < n; i++) {
		uint32_t next = a[i] >> 31;

		r[i] = (a[i] << 1) | c;
		c = next;
	}
	return(c);
}

uint32_t mp_bn_shr1(uint32_t *r, const uint32_t *a, int n) {
	uint32_t c = 0;
	int i;

	for(i = n - 1; i >= 0; i--) {
		uint32_t next = a[i] & 1;

		r[i] = (a[i] >> 1) | (c << 31);
		c = next;
	}
	return(c);
}

void mp_bn_cmov(uint32_t *dst, const uint32_t *src, int n, uint32_t bit) {
	uint32_t mask = 0u - (bit & 1u);
	int i;

	for(i = 0; i < n; i++)
		dst[i] = (dst[i] & ~mask) | (src[i] & mask);
}

void mp_bn_from_be(uint32_t *a, int n, const uint8_t *be, unsigned int len) {
	unsigned int i;

	mp_bn_zero(a, n);
	for(i = 0; i < len && i < (unsigned int)n * 4; i++) {
		unsigned int at = len - 1 - i;

		a[i >> 2] |= (uint32_t)be[at] << (8 * (i & 3));
	}
}

void mp_bn_to_be(uint8_t *be, unsigned int len, const uint32_t *a, int n) {
	unsigned int i;

	for(i = 0; i < len; i++) {
		unsigned int at = len - 1 - i;
		unsigned int word = i >> 2;

		if(word < (unsigned int)n)
			be[at] = (uint8_t)(a[word] >> (8 * (i & 3)));
		else
			be[at] = 0;
	}
}

void mp_bn_from_le(uint32_t *a, int n, const uint8_t *le, unsigned int len) {
	unsigned int i;

	mp_bn_zero(a, n);
	for(i = 0; i < len && i < (unsigned int)n * 4; i++)
		a[i >> 2] |= (uint32_t)le[i] << (8 * (i & 3));
}

void mp_bn_to_le(uint8_t *le, unsigned int len, const uint32_t *a, int n) {
	unsigned int i;

	for(i = 0; i < len; i++) {
		unsigned int word = i >> 2;

		if(word < (unsigned int)n)
			le[i] = (uint8_t)(a[word] >> (8 * (i & 3)));
		else
			le[i] = 0;
	}
}

/* lo + term + *carry, without wrapping the uint64 sum. *carry comes back under 2^34 for a 32-bit product. */
static void _bn_acc(uint32_t *lo, uint64_t *carry, uint64_t term) {
	uint64_t s = (uint64_t)(*lo) + term;
	uint64_t ov = (s < term);

	s += *carry;
	ov += (s < *carry);
	*lo = (uint32_t)s;
	*carry = (s >> 32) + (ov << 32);
}

void mp_bn_mul(uint32_t *r, const uint32_t *a, const uint32_t *b, int n) {
	int i, j, k;

	mp_bn_zero(r, n * 2);
	for(i = 0; i < n; i++) {
		uint64_t c = 0;

		for(j = 0; j < n; j++)
			_bn_acc(&r[i + j], &c, (uint64_t)a[j] * b[i]);
		for(k = i + n; c && k < n * 2; k++) {
			uint64_t s = (uint64_t)r[k] + (uint32_t)c;

			r[k] = (uint32_t)s;
			c = (c >> 32) + (s >> 32);
		}
	}
}

/* 1 when a >= b. A uint32 difference with its top bit set is not a borrow. */
static uint32_t _bn_ge(const uint32_t *a, const uint32_t *b, int n) {
	uint32_t gt = 0;
	uint32_t lt = 0;
	int i;

	for(i = n - 1; i >= 0; i--) {
		uint32_t decided = gt | lt;
		uint32_t open = 0u - (1u ^ decided);
		uint32_t ag = (uint32_t)(((uint64_t)b[i] - a[i]) >> 63) & open;
		uint32_t al = (uint32_t)(((uint64_t)a[i] - b[i]) >> 63) & open;

		gt |= ag;
		lt |= al;
	}
	return(1u ^ lt);
}

void mp_bn_mod(uint32_t *r, const uint32_t *num, int nn, const uint32_t *den, int n) {
	int bits = nn * 32;
	int i;

	mp_bn_zero(_div_rem, n + 1);
	mp_bn_zero(_div_den, n + 1);
	mp_bn_copy(_div_den, den, n);
	mp_bn_copy(_div_num, num, nn);
	for(i = bits - 1; i >= 0; i--) {
		uint32_t top = mp_bn_shl1(_div_rem, _div_rem, n + 1);
		uint32_t ge;

		if((_div_num[i >> 5] >> (i & 31)) & 1u)
			_div_rem[0] |= 1u;
		/* rem had n+1 words and was < den before the shift, so the bit that
		 * fell off is 0; top stays 0 and the compare is n+1 words against den */
		(void)top;
		ge = _bn_ge(_div_rem, _div_den, n + 1);
		/* constant-time conditional subtract: rem = ge ? rem - den : rem.
		 * The branch is on ge, which the quotient bit is anyway, but the
		 * subtract runs every iteration and the result is selected by cmov,
		 * so the work does not depend on the secret. */
		mp_bn_sub(_div_sub, _div_rem, _div_den, n + 1);
		mp_bn_cmov(_div_rem, _div_sub, n + 1, ge);
		if((i >> 5) < MP_BN_MAX * 2)
			_div_q[i >> 5] |= (uint32_t)ge << (i & 31);
	}
	mp_bn_copy(r, _div_rem, n);
	mp_bn_zero(_div_q, MP_BN_MAX * 2);
}

void mp_bn_add_mod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *m, int n) {
	/* r = a+b, then subtract m when the sum reached or passed it. Constant time:
	 * the subtract always runs and the result is selected by cmov; _bn_ge has no
	 * early exit, unlike mp_bn_cmp. */
	uint32_t c = mp_bn_add(_w[7], a, b, n);
	uint32_t ge;

	mp_bn_sub(_w[6], _w[7], m, n);
	ge = (c != 0) | _bn_ge(_w[7], m, n);
	mp_bn_cmov(_w[7], _w[6], n, ge);
	mp_bn_copy(r, _w[7], n);
}

void mp_bn_sub_mod(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *m, int n) {
	uint32_t br = mp_bn_sub(_w[7], a, b, n);

	mp_bn_add(_w[6], _w[7], m, n);
	mp_bn_cmov(_w[7], _w[6], n, br);
	mp_bn_copy(r, _w[7], n);
}

uint32_t mp_bn_mod_u32(const uint32_t *a, int n, uint32_t m) {
	uint64_t r = 0;
	int i;

	if(!m)
		return(0);
	for(i = n - 1; i >= 0; i--)
		r = ((r << 32) | a[i]) % m;
	return((uint32_t)r);
}

static uint32_t _bn_n0(uint32_t odd) {
	uint32_t x = odd;
	int i;

	/* Newton: five rounds give the inverse mod 2^32, then negate it */
	for(i = 0; i < 5; i++)
		x *= 2u - odd * x;
	return(0u - x);
}

/* CIOS. r may alias a or b; both are read in full before r is written. m is odd. */
static void _bn_mont(uint32_t *r, const uint32_t *a, const uint32_t *b, const uint32_t *m, int n, uint32_t n0) {
	uint32_t *t = _div_rem;
	int i, j;

	mp_bn_zero(t, n + 2);
	for(i = 0; i < n; i++) {
		uint64_t c = 0;
		uint64_t s;
		uint32_t mprime;

		for(j = 0; j < n; j++)
			_bn_acc(&t[j], &c, (uint64_t)a[j] * b[i]);
		s = (uint64_t)t[n] + c;
		t[n] = (uint32_t)s;
		t[n + 1] = (uint32_t)(s >> 32);

		c = 0;
		mprime = t[0] * n0;
		for(j = 0; j < n; j++)
			_bn_acc(&t[j], &c, (uint64_t)mprime * m[j]);
		s = (uint64_t)t[n] + c;
		t[n] = (uint32_t)s;
		t[n + 1] += (uint32_t)(s >> 32);

		for(j = 0; j <= n; j++)
			t[j] = t[j + 1];
		t[n + 1] = 0;
	}
	/* CIOS leaves a value under 2*m; a second subtract covers a slack word */
	{
		int pass;

		mp_bn_zero(_div_den, n + 1);
		mp_bn_copy(_div_den, m, n);
		for(pass = 0; pass < 2; pass++) {
			uint32_t br = 0;
			uint32_t ge;
			int k;

			for(k = 0; k < n + 1; k++) {
				uint64_t d = (uint64_t)t[k] - _div_den[k] - br;

				_div_q[k] = (uint32_t)d;
				br = (d >> 32) ? 1 : 0;
			}
			ge = 1u ^ br;
			mp_bn_cmov(t, _div_q, n + 1, ge);
		}
	}
	mp_bn_copy(r, t, n);
}

/* r = 2*a mod m, a < m, constant time. Uses _w[6] and _w[7]. */
static void _bn_dbl_mod(uint32_t *a, const uint32_t *m, int n) {
	uint32_t c = mp_bn_shl1(_w[6], a, n);
	uint32_t br = 0;
	uint32_t ge;
	int i;

	for(i = 0; i < n; i++) {
		uint64_t d = (uint64_t)_w[6][i] - m[i] - br;

		_w[7][i] = (uint32_t)d;
		br = (d >> 32) ? 1 : 0;
	}
	/* a < m, so 2*a < 2*m and the reduced value fits in n words */
	ge = (c != 0) || (br == 0);
	mp_bn_cmov(_w[6], _w[7], n, ge);
	mp_bn_copy(a, _w[6], n);
}

mp_ret_t mp_bn_modexp(uint32_t *r, const uint32_t *base, const uint32_t *exp, int en, const uint32_t *m, int n) {
	uint32_t n0;
	int i;

	if(n <= 0 || n > MP_BN_MAX || en <= 0 || en > MP_BN_MAX || (m[0] & 1u) == 0)
		return(FALSE);
	mp_bn_copy(_w[0], m, n);
	mp_bn_zero(_w[1], n);
	if(en < n)
		mp_bn_copy(_w[1], exp, en);
	else
		mp_bn_copy(_w[1], exp, n);
	/* a width above n would need a wider exponent buffer; the signature
	 * algorithms never ask for one, and the high words would be ignored */
	if(en > n)
		return(FALSE);
	mp_bn_mod(_w[2], base, n, _w[0], n);

	mp_bn_zero(_w[3], n);
	_w[3][0] = 1;
	for(i = 0; i < 2 * 32 * n; i++)
		_bn_dbl_mod(_w[3], _w[0], n);

	n0 = _bn_n0(_w[0][0]);
	mp_bn_zero(_w[5], n);
	_w[5][0] = 1;
	_bn_mont(_w[4], _w[5], _w[3], _w[0], n, n0);
	_bn_mont(_w[5], _w[2], _w[3], _w[0], n, n0);

	for(i = en * 32 - 1; i >= 0; i--) {
		uint32_t bit = (_w[1][i >> 5] >> (i & 31)) & 1u;

		_bn_mont(_w[4], _w[4], _w[4], _w[0], n, n0);
		_bn_mont(_w[6], _w[4], _w[5], _w[0], n, n0);
		mp_bn_cmov(_w[4], _w[6], n, bit);
	}
	mp_bn_zero(_w[6], n);
	_w[6][0] = 1;
	_bn_mont(r, _w[4], _w[6], _w[0], n, n0);
	return(TRUE);
}

/**
 * Quotient in _div_q (nn words of room, the useful part is n) and remainder
 * in rem. num has nn words, den has n. Unlike mp_bn_mod this keeps the quotient.
 */
static void _bn_div(uint32_t *rem, const uint32_t *num, int nn, const uint32_t *den, int n) {
	int bits = nn * 32;
	int i;

	mp_bn_zero(_div_q, MP_BN_MAX * 2);
	mp_bn_zero(_div_rem, n + 1);
	mp_bn_zero(_div_den, n + 1);
	mp_bn_copy(_div_den, den, n);
	mp_bn_copy(_div_num, num, nn);
	for(i = bits - 1; i >= 0; i--) {
		uint32_t ge;

		mp_bn_shl1(_div_rem, _div_rem, n + 1);
		if((_div_num[i >> 5] >> (i & 31)) & 1u)
			_div_rem[0] |= 1u;
		ge = _bn_ge(_div_rem, _div_den, n + 1);
		mp_bn_sub(_div_sub, _div_rem, _div_den, n + 1);
		mp_bn_cmov(_div_rem, _div_sub, n + 1, ge);
		_div_q[i >> 5] |= (uint32_t)ge << (i & 31);
	}
	mp_bn_copy(rem, _div_rem, n);
}

mp_ret_t mp_bn_modinv(uint32_t *r, const uint32_t *a, const uint32_t *m, int n) {
	/* _w: u, v, s0, s1, scratch, saved remainder. s0*a ≡ u and s1*a ≡ v, mod m. */
	if(n <= 0 || n > MP_BN_MAX || mp_bn_is_zero(m, n) || mp_bn_is_zero(a, n))
		return(FALSE);
	mp_bn_copy(_w[0], m, n);
	mp_bn_mod(_w[1], a, n, m, n);
	mp_bn_zero(_w[2], n);
	mp_bn_zero(_w[3], n);
	_w[3][0] = 1;
	if(mp_bn_is_zero(_w[1], n))
		return(FALSE);
	while(!mp_bn_is_zero(_w[1], n)) {
		/* rem in _w[5], q in _div_q; the product and the next mod reuse the div scratch */
		_bn_div(_w[5], _w[0], n, _w[1], n);
		mp_bn_mul(_wide, _div_q, _w[3], n);
		mp_bn_mod(_w[4], _wide, n * 2, m, n);
		mp_bn_sub_mod(_w[4], _w[2], _w[4], m, n);
		mp_bn_copy(_w[0], _w[1], n);
		mp_bn_copy(_w[2], _w[3], n);
		mp_bn_copy(_w[1], _w[5], n);
		mp_bn_copy(_w[3], _w[4], n);
	}
	if(_w[0][0] != 1 || (n > 1 && !mp_bn_is_zero(_w[0] + 1, n - 1)))
		return(FALSE);
	mp_bn_copy(r, _w[2], n);
	return(TRUE);
}

/* nm1, d and the witness bound outlive mp_bn_modexp, which reuses _w and the div scratch */
static uint32_t _nm1[MP_BN_PRIME_MAX];
static uint32_t _odd[MP_BN_PRIME_MAX];
static uint32_t _bound[MP_BN_PRIME_MAX];
static uint32_t _witness[MP_BN_PRIME_MAX];
static uint32_t _x[MP_BN_PRIME_MAX];

mp_bool_t mp_bn_prime(const uint32_t *a, int n, int rounds, mp_bn_fill_t fill, void *user) {
	int s, round;
	uint32_t two[1];

	if(n <= 0 || n > MP_BN_PRIME_MAX || !fill || (a[0] & 1u) == 0)
		return(NO);
	if(n == 1 && a[0] < 3)
		return(NO);
	mp_bn_zero(_w[7], n);
	_w[7][0] = 1;
	if(mp_bn_sub(_nm1, a, _w[7], n))
		return(NO);
	mp_bn_copy(_odd, _nm1, n);
	s = 0;
	while((_odd[0] & 1u) == 0) {
		mp_bn_shr1(_odd, _odd, n);
		s++;
	}
	/* witnesses are drawn in [2, a - 2], i.e. 2 + a number mod (a - 3) */
	_w[7][0] = 2;
	if(mp_bn_sub(_bound, _nm1, _w[7], n))
		return(n == 1 && a[0] == 3 ? YES : NO);
	two[0] = 2;
	for(round = 0; round < rounds; round++) {
		uint8_t raw[MP_BN_PRIME_MAX * 4];
		int r;
		int matched;

		fill(user, raw, (unsigned int)n * 4);
		mp_bn_from_le(_witness, n, raw, (unsigned int)n * 4);
		memset(raw, 0, sizeof(raw));
		mp_bn_mod(_witness, _witness, n, _bound, n);
		mp_bn_zero(_w[7], n);
		_w[7][0] = 2;
		mp_bn_add(_witness, _witness, _w[7], n);
		if(mp_bn_modexp(_x, _witness, _odd, n, a, n) != TRUE)
			return(NO);
		if((_x[0] == 1 && mp_bn_is_zero(_x + 1, n - 1)) || mp_bn_cmp(_x, _nm1, n) == 0)
			continue;
		matched = 0;
		for(r = 1; r < s; r++) {
			if(mp_bn_modexp(_x, _x, two, 1, a, n) != TRUE)
				return(NO);
			if(mp_bn_cmp(_x, _nm1, n) == 0) {
				matched = 1;
				break;
			}
			if(_x[0] == 1 && mp_bn_is_zero(_x + 1, n - 1))
				return(NO);
		}
		if(!matched)
			return(NO);
	}
	return(YES);
}

/**@}*/

#endif
