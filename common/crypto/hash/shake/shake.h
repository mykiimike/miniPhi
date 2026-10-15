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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifdef SUPPORT_COMMON_CRYPTO_HASH_SHAKE

#ifndef _HAVE_MP_COMMON_SHAKE_H
	#define _HAVE_MP_COMMON_SHAKE_H

	#include <stdint.h>

	/**
	 * @defgroup mpCommonShake
	 * @{
	 */

	/* Keccak-f[1600] sponge, the SHA-3 family. The context owns a 200-byte
	 * state buffer; the caller owns the context. Four modes share one core:
	 *   SHA3-256 : rate 136, suffix 0x06, digest 32
	 *   SHA3-512 : rate  72, suffix 0x06, digest 64
	 *   SHAKE-128: rate 168, suffix 0x1f, XOF
	 *   SHAKE-256: rate 136, suffix 0x1f, XOF
	 * Constant time: the permutation is a fixed function of the full state,
	 * so no branch or memory access depends on a secret. */

	#define MP_SHAKE_STATE 200   /* 25 lanes * 8 bytes */

	typedef struct {
		uint64_t a[25];          /* sponge state, lanes x + 5*y */
		uint8_t buf[MP_SHAKE_STATE];
		unsigned int rate;       /* absorb/squeeze rate in bytes */
		unsigned int idx;        /* bytes pending in buf */
		uint8_t suffix;         /* domain suffix (0x06 SHA3, 0x1f SHAKE) */
		uint8_t squeezing;      /* 1 once the pad has been applied */
		uint8_t firstperm;      /* 1: next squeeze must skip the permutation */
	} mp_keccak_t;

	/* generic sponge: rate in bytes, suffix byte. */
	void mp_keccak_init(mp_keccak_t *k, unsigned int rate, uint8_t suffix);
	void mp_keccak_absorb(mp_keccak_t *k, const void *data, unsigned int len);
	void mp_keccak_pad(mp_keccak_t *k);
	void mp_keccak_squeeze(mp_keccak_t *k, uint8_t *out, unsigned int len);

	/* SHA3-256: fixed 32-byte digest. */
	typedef mp_keccak_t mp_sha3_256_t;
	void mp_sha3_256_init(mp_sha3_256_t *c);
	void mp_sha3_256_update(mp_sha3_256_t *c, const void *data, unsigned int len);
	void mp_sha3_256_final(mp_sha3_256_t *c, uint8_t *digest);
	void mp_sha3_256(const void *in, unsigned int len, uint8_t *out);

	/* SHA3-512: fixed 64-byte digest. */
	typedef mp_keccak_t mp_sha3_512_t;
	void mp_sha3_512_init(mp_sha3_512_t *c);
	void mp_sha3_512_update(mp_sha3_512_t *c, const void *data, unsigned int len);
	void mp_sha3_512_final(mp_sha3_512_t *c, uint8_t *digest);
	void mp_sha3_512(const void *in, unsigned int len, uint8_t *out);

	/* SHAKE-128: extendable output. Absorb, then squeeze any length. */
	typedef mp_keccak_t mp_shake128_t;
	void mp_shake128_init(mp_shake128_t *c);
	void mp_shake128_absorb(mp_shake128_t *c, const void *data, unsigned int len);
	void mp_shake128_finalize(mp_shake128_t *c);
	void mp_shake128_squeeze(mp_shake128_t *c, uint8_t *out, unsigned int len);
	void mp_shake128(uint8_t *out, unsigned int outlen, const void *in, unsigned int inlen);

	/* SHAKE-256: extendable output. Absorb, then squeeze any length. */
	typedef mp_keccak_t mp_shake256_t;
	void mp_shake256_init(mp_shake256_t *c);
	void mp_shake256_absorb(mp_shake256_t *c, const void *data, unsigned int len);
	void mp_shake256_finalize(mp_shake256_t *c);
	void mp_shake256_squeeze(mp_shake256_t *c, uint8_t *out, unsigned int len);
	void mp_shake256(uint8_t *out, unsigned int outlen, const void *in, unsigned int inlen);

	/** @}*/
#endif
#endif
