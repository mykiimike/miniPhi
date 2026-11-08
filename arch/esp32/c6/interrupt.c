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

/**
@defgroup mpArchESP32Interrupt Interrupts

@ingroup mpArchESP32

One machine-mode handler. mcause carries the CPU line. The matrix
register for a peripheral source selects that line. The machine PLIC
at 0x20001000 is what the core takes after the ROM boots: its
threshold comes up at 1 and masks a priority of 1, so init writes it
back to 0. INTPRI at 0x600C5000 is programmed the same way. Priority
n lives at +0x10 + n*4 on the PLIC and at +0x0c + n*4 on INTPRI. Bit
n of either type register is level when clear. A preemptive build
routes FROM_CPU source 22 to CPU line 5 at priority 1. The tick and
the USB line stay at priority 2, so they can preempt the dispatcher.

@{
*/

static mp_interrupt_t _slot[32];

/* The HP core takes vectored mode only: mtvec bit 0 stays set and the base
 * is forced to a 256-byte boundary. Each slot is one j to the common handler.
 */
__attribute__((naked, aligned(256), used))
static void _vectors(void) {
	__asm__ volatile (
		".option push\n"
		".option norvc\n"
#if MP_TASK_SHARE
		/* Line 5 is the sched line. Its frame is the task stack. */
		"j mp_trap\n"
		"j mp_trap\n"
		"j mp_trap\n"
		"j mp_trap\n"
		"j mp_trap\n"
		"j mp_task_share_trap\n"
		".rept 26\n"
		"j mp_trap\n"
		".endr\n"
#else
		".rept 32\n"
		"j mp_trap\n"
		".endr\n"
#endif
		".option pop\n"
	);
}

static void _mie_set(int line) {
	unsigned long bit = 1UL << line;

	__asm__ volatile ("csrs mie, %0" :: "r"(bit) : "memory");
}

static void _mie_clr(int line) {
	unsigned long bit = 1UL << line;

	__asm__ volatile ("csrc mie, %0" :: "r"(bit) : "memory");
}

static void _pri(int line, unsigned long pri) {
	c6_write(C6_PLIC + 0x10UL + (unsigned long)line * 4UL, pri);
	c6_write(C6_INTPRI + 0x0cUL + (unsigned long)line * 4UL, pri);
}

static int _line(int source) {
	if(source == C6_SRC_USB)
		return(C6_CPU_USB);
	if(source == C6_SRC_SYSTIMER)
		return(C6_CPU_TICK);
	if(source == C6_SRC_GPIO)
		return(C6_CPU_GPIO);
	return(-1);
}

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	static void _c6_sched_line(void);
	#if !MP_TASK_SHARE
		static void _c6_sched(void);
	#endif
	void mp_task_sched_dispatch(void);
#endif

void __attribute__((interrupt("machine"), aligned(4), used)) mp_trap(void) {
	unsigned long cause;
	int line;

	/* A task switch waits on the sched line. This line is above it, so a
	 * callback started here must not arm a stack: that wait would not run
	 * until this handler returned. */
#if MP_TASK_SHARE
	mp_task_isr_nest(1);
#endif
	__asm__ volatile ("csrr %0, mcause" : "=r"(cause));
	if((cause & 0x80000000UL) == 0) {
		for(;;)
			;
	}

	line = (int)(cause & 31UL);
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT && !MP_TASK_SHARE
	if(line == C6_CPU_SCHED) {
		_c6_sched();
		return;
	}
#endif
	if(_slot[line].callback)
		_slot[line].callback(_slot[line].user);
#if MP_TASK_SHARE
	mp_task_isr_nest(0);
#endif
}

mp_ret_t mp_interrupt_init(void) {
	int line;

	for(line = 0; line < 32; line++) {
		_slot[line].callback = NULL;
		_slot[line].user = NULL;
		_slot[line].who = NULL;
	}

	c6_write(C6_PLIC + 0x90UL, 0);
	c6_write(C6_PLIC, 0);
	c6_write(C6_PLIC + 0x4UL, 0);
	c6_write(C6_INTPRI + 0x8cUL, 0);
	c6_write(C6_INTPRI, 0);
	c6_write(C6_INTPRI + 0x4UL, 0);
	__asm__ volatile ("csrw mtvec, %0" :: "r"(((unsigned long)_vectors & ~0xFFUL) | 1UL) : "memory");
	__asm__ volatile ("csrw mie, zero" ::: "memory");
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	_c6_sched_line();
#endif
	mp_interrupt_disable();
	return(TRUE);
}

mp_ret_t mp_interrupt_fini(void) {
	mp_interrupt_disable();
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	c6_write(C6_FROM_CPU, 0);
#endif
	c6_write(C6_PLIC, 0);
	c6_write(C6_INTPRI, 0);
	return(TRUE);
}

mp_interrupt_t *mp_interrupt_set(int vector, mp_interrupt_cb_t in, void *user, char *who) {
	int line = _line(vector);

	if(line < 0) {
		mp_printk("Interrupt %d has no CPU line", vector);
		return(NULL);
	}

	_slot[line].callback = in;
	_slot[line].user = user;
	_slot[line].who = who;
	c6_write(C6_INTMTX + (unsigned long)vector * 4UL, (unsigned long)line);
	/* Above the scheduler line, so the tick and USB preempt a task. */
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	_pri(line, C6_PRIO_DEVICE);
#else
	_pri(line, 1);
#endif
	c6_clr(C6_PLIC + 0x4UL, 1UL << line);
	c6_set(C6_PLIC, 1UL << line);
	c6_clr(C6_INTPRI + 0x4UL, 1UL << line);
	c6_set(C6_INTPRI, 1UL << line);
	_mie_set(line);
	return(&_slot[line]);
}

mp_ret_t mp_interrupt_unset(int vector) {
	int line = _line(vector);

	if(line < 0)
		return(FALSE);
	c6_clr(C6_PLIC, 1UL << line);
	c6_clr(C6_INTPRI, 1UL << line);
	_mie_clr(line);
	_slot[line].callback = NULL;
	_slot[line].user = NULL;
	_slot[line].who = NULL;
	return(TRUE);
}

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT

/* FROM_CPU source 22, CPU line 5, priority 1, level. The tick and USB
 * stay at priority 2. The PLIC masks a priority equal to its threshold,
 * so 1 hides this line and still delivers 2. INTPRI masks a priority
 * below its threshold, so 2 is the value that hides 1 and delivers 2. */
static void _c6_sched_line(void) {
	c6_write(C6_INTMTX + (unsigned long)C6_SRC_FROM_CPU * 4UL, C6_CPU_SCHED);
	_pri(C6_CPU_SCHED, C6_PRIO_SCHED);
	c6_clr(C6_PLIC + 0x4UL, 1UL << C6_CPU_SCHED);
	c6_set(C6_PLIC, 1UL << C6_CPU_SCHED);
	c6_clr(C6_INTPRI + 0x4UL, 1UL << C6_CPU_SCHED);
	c6_set(C6_INTPRI, 1UL << C6_CPU_SCHED);
	c6_write(C6_FROM_CPU, 0);
	_mie_set(C6_CPU_SCHED);
}

#if !MP_TASK_SHARE
static void _c6_sched(void) {
	unsigned long epc;
	unsigned long status;

	__asm__ volatile ("csrr %0, mepc" : "=r"(epc));
	__asm__ volatile ("csrr %0, mstatus" : "=r"(status));
	c6_write(C6_FROM_CPU, 0);
	c6_write(C6_PLIC + 0x90UL, C6_PRIO_SCHED);
	c6_write(C6_INTPRI + 0x8cUL, C6_PRIO_DEVICE);
	__asm__ volatile ("fence" ::: "memory");
	__asm__ volatile ("csrs mstatus, %0" :: "r"(8) : "memory");
	mp_task_sched_dispatch();
	__asm__ volatile ("csrc mstatus, %0" :: "r"(8) : "memory");
	c6_write(C6_PLIC + 0x90UL, 0);
	c6_write(C6_INTPRI + 0x8cUL, 0);
	__asm__ volatile ("fence" ::: "memory");
	__asm__ volatile ("csrw mstatus, %0" :: "r"(status) : "memory");
	__asm__ volatile ("csrw mepc, %0" :: "r"(epc) : "memory");
}
#endif

/* Strong symbol. The weak one in preempt.c runs the dispatcher inline. */
void mp_task_pend(void) {
	c6_write(C6_FROM_CPU, 1);
}

#endif

/**@}*/
