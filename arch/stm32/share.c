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

/* PendSV frame on MSP. No PSP and no FPU: the build is soft-float, so
 * the hardware frame stays eight words. Low to high:
 *   pad, exc_return, r4-r11, then r0, r1, r2, r3, r12, lr, pc, xpsr.
 * 72 bytes, 8-byte aligned. PRIMASK is not stacked, so it is clear
 * again before the exception return. */
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
	/* Thread mode, MSP, no floating-point frame. */
	frame[1] = 0xFFFFFFF9UL;
	frame[10] = (unsigned long)arg;
	frame[15] = (unsigned long)entry;
	frame[16] = (unsigned long)entry;
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
		"stmdb sp!, {r4-r11}\n"
		"sub sp, sp, #8\n"
		"str lr, [sp, #4]\n"
		"mov r0, sp\n"
		"bl mp_task_share_irq\n"
		"mov sp, r0\n"
		"ldr lr, [sp, #4]\n"
		"add sp, sp, #8\n"
		"ldmia sp!, {r4-r11}\n"
		"cpsie i\n"
		"bx lr\n"
	);
}

#endif
