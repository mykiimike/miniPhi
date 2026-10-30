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
@defgroup mpArchESP32C6Boot Boot

@ingroup mpArchESP32

The ROM copies this image into HP SRAM and jumps to _start. The
flash-boot watchdogs are still armed: they reset the chip if the image
never reaches here, and they reset it again if this code leaves them on.

@{
*/

extern unsigned long _sidata, _sdata, _edata, _sbss, _ebss;

int main(void);

/* TIMG flash-boot mode fires even when the watchdog enable bit is clear. */
static void _wdt_stop(unsigned long base) {
	c6_write(base + 0x64UL, C6_WDT_KEY);
	c6_clr(base + 0x48UL, (1UL << 31) | (1UL << 14));
	c6_write(base + 0x64UL, 0);
}

static void _watchdogs_off(void) {
	_wdt_stop(C6_TIMG0);
	_wdt_stop(C6_TIMG1);

	c6_write(C6_LP_WDT + 0x18UL, C6_WDT_KEY);
	c6_clr(C6_LP_WDT, (1UL << 31) | (1UL << 12));
	c6_write(C6_LP_WDT + 0x18UL, 0);

	c6_write(C6_LP_WDT + 0x20UL, C6_WDT_KEY);
	c6_set(C6_LP_WDT + 0x1CUL, (1UL << 30) | (1UL << 18));
	c6_write(C6_LP_WDT + 0x20UL, 0);
}

void Reset_Handler(void) {
	unsigned long *src;
	unsigned long *dst;

	_watchdogs_off();

	src = &_sidata;
	for(dst = &_sdata; dst < &_edata; dst++)
		*dst = *src++;

	for(dst = &_sbss; dst < &_ebss; dst++)
		*dst = 0;

	main();

	c6_write(C6_LP_AON + 0x34UL, 1UL << 31);
	for(;;)
		;
}

__attribute__((naked, section(".init"), used, noreturn))
void _start(void) {
	__asm__ volatile (
		"la sp, __stack_top\n"
		"call Reset_Handler\n"
	);
}

/**@}*/
