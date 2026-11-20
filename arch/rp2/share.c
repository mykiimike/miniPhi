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

#if MP_TASK_SHARE

/* PendSV frame on MSP. No PSP. The integer frame is 72 bytes, the same
 * layout the STM32 port forges. EXC_RETURN bit 4 clear means the core
 * already stacked s0-s15, and this handler adds s16-s31 under it.
 * Lazy stacking is off, so that bit is valid on entry. Low to high when
 * the FPU was live: pad, exc_return, r4-r11, s16-s31, then the hardware
 * frame. A forged task has bit 4 set and no s16-s31. */

#define _FRAME 72

void *mp_task_arch_forge(void *top, void (*entry)(void *), void *arg) {
	unsigned long *frame;
	unsigned long sp;
	int i;

	if(!top || !entry)
		return(NULL);
	sp = (unsigned long)top & ~7UL;
	if(sp < _FRAME)
		return(NULL);
	sp -= _FRAME;
	frame = (unsigned long *)sp;
	for(i = 0; i < _FRAME / 4; i++)
		frame[i] = 0;
	/* ARMv8-M EXC_RETURN: S=1, DCRS=1, FType=1, Mode=Thread, SPSEL=MSP,
	 * bit[1] RES0, ES=1. Bit[1] set raises INVPC on the return. */
	frame[1] = 0xFFFFFFF9UL;
	frame[10] = (unsigned long)arg;
	frame[15] = (unsigned long)entry;
	frame[16] = (unsigned long)entry & ~1UL;
	frame[17] = 0x01000000UL;
	return((void *)sp);
}

void mp_task_share_open(void) {
	__asm__ volatile ("cpsie i" ::: "memory");
}

void mp_task_share_close(void) {
	__asm__ volatile ("cpsid i" ::: "memory");
}

__attribute__((naked, used))
void PendSV_Handler(void) {
	__asm__ volatile (
		"tst lr, #0x10\n"
		"bne 1f\n"
		"vstmdb sp!, {s16-s31}\n"
		"1:\n"
		"stmdb sp!, {r4-r11}\n"
		"sub sp, sp, #8\n"
		"str lr, [sp, #4]\n"
		"mov r0, sp\n"
		"bl mp_task_share_irq\n"
		"mov sp, r0\n"
		"ldr lr, [sp, #4]\n"
		"add sp, sp, #8\n"
		"ldmia sp!, {r4-r11}\n"
		"tst lr, #0x10\n"
		"bne 2f\n"
		"vldmia sp!, {s16-s31}\n"
		"2:\n"
		"cpsie i\n"
		"bx lr\n"
	);
}

#endif
