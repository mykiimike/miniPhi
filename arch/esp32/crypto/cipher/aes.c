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
@defgroup mpArchESP32Aes AES

@ingroup mpArchESP32

Typical AES on the C6, one block at a time. AES-128 and AES-256 only;
a 24-byte key returns NO and the caller keeps the software schedule.
Each block writes its own key, so two contexts can take turns. The
key registers are cleared when the block is done.

@{
*/

#define _TEXT_IN   0x20UL
#define _TEXT_OUT  0x30UL
#define _MODE      0x40UL
#define _TRIGGER   0x48UL
#define _STATE     0x4CUL
#define _DMA       0x90UL

static void _on(void) {
	static int up;

	if(up)
		return;
	/* Clock on, then a reset pulse. DS shares this core and holds it
	 * while its own reset bit stays set. */
	c6_write(C6_PCR + 0xC8UL, 1UL | (1UL << 1));
	c6_write(C6_PCR + 0xC8UL, 1UL);
	c6_clr(C6_PCR + 0xE0UL, 1UL << 1);
	c6_write(C6_AES + _DMA, 0);
	up = 1;
}

static void _word_write(unsigned long addr, const uint8_t *src) {
	unsigned long word;

	memcpy(&word, src, 4);
	c6_write(addr, word);
}

static void _word_read(uint8_t *dst, unsigned long addr) {
	unsigned long word = c6_read(addr);

	memcpy(dst, &word, 4);
}

static void _block(const mp_arch_aes_t *hw, int decrypt, const void *in, unsigned int inlen, uint8_t *out) {
	uint8_t block[16];
	unsigned int n = inlen < 16 ? inlen : 16;
	unsigned int i;
	unsigned long mode;

	memset(block, 0, sizeof(block));
	if(n)
		memcpy(block, in, n);

	/* 16 bytes -> mode 0/4, 32 bytes -> mode 2/6. Decrypt is +4. */
	mode = (decrypt ? 4UL : 0UL) + ((unsigned long)hw->keylen / 8UL - 2UL);
	c6_write(C6_AES + _DMA, 0);
	c6_write(C6_AES + _MODE, mode);
	for(i = 0; i < hw->keylen / 4; i++)
		_word_write(C6_AES + (unsigned long)i * 4UL, hw->key + i * 4);
	for(i = 0; i < 4; i++)
		_word_write(C6_AES + _TEXT_IN + (unsigned long)i * 4UL, block + i * 4);
	c6_write(C6_AES + _TRIGGER, 1UL);
	for(i = 0; i < 1000000UL; i++) {
		if(c6_read(C6_AES + _STATE) == 0)
			break;
	}
	for(i = 0; i < 4; i++)
		_word_read(out + i * 4, C6_AES + _TEXT_OUT + (unsigned long)i * 4UL);
	for(i = 0; i < 8; i++)
		c6_write(C6_AES + (unsigned long)i * 4UL, 0);
	memset(block, 0, sizeof(block));
}

mp_bool_t mp_arch_aes_init(mp_arch_aes_t *hw, const void *key, unsigned int keylen) {
	if(keylen != 16 && keylen != 32)
		return(NO);
	_on();
	memset(hw, 0, sizeof(*hw));
	memcpy(hw->key, key, keylen);
	hw->keylen = keylen;
	return(YES);
}

void mp_arch_aes_update_block(mp_arch_aes_t *hw, const void *in, unsigned int inlen, uint8_t *out) {
	_block(hw, 0, in, inlen, out);
}

void mp_arch_aes_update_block_inv(mp_arch_aes_t *hw, const void *in, unsigned int inlen, uint8_t *out) {
	_block(hw, 1, in, inlen, out);
}

/**@}*/

#else
	#error AES accelerator is implemented for the ESP32-C6
#endif
