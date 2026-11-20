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
@defgroup mpArchRP2350Interrupt Interrupts

@ingroup mpArchRP2350

The peripheral handlers in startup.c call mp_interrupt_dispatch(). It
reads the active line in SCB->ICSR and runs the callback from
mp_interrupt_set().

@{
*/

static mp_interrupt_t __interrupts[MP_ARCH_IRQ_COUNT];

#define _IRQ_PRIORITY 0x80

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
#define _PENDSV_SHP 10
#define _PENDSV_PRIORITY 0xF0

void mp_task_sched_dispatch(void);
#endif

/* Peripheral lines live in core 0's NVIC, so a driver opened from a task
 * on another core still takes its interrupts where the tick is. */
static void _mp_interrupt_line(int vector, int on) {
#if MP_CPUS > 1
	if(SIO_CPUID != 0) {
		mp_cpu_line(vector, on);
		return;
	}
#endif
	if(on) {
		NVIC->ICPR[vector >> 5] = 1UL << (vector & 31);
		NVIC->ISER[vector >> 5] = 1UL << (vector & 31);
	}
	else
		NVIC->ICER[vector >> 5] = 1UL << (vector & 31);
}

static void _mp_interrupt_unhandled(void *user) {
	int irq = (int)(intptr_t)user;

	NVIC->ICER[irq >> 5] = 1UL << (irq & 31);
}

mp_ret_t mp_interrupt_init() {
	int a;

	for(a = 0; a < MP_ARCH_IRQ_COUNT; a++) {
		__interrupts[a].callback = _mp_interrupt_unhandled;
		__interrupts[a].user = (void *)(intptr_t)a;
		__interrupts[a].who = NULL;
		NVIC->IP[a] = _IRQ_PRIORITY;
	}

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	SCB->AIRCR = (0x05FAUL << 16) | (3UL << 8);
	SCB->SHP[_PENDSV_SHP] = _PENDSV_PRIORITY;
	SCB->ICSR = SCB_ICSR_PENDSVCLR;
#endif

	return(TRUE);
}

mp_ret_t mp_interrupt_fini() {
	int a;

	for(a = 0; a < MP_ARCH_IRQ_COUNT; a++)
		mp_interrupt_unset(a);

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	SCB->ICSR = SCB_ICSR_PENDSVCLR;
#endif

	return(TRUE);
}

mp_interrupt_t *mp_interrupt_set(int vector, mp_interrupt_cb_t in, void *user, char *who) {
	mp_interrupt_t *interrupt;

	if(vector < 0 || vector >= MP_ARCH_IRQ_COUNT)
		return(NULL);

	interrupt = &__interrupts[vector];

	MP_INTERRUPT_SAFE_BEGIN

	interrupt->callback = in;
	interrupt->user = user;
	interrupt->who = who;

	MP_INTERRUPT_SAFE_END

	_mp_interrupt_line(vector, 1);

	return(interrupt);
}

mp_ret_t mp_interrupt_unset(int vector) {
	mp_interrupt_t *interrupt;

	if(vector < 0 || vector >= MP_ARCH_IRQ_COUNT)
		return(FALSE);

	interrupt = &__interrupts[vector];

	_mp_interrupt_line(vector, 0);

	MP_INTERRUPT_SAFE_BEGIN

	interrupt->callback = _mp_interrupt_unhandled;
	interrupt->user = (void *)(intptr_t)vector;
	interrupt->who = NULL;

	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

void mp_interrupt_dispatch(void) {
	int irq = (int)(SCB->ICSR & SCB_ICSR_VECTACTIVE) - 16;
	mp_interrupt_t *interrupt;

	if(irq < 0 || irq >= MP_ARCH_IRQ_COUNT)
		return;

	interrupt = &__interrupts[irq];
	interrupt->callback(interrupt->user);
}

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT

#if !MP_TASK_SHARE
void PendSV_Handler(void) {
	mp_task_sched_dispatch();
}
#endif

/* PendSV is banked: this pends the scheduler of the calling core */
void mp_task_pend(void) {
	SCB->ICSR = SCB_ICSR_PENDSVSET;
}

#endif

/**@}*/
