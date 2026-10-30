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
@defgroup mpArchESP32S3Boot Boot

@ingroup mpArchESP32

The ROM copies this image into SRAM and jumps to _start. CPU0 is the
only core that runs. The flash-boot watchdogs are still armed until
Reset_Handler clears them. VECBASE points at the window spill and the
level-1 exception slot.

@{
*/

extern unsigned long _sidata, _sdata, _edata, _sbss, _ebss;

int main(void);

/* TIMG flash-boot mode fires even when the watchdog enable bit is clear. */
static void _wdt_stop(unsigned long base) {
	s3_write(base + 0x64UL, S3_WDT_KEY);
	s3_clr(base + 0x48UL, (1UL << 31) | (1UL << 14));
	s3_write(base + 0x64UL, 0);
}

static void _watchdogs_off(void) {
	_wdt_stop(S3_TIMG0);
	_wdt_stop(S3_TIMG1);

	s3_write(S3_RTC + 0xB0UL, S3_WDT_KEY);
	s3_clr(S3_RTC + 0x98UL, (1UL << 31) | (1UL << 12));
	s3_write(S3_RTC + 0xB0UL, 0);

	s3_write(S3_RTC + 0xB8UL, S3_SWD_KEY);
	s3_set(S3_RTC + 0xB4UL, (1UL << 31) | (1UL << 30));
	s3_write(S3_RTC + 0xB8UL, 0);
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

	s3_write(S3_RTC, 1UL << 31);
	for(;;)
		;
}

/* Window spill the ABI requires, at the VECBASE offsets. Level-1
 * exceptions land at +0x300. The save frame sits 128 bytes under the
 * interrupted stack pointer: the 16 bytes just under it hold the
 * caller's return address, and a store there becomes the next retw. */
__asm__(
	".section .vecbase,\"ax\"\n"
	".align 1024\n"
	".global _vecbase\n"
	"_vecbase:\n"
	".org 0x000\n"
	"s32e a0, a5, -16\n"
	"s32e a1, a5, -12\n"
	"s32e a2, a5, -8\n"
	"s32e a3, a5, -4\n"
	"rfwo\n"
	".org 0x040\n"
	"l32e a0, a5, -16\n"
	"l32e a1, a5, -12\n"
	"l32e a2, a5, -8\n"
	"l32e a3, a5, -4\n"
	"rfwu\n"
	".org 0x080\n"
	"s32e a0, a9, -16\n"
	"l32e a0, a1, -12\n"
	"s32e a1, a9, -12\n"
	"s32e a2, a9, -8\n"
	"s32e a3, a9, -4\n"
	"s32e a4, a0, -32\n"
	"s32e a5, a0, -28\n"
	"s32e a6, a0, -24\n"
	"s32e a7, a0, -20\n"
	"rfwo\n"
	".org 0x0C0\n"
	"l32e a0, a9, -16\n"
	"l32e a1, a9, -12\n"
	"l32e a2, a9, -8\n"
	"l32e a7, a1, -12\n"
	"l32e a3, a9, -4\n"
	"l32e a4, a7, -32\n"
	"l32e a5, a7, -28\n"
	"l32e a6, a7, -24\n"
	"l32e a7, a7, -20\n"
	"rfwu\n"
	".org 0x100\n"
	"s32e a0, a13, -16\n"
	"l32e a0, a1, -12\n"
	"s32e a1, a13, -12\n"
	"s32e a2, a13, -8\n"
	"s32e a3, a13, -4\n"
	"s32e a4, a0, -48\n"
	"s32e a5, a0, -44\n"
	"s32e a6, a0, -40\n"
	"s32e a7, a0, -36\n"
	"s32e a8, a0, -32\n"
	"s32e a9, a0, -28\n"
	"s32e a10, a0, -24\n"
	"s32e a11, a0, -20\n"
	"rfwo\n"
	".org 0x140\n"
	"l32e a0, a13, -16\n"
	"l32e a1, a13, -12\n"
	"l32e a2, a13, -8\n"
	"l32e a11, a1, -12\n"
	"l32e a3, a13, -4\n"
	"l32e a4, a11, -48\n"
	"l32e a5, a11, -44\n"
	"l32e a6, a11, -40\n"
	"l32e a7, a11, -36\n"
	"l32e a8, a11, -32\n"
	"l32e a9, a11, -28\n"
	"l32e a10, a11, -24\n"
	"l32e a11, a11, -20\n"
	"rfwu\n"
	".org 0x180\n"
	"j _s3_hang\n"
	".org 0x1C0\n"
	"j _s3_hang\n"
	".org 0x200\n"
	"j _s3_hang\n"
	".org 0x240\n"
	"j _s3_hang\n"
	".org 0x280\n"
	"j _s3_hang\n"
	".org 0x2C0\n"
	"j _s3_hang\n"
	".org 0x300\n"
	"j _s3_exc\n"
	".org 0x340\n"
	"j _s3_exc\n"
	".org 0x3C0\n"
	"j _s3_hang\n"
	".org 0x400\n"
	".text\n"
	".align 4\n"
	".global _s3_hang\n"
	"_s3_hang:\n"
	"j _s3_hang\n"
	".align 4\n"
	".Ls3slot:\n"
	".word s3_slot\n"
	".global _s3_exc\n"
	"_s3_exc:\n"
	"wsr a0, excsave1\n"
	"rsr a0, exccause\n"
	"bnei a0, 4, _s3_hang\n"
	"rsr a0, excsave1\n"
	"addi a1, a1, -128\n"
	"s32i a0, a1, 0\n"
	"s32i a2, a1, 4\n"
	"s32i a3, a1, 8\n"
	"s32i a4, a1, 12\n"
	"s32i a5, a1, 16\n"
	"s32i a6, a1, 20\n"
	"s32i a7, a1, 24\n"
	"s32i a8, a1, 28\n"
	"s32i a9, a1, 32\n"
	"s32i a10, a1, 36\n"
	"s32i a11, a1, 40\n"
	"s32i a12, a1, 44\n"
	"s32i a13, a1, 48\n"
	"s32i a14, a1, 52\n"
	"s32i a15, a1, 56\n"
	"rsr a2, epc1\n"
	"s32i a2, a1, 60\n"
	"rsr a2, ps\n"
	"s32i a2, a1, 64\n"
	"movi a2, 1\n"
	"slli a2, a2, 18\n"
	"addi a2, a2, 1\n"
	"wsr a2, ps\n"
	"rsync\n"
	"l32r a4, .Ls3slot\n"
	"rsr a2, interrupt\n"
	"rsr a3, intenable\n"
	"and a2, a2, a3\n"
	"movi a3, 0\n"
	"movi a5, 1\n"
	".Ls3loop:\n"
	"and a6, a2, a5\n"
	"beqz a6, .Ls3next\n"
	"l32i a6, a4, 0\n"
	"beqz a6, .Ls3next\n"
	"l32i a10, a4, 4\n"
	"callx8 a6\n"
	".Ls3next:\n"
	"addi a3, a3, 1\n"
	"addi a4, a4, 12\n"
	"slli a5, a5, 1\n"
	"bnei a3, 32, .Ls3loop\n"
	"wsr a2, intclear\n"
	"l32i a2, a1, 60\n"
	"wsr a2, epc1\n"
	"l32i a2, a1, 64\n"
	"wsr a2, ps\n"
	"l32i a0, a1, 0\n"
	"l32i a3, a1, 8\n"
	"l32i a4, a1, 12\n"
	"l32i a5, a1, 16\n"
	"l32i a6, a1, 20\n"
	"l32i a7, a1, 24\n"
	"l32i a8, a1, 28\n"
	"l32i a9, a1, 32\n"
	"l32i a10, a1, 36\n"
	"l32i a11, a1, 40\n"
	"l32i a12, a1, 44\n"
	"l32i a13, a1, 48\n"
	"l32i a14, a1, 52\n"
	"l32i a15, a1, 56\n"
	"l32i a2, a1, 4\n"
	"addi a1, a1, 128\n"
	"rsync\n"
	"rfe\n"
	".section .init,\"ax\"\n"
	".align 4\n"
	".global _start\n"
	"_start:\n"
	"j .Lstart\n"
	".align 4\n"
	".Lvec:\n"
	".word _vecbase\n"
	".Lstack:\n"
	".word __stack_top\n"
	".Lstart:\n"
	"movi a0, 0\n"
	"wsr a0, intenable\n"
	"wsr a0, windowbase\n"
	"rsync\n"
	"movi a0, 1\n"
	"wsr a0, windowstart\n"
	"rsync\n"
	"movi a0, 1\n"
	"slli a0, a0, 18\n"
	"wsr a0, ps\n"
	"rsync\n"
	"l32r a0, .Lvec\n"
	"wsr a0, vecbase\n"
	"rsync\n"
	"l32r a1, .Lstack\n"
	"call4 Reset_Handler\n"
	"j _s3_hang\n"
);

/**@}*/
