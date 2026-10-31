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
@defgroup mpArchESP32Xts XTS

@ingroup mpArchESP32

One manual XTS-AES line for an external-flash address. The key is the
eFuse slot whose purpose is XTS-AES-128. The line is 16, 32 or 64
bytes and the address is aligned to that. The result is copied out
and then destroyed, so the SPI controller does not keep it.

@{
*/

#define _PLAIN  0x300UL
#define _LINE   0x340UL
#define _DEST   0x344UL
#define _ADDR   0x348UL
#define _TRIG   0x34CUL
#define _REL    0x350UL
#define _KILL   0x354UL
#define _STATE  0x358UL

static int _state(unsigned long want) {
	unsigned long n;

	for(n = 0; n < 1000000UL; n++) {
		if((c6_read(C6_SPI0 + _STATE) & 3UL) == want)
			return(1);
	}
	return(0);
}

static void _off(void) {
	if((c6_read(C6_SPI0 + _STATE) & 3UL) == 2UL)
		c6_write(C6_SPI0 + _REL, 1UL);
	if((c6_read(C6_SPI0 + _STATE) & 3UL) == 3UL)
		c6_write(C6_SPI0 + _KILL, 1UL);
	_state(0);
	c6_clr(C6_HP_SYS, 1UL);
}

/**
 * @brief Encrypt one flash line
 *
 * @param[in] slot KEY index, already checked as XTS
 * @param[in] address External flash address, aligned to len
 * @param[in] in Plain line
 * @param[in] len 16, 32 or 64
 * @param[out] out Cipher line
 * @return len, or 0 when the block does not finish. out is unchanged then
 */
unsigned int mp_arch_xts_crypt(unsigned int slot, unsigned long address, const void *in, unsigned int len, void *out) {
	unsigned int linesize;
	unsigned int i;
	uint8_t got[64];

	(void)slot;
	if(!in || !out || (len != 16 && len != 32 && len != 64) || (address & (len - 1)) != 0)
		return(0);
	linesize = len == 16 ? 0 : (len == 32 ? 1 : 2);
	if((c6_read(C6_SPI0 + _STATE) & 3UL) != 0)
		_off();
	c6_set(C6_HP_SYS, 1UL);
	c6_write(C6_SPI0 + _LINE, linesize);
	c6_write(C6_SPI0 + _DEST, 0);
	c6_write(C6_SPI0 + _ADDR, address);
	for(i = 0; i < len / 4; i++) {
		uint32_t word;

		memcpy(&word, (const uint8_t *)in + i * 4, 4);
		c6_write(C6_SPI0 + _PLAIN + i * 4UL, word);
	}
	c6_write(C6_SPI0 + _TRIG, 1UL);
	if(!_state(2)) {
		_off();
		return(0);
	}
	for(i = 0; i < len / 4; i++) {
		uint32_t word = (uint32_t)c6_read(C6_SPI0 + _PLAIN + i * 4UL);

		memcpy(got + i * 4, &word, 4);
	}
	_off();
	if(memcmp(got, in, len) == 0)
		return(0);
	memcpy(out, got, len);
	return(len);
}

/**@}*/

#else
	#error XTS accelerator is implemented for the ESP32-C6
#endif
