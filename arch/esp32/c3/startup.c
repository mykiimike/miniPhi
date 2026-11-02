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

#include <arch/series.h>

/**
@defgroup mpArchESP32C3Boot Boot

@ingroup mpArchESP32

The ROM copies this image into SRAM and jumps to _start. The flash-boot
watchdogs are still armed: they reset the chip if the image never
reaches here, and they reset it again if this code leaves them on.

@{
*/

extern unsigned long _sidata, _sdata, _edata, _sbss, _ebss;

int main(void);

/* TIMG flash-boot mode fires even when the watchdog enable bit is clear. */
static void _wdt_stop(unsigned long base) {
	c3_write(base + 0x64UL, C3_WDT_KEY);
	c3_clr(base + 0x48UL, (1UL << 31) | (1UL << 14));
	c3_write(base + 0x64UL, 0);
}

static void _watchdogs_off(void) {
	_wdt_stop(C3_TIMG0);
	_wdt_stop(C3_TIMG1);

	c3_write(C3_RTC + 0xA8UL, C3_WDT_KEY);
	c3_clr(C3_RTC + 0x90UL, (1UL << 31) | (1UL << 12));
	c3_write(C3_RTC + 0xA8UL, 0);

	c3_write(C3_RTC + 0xB0UL, C3_SWD_KEY);
	c3_set(C3_RTC + 0xACUL, (1UL << 31) | (1UL << 30));
	c3_write(C3_RTC + 0xB0UL, 0);
}

void Reset_Handler(void) {
	unsigned long *src;
	unsigned long *dst;

	/* Bit 2 only. Bits 0 and 1 are the sticky DTR/RTS reset flags: writing
	 * them straps the ROM into download. Until bit 2 is set, opening the
	 * host port asserts RTS and resets the chip. */
	c3_write(C3_USB + 0x4cUL, 1UL << 2);
	_watchdogs_off();

	/* LMA equals VMA, so a library memcpy of this range onto itself
	 * corrupts .data (the gate table loses its terminator). Copy only
	 * when the ROM placed the load image somewhere else. */
	src = &_sidata;
	dst = &_sdata;
	if(src != dst) {
		while(dst < &_edata)
			*dst++ = *src++;
	}

	for(dst = &_sbss; dst < &_ebss; dst++)
		*dst = 0;

	main();

	c3_write(C3_RTC, 1UL << 31);
	for(;;)
		;
}

__attribute__((naked, section(".init"), used, noreturn))
void _start(void) {
	__asm__ volatile (
		"la sp, __stack_top\n"
		/* The ROM leaves machine interrupts enabled. A full write of mie
		 * is illegal on this core; clearing MIE is enough to keep the
		 * ROM handler from running over startup. */
		"csrci mstatus, 8\n"
		"call Reset_Handler\n"
	);
}

/**@}*/
