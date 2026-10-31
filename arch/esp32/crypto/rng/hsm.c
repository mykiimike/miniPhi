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
@defgroup mpArchESP32Hsm HSM

@ingroup mpArchESP32

The C6 RNG data register. One byte is taken from each read, and the
reads are spaced, because a back-to-back word is not a fresh sample
while the radios are off. An all-zero buffer is refused so a stopped
block does not seed the DRBG.

@{
*/

#define _CLK  0x00UL
#define _DATA 0x08UL
#define _CK   (1UL << 24)

/* SYSTIMER unit 0 is 16 MHz. 256 counts is 16 us between bytes. */
#define _UNIT0_OP (C6_SYSTIMER + 0x04UL)
#define _VALUE_LO (C6_SYSTIMER + 0x44UL)
#define _GAP      256UL

static void _on(void) {
	static int up;

	if(up)
		return;
	c6_set(C6_LPPERI + _CLK, _CK);
	up = 1;
}

static unsigned long _time(void) {
	unsigned long n;

	c6_write(_UNIT0_OP, 1UL << 29);
	c6_write(_UNIT0_OP, 1UL << 30);
	for(n = 0; n < 1000UL && (c6_read(_UNIT0_OP) & (1UL << 29)) == 0; n++)
		;
	return(c6_read(_VALUE_LO));
}

static uint8_t _byte(void) {
	unsigned long start, spins, word;

	word = c6_read(C6_LPPERI + _DATA);
	start = _time();
	spins = 0;
	do {
		word ^= c6_read(C6_LPPERI + _DATA);
		spins++;
	} while(_time() - start < _GAP && spins < 1000UL);
	return((uint8_t)word);
}

mp_bool_t mp_arch_hsm_entropy(void *out, unsigned int size) {
	uint8_t *bytes = out;
	unsigned int i;
	int any = 0;

	_on();
	for(i = 0; i < size; i++) {
		bytes[i] = _byte();
		if(bytes[i])
			any = 1;
	}
	if(!any) {
		memset(out, 0, size);
		return(NO);
	}
	return(YES);
}

/**@}*/

#else
	#error HSM accelerator is implemented for the ESP32-C6
#endif
