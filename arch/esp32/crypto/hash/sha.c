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
@defgroup mpArchESP32Sha SHA

@ingroup mpArchESP32

Typical SHA on the C6. One engine serves SHA-1 and SHA-256. A context
keeps the chaining value, so a second digest restores it and continues.
The first byte of a word sits in the low 8 bits, which is the memory
image of the message and of the digest.

@{
*/

#define _MODE   0x00UL
#define _START  0x10UL
#define _CONT   0x14UL
#define _BUSY   0x18UL
#define _H      0x40UL
#define _M      0x80UL

#define _SHA1   0
#define _SHA256 2

typedef struct {
	uint32_t hash[8];
	uint64_t bytes;
	uint8_t block[64];
	uint8_t used;
	uint8_t started;
} _sha_t;

static void _on(void) {
	static int up;

	if(up)
		return;
	/* Clock on, then a reset pulse. DS and HMAC share this core and hold
	 * it while their own reset bit stays set. */
	c6_write(C6_PCR + 0xCCUL, 1UL | (1UL << 1));
	c6_write(C6_PCR + 0xCCUL, 1UL);
	c6_clr(C6_PCR + 0xE0UL, 1UL << 1);
	c6_clr(C6_PCR + 0xE4UL, 1UL << 1);
	up = 1;
}

static int _idle(void) {
	unsigned long n;

	for(n = 0; n < 1000000UL; n++) {
		if((c6_read(C6_SHA + _BUSY) & 1UL) == 0)
			return(1);
	}
	return(0);
}

static void _word_write(unsigned long addr, const uint8_t *src) {
	unsigned long word;

	memcpy(&word, src, 4);
	c6_write(addr, word);
}

static void _block(_sha_t *hw, int mode, int words) {
	int i;

	_idle();
	c6_write(C6_SHA + _MODE, (unsigned long)mode);
	if(hw->started) {
		for(i = 0; i < words; i++)
			c6_write(C6_SHA + _H + (unsigned long)i * 4UL, hw->hash[i]);
	}
	for(i = 0; i < 16; i++)
		_word_write(C6_SHA + _M + (unsigned long)i * 4UL, hw->block + i * 4);
	c6_write(C6_SHA + (hw->started ? _CONT : _START), 1UL);
	hw->started = 1;
	_idle();
	for(i = 0; i < words; i++)
		hw->hash[i] = (uint32_t)c6_read(C6_SHA + _H + (unsigned long)i * 4UL);
	hw->used = 0;
}

static void _update(_sha_t *hw, int mode, int words, const uint8_t *data, unsigned int size) {
	unsigned int n;

	while(size) {
		n = 64 - hw->used;
		if(n > size)
			n = size;
		memcpy(hw->block + hw->used, data, n);
		hw->used += n;
		hw->bytes += n;
		data += n;
		size -= n;
		if(hw->used == 64)
			_block(hw, mode, words);
	}
}

static void _final(_sha_t *hw, int mode, int words, uint8_t *digest, unsigned int len) {
	uint64_t bits = hw->bytes << 3;
	int i;

	hw->block[hw->used++] = 0x80;
	if(hw->used > 56) {
		while(hw->used < 64)
			hw->block[hw->used++] = 0;
		_block(hw, mode, words);
	}
	while(hw->used < 56)
		hw->block[hw->used++] = 0;
	for(i = 7; i >= 0; i--)
		hw->block[hw->used++] = (uint8_t)(bits >> (i * 8));
	_block(hw, mode, words);
	memcpy(digest, hw->hash, len);
	memset(hw, 0, sizeof(*hw));
}

#ifdef MP_ARCH_SHA1

typedef char _sha1_fit[(sizeof(mp_arch_sha1_t) == sizeof(_sha_t)) ? 1 : -1];

mp_bool_t mp_arch_sha1_init(mp_arch_sha1_t *hw) {
	_on();
	memset(hw, 0, sizeof(*hw));
	return(YES);
}

void mp_arch_sha1_update(mp_arch_sha1_t *hw, const uint8_t *data, unsigned int size) {
	_update((_sha_t *)hw, _SHA1, 5, data, size);
}

void mp_arch_sha1_final(mp_arch_sha1_t *hw, uint8_t digest[20]) {
	_final((_sha_t *)hw, _SHA1, 5, digest, 20);
}

#endif

#ifdef MP_ARCH_SHA256

typedef char _sha256_fit[(sizeof(mp_arch_sha256_t) == sizeof(_sha_t)) ? 1 : -1];

mp_bool_t mp_arch_sha256_init(mp_arch_sha256_t *hw) {
	_on();
	memset(hw, 0, sizeof(*hw));
	return(YES);
}

void mp_arch_sha256_update(mp_arch_sha256_t *hw, const uint8_t *data, unsigned int size) {
	_update((_sha_t *)hw, _SHA256, 8, data, size);
}

void mp_arch_sha256_final(mp_arch_sha256_t *hw, uint8_t digest[32]) {
	_final((_sha_t *)hw, _SHA256, 8, digest, 32);
}

#endif

/**@}*/

#else
	#error SHA accelerator is implemented for the ESP32-C6
#endif
