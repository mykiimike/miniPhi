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
@defgroup mpArchESP32ClassicInterrupt Interrupts

@ingroup mpArchESP32

Level-1 sources arrive through the exception vector with EXCCAUSE 4.
The PRO CPU map register for a peripheral source selects the CPU line,
and INTENABLE unmasks that line. There is no PLIC and no threshold.
The tick and FROM_CPU source 24 are level-2 lines. The dispatcher runs
on that vector, so a long background task keeps taking the tick. GPIO
and the UARTs stay on level-1 lines.

@{
*/

mp_interrupt_t cl_slot[32];

static int _line(int source) {
	if(source == CL_SRC_TG0_T0)
		return(CL_CPU_TICK);
	if(source == CL_SRC_GPIO)
		return(CL_CPU_GPIO);
	if(source == CL_SRC_UART0 || source == CL_SRC_UART1 || source == CL_SRC_UART2)
		return(CL_CPU_UART);
	return(-1);
}

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	static void _cl_sched_line(void);
#endif

static void _intenable(int line, int on) {
	unsigned long value;
	unsigned long bit = 1UL << line;

	__asm__ volatile ("rsr.intenable %0" : "=r"(value));
	if(on)
		value |= bit;
	else
		value &= ~bit;
	__asm__ volatile ("wsr.intenable %0" :: "r"(value) : "memory");
}

mp_ret_t mp_interrupt_init(void) {
	int line;
	unsigned long zero = 0;

	for(line = 0; line < 32; line++) {
		cl_slot[line].callback = NULL;
		cl_slot[line].user = NULL;
		cl_slot[line].who = NULL;
	}

	__asm__ volatile ("wsr.intenable %0" :: "r"(zero) : "memory");
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	_cl_sched_line();
#endif
	mp_interrupt_disable();
	return(TRUE);
}

mp_ret_t mp_interrupt_fini(void) {
	unsigned long zero = 0;

	mp_interrupt_disable();
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	cl_write(CL_FROM_CPU, 0);
#endif
	__asm__ volatile ("wsr.intenable %0" :: "r"(zero) : "memory");
	return(TRUE);
}

mp_interrupt_t *mp_interrupt_set(int vector, mp_interrupt_cb_t in, void *user, char *who) {
	int line = _line(vector);

	if(line < 0) {
		mp_printk("Interrupt %d has no CPU line", vector);
		return(NULL);
	}

	cl_slot[line].callback = in;
	cl_slot[line].user = user;
	cl_slot[line].who = who;
	cl_write(CL_INTMTX + (unsigned long)vector * 4UL, (unsigned long)line);
	_intenable(line, 1);
	return(&cl_slot[line]);
}

mp_ret_t mp_interrupt_unset(int vector) {
	int line = _line(vector);

	if(line < 0)
		return(FALSE);
	_intenable(line, 0);
	cl_slot[line].callback = NULL;
	cl_slot[line].user = NULL;
	cl_slot[line].who = NULL;
	return(TRUE);
}

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT

void mp_task_sched_dispatch(void);

/* The level-2 handler calls this with INTLEVEL already at 2. Clearing
 * the source before the dispatcher lets a later pend fire again on rfi. */
static void _cl_sched(void *user) {
	(void)user;
#if MP_CPUS > 1
	if(cl_cpu_id()) {
		cl_write(CL_FROM_CPU1, 0);
		if(cl_cpu_should_park()) {
			cl_cpu_park_spin();
			return;
		}
	}
	else
#endif
		cl_write(CL_FROM_CPU, 0);
	mp_task_sched_dispatch();
}

static void _cl_sched_line(void) {
	cl_slot[CL_CPU_SCHED].callback = _cl_sched;
	cl_slot[CL_CPU_SCHED].user = NULL;
	cl_slot[CL_CPU_SCHED].who = "task";
	cl_write(CL_INTMTX + (unsigned long)CL_SRC_FROM_CPU * 4UL, CL_CPU_SCHED);
	cl_write(CL_FROM_CPU, 0);
	_intenable(CL_CPU_SCHED, 1);
}

void mp_task_pend(void) {
#if MP_CPUS > 1
	if(cl_cpu_id()) {
		cl_write(CL_FROM_CPU1, 1);
		return;
	}
#endif
	cl_write(CL_FROM_CPU, 1);
}

#endif

/**@}*/
