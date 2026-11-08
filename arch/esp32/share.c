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

/* One frame, 32 words, 16-byte aligned. The trap stores it and mret
 * restores it, so a forged first frame and a preempted callback are
 * the same shape. No F or D registers: these cores are integer only.
 *   0 mepc  1 mstatus  2 ra  3 gp  4 tp
 *   5 t0 .. 11 t6  12 a0 .. 19 a7  20 s0 .. 31 s11
 */
#define _FRAME 128

void *mp_task_arch_forge(void *top, void (*entry)(void *), void *arg) {
	unsigned long *frame;
	unsigned long sp;
	unsigned long gp;
	unsigned long tp;
	int i;

	if(!top || !entry)
		return(NULL);
	sp = (unsigned long)top & ~15UL;
	if(sp < _FRAME)
		return(NULL);
	sp -= _FRAME;
	frame = (unsigned long *)sp;
	for(i = 0; i < _FRAME / 4; i++)
		frame[i] = 0;
	/* MPP is M, MPIE is set, MIE is clear. mret copies MPIE into MIE. */
	frame[0] = (unsigned long)entry;
	frame[1] = 0x1880UL;
	frame[2] = (unsigned long)entry;
	__asm__ volatile ("mv %0, gp" : "=r"(gp));
	__asm__ volatile ("mv %0, tp" : "=r"(tp));
	frame[3] = gp;
	frame[4] = tp;
	frame[12] = (unsigned long)arg;
	return((void *)sp);
}

/* The tick and USB sit at priority 2. Raising the threshold hides the
 * sched line, so dispatch can run a higher task without this trap
 * reentering. MIE is cleared again before the stack pointer changes. */
void mp_task_share_open(void) {
#if defined(ESP32C6)
	c6_write(C6_PLIC + 0x90UL, C6_PRIO_SCHED);
	c6_write(C6_INTPRI + 0x8cUL, C6_PRIO_DEVICE);
#elif defined(ESP32C3)
	c3_write(C3_INTC_THRESH, C3_PRIO_SCHED);
#endif
	__asm__ volatile ("fence" ::: "memory");
	__asm__ volatile ("csrs mstatus, %0" :: "r"(8) : "memory");
}

void mp_task_share_close(void) {
	__asm__ volatile ("csrc mstatus, %0" :: "r"(8) : "memory");
#if defined(ESP32C6)
	c6_write(C6_PLIC + 0x90UL, 0);
	c6_write(C6_INTPRI + 0x8cUL, 0);
#elif defined(ESP32C3)
	c3_write(C3_INTC_THRESH, 0);
#endif
	__asm__ volatile ("fence" ::: "memory");
}

/* Vectored entry for the sched line. Hardware saves nothing. The C
 * function returns the next frame, which may be a different stack. */
__attribute__((naked, aligned(4), used))
void mp_task_share_trap(void) {
	__asm__ volatile (
		".option push\n"
		".option norvc\n"
		"addi sp, sp, -128\n"
		"sw ra, 8(sp)\n"
		"sw gp, 12(sp)\n"
		"sw tp, 16(sp)\n"
		"sw t0, 20(sp)\n"
		"sw t1, 24(sp)\n"
		"sw t2, 28(sp)\n"
		"sw t3, 32(sp)\n"
		"sw t4, 36(sp)\n"
		"sw t5, 40(sp)\n"
		"sw t6, 44(sp)\n"
		"sw a0, 48(sp)\n"
		"sw a1, 52(sp)\n"
		"sw a2, 56(sp)\n"
		"sw a3, 60(sp)\n"
		"sw a4, 64(sp)\n"
		"sw a5, 68(sp)\n"
		"sw a6, 72(sp)\n"
		"sw a7, 76(sp)\n"
		"sw s0, 80(sp)\n"
		"sw s1, 84(sp)\n"
		"sw s2, 88(sp)\n"
		"sw s3, 92(sp)\n"
		"sw s4, 96(sp)\n"
		"sw s5, 100(sp)\n"
		"sw s6, 104(sp)\n"
		"sw s7, 108(sp)\n"
		"sw s8, 112(sp)\n"
		"sw s9, 116(sp)\n"
		"sw s10, 120(sp)\n"
		"sw s11, 124(sp)\n"
		"csrr t0, mepc\n"
		"sw t0, 0(sp)\n"
		"csrr t0, mstatus\n"
		"sw t0, 4(sp)\n"
		"mv a0, sp\n"
		"call mp_task_share_irq\n"
		"mv sp, a0\n"
		"lw t0, 0(sp)\n"
		"csrw mepc, t0\n"
		"lw t0, 4(sp)\n"
		"csrw mstatus, t0\n"
		"lw ra, 8(sp)\n"
		"lw gp, 12(sp)\n"
		"lw tp, 16(sp)\n"
		"lw t0, 20(sp)\n"
		"lw t1, 24(sp)\n"
		"lw t2, 28(sp)\n"
		"lw t3, 32(sp)\n"
		"lw t4, 36(sp)\n"
		"lw t5, 40(sp)\n"
		"lw t6, 44(sp)\n"
		"lw a0, 48(sp)\n"
		"lw a1, 52(sp)\n"
		"lw a2, 56(sp)\n"
		"lw a3, 60(sp)\n"
		"lw a4, 64(sp)\n"
		"lw a5, 68(sp)\n"
		"lw a6, 72(sp)\n"
		"lw a7, 76(sp)\n"
		"lw s0, 80(sp)\n"
		"lw s1, 84(sp)\n"
		"lw s2, 88(sp)\n"
		"lw s3, 92(sp)\n"
		"lw s4, 96(sp)\n"
		"lw s5, 100(sp)\n"
		"lw s6, 104(sp)\n"
		"lw s7, 108(sp)\n"
		"lw s8, 112(sp)\n"
		"lw s9, 116(sp)\n"
		"lw s10, 120(sp)\n"
		"lw s11, 124(sp)\n"
		"addi sp, sp, 128\n"
		"mret\n"
		".option pop\n"
	);
}

#endif
