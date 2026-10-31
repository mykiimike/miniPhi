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

#ifdef ESP32C6

#include <string.h>

/**
@defgroup mpArchESP32Ds RSA_DS

@ingroup mpArchESP32

One RSA signature. HMAC derives the AES key from the eFuse slot and
the peripheral decrypts the wrapped private key. The plaintext key
does not leave the block. The wrapped bytes are IV, Y, M, r, then the
box, 1216 bytes. X and Z are 3072-bit big-endian integers.

@{
*/

#define _Y    0x000UL
#define _M    0x200UL
#define _RB   0x400UL
#define _BOX  0x600UL
#define _IV   0x630UL
#define _X    0x800UL
#define _Z    0xA00UL
#define _START  0xE00UL
#define _ME     0xE04UL
#define _FINISH 0xE08UL
#define _BUSY   0xE0CUL
#define _WRONG  0xE10UL
#define _CHECK  0xE14UL

#define _PCR_AES  0xC8UL
#define _PCR_RSA  0xD0UL
#define _PCR_DS   0xE0UL

static void _clock(unsigned long off) {
	c6_write(C6_PCR + off, 1UL | 2UL);
	c6_write(C6_PCR + off, 1UL);
}

static int _idle(void) {
	unsigned long n;

	for(n = 0; n < 20000000UL; n++) {
		if((c6_read(C6_DS + _BUSY) & 1UL) == 0)
			return(1);
	}
	return(0);
}

static void _store(unsigned long addr, const uint8_t *bytes, unsigned int nwords) {
	unsigned int i;

	for(i = 0; i < nwords; i++) {
		uint32_t word;

		memcpy(&word, bytes + i * 4, 4);
		c6_write(addr + i * 4UL, word);
	}
}

/** 384-byte big-endian integer, least significant word at the low address */
static void _store_be(unsigned long addr, const uint8_t *be) {
	unsigned int i;

	for(i = 0; i < 96; i++) {
		const uint8_t *p = be + 384 - 4 - i * 4;
		uint32_t word = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];

		c6_write(addr + i * 4UL, word);
	}
}

static void _load_be(uint8_t *be, unsigned long addr) {
	unsigned int i;

	for(i = 0; i < 96; i++) {
		uint32_t word = (uint32_t)c6_read(addr + i * 4UL);
		uint8_t *p = be + 384 - 4 - i * 4;

		p[0] = (uint8_t)(word >> 24);
		p[1] = (uint8_t)(word >> 16);
		p[2] = (uint8_t)(word >> 8);
		p[3] = (uint8_t)word;
	}
}

static void _finish(void) {
	c6_write(C6_DS + _FINISH, 1UL);
	_idle();
	c6_write(C6_HMAC + 0x64UL, 1UL);
}

/**
 * @brief Sign with a wrapped private key
 *
 * @param[in] slot KEY index, purpose RSA_DS or HMAC downstream for both
 * @param[in] wrapped 1216 bytes: IV, Y, M, r, box
 * @param[in] em 384-byte big-endian PKCS#1 block
 * @param[out] sig 384-byte big-endian signature
 * @return 384, or 0 when the block refuses. sig is unchanged on failure
 */
unsigned int mp_arch_rsa_sign_fuse(unsigned int slot, const void *wrapped, const uint8_t *em, uint8_t *sig) {
	const uint8_t *w = wrapped;
	unsigned long check;

	if(!wrapped || !em || !sig)
		return(0);
	_clock(_PCR_AES);
	_clock(_PCR_RSA);
	_clock(_PCR_DS);
	if(mp_arch_hmac_downstream(slot) != YES)
		return(0);
	c6_write(C6_DS + _START, 1UL);
	if(!_idle() || c6_read(C6_DS + _WRONG) != 0)
		goto fail;
	_store(C6_DS + _IV, w, 4);
	_store_be(C6_DS + _X, em);
	_store(C6_DS + _Y, w + 16, 96);
	_store(C6_DS + _M, w + 16 + 384, 96);
	_store(C6_DS + _RB, w + 16 + 384 + 384, 96);
	_store(C6_DS + _BOX, w + 16 + 384 + 384 + 384, 12);
	c6_write(C6_DS + _ME, 1UL);
	if(!_idle())
		goto fail;
	check = c6_read(C6_DS + _CHECK);
	if((check & 1UL) != 0)
		goto fail;
	_load_be(sig, C6_DS + _Z);
	_finish();
	return(384);
fail:
	_finish();
	return(0);
}

/**@}*/

#else
	#error RSA_DS accelerator is implemented for the ESP32-C6
#endif
