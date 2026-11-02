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
@defgroup mpArchESP32C3Interrupt Interrupts

@ingroup mpArchESP32

One machine-mode handler. mcause carries the CPU line. The matrix
register for a peripheral source selects that line. There is no PLIC.
Init writes the CPU threshold to 0 so a priority of 1 is delivered.
Priority n lives at 0x114 + n*4. A clear bit in the type register is
level. The systimer target is a pulse, so its line is edge: the CPU
latches it, and the handler writes the clear register or the next
pulse never arrives.

@{
*/

static mp_interrupt_t _slot[32];

/* Vectored mode only: mtvec bit 0 stays set and the base is a 256-byte
 * boundary. norvc keeps each slot one 4-byte j; a compressed j would
 * pack two slots into the word the core fetches. */
__attribute__((naked, section(".vectors"), aligned(256), used))
static void _vectors(void) {
	__asm__ volatile (
		".option push\n"
		".option norvc\n"
		".rept 32\n"
		"j mp_trap\n"
		".endr\n"
		".option pop\n"
	);
}

static int _line(int source) {
	if(source == C3_SRC_USB)
		return(C3_CPU_USB);
	if(source == C3_SRC_SYSTIMER)
		return(C3_CPU_TICK);
	if(source == C3_SRC_GPIO)
		return(C3_CPU_GPIO);
	return(-1);
}

void __attribute__((interrupt("machine"), aligned(4), used)) mp_trap(void) {
	unsigned long cause;
	int line;

	__asm__ volatile ("csrr %0, mcause" : "=r"(cause));
	if((cause & 0x80000000UL) == 0) {
		for(;;)
			;
	}

	line = (int)(cause & 31UL);
	if(_slot[line].callback)
		_slot[line].callback(_slot[line].user);
	/* The systimer line is edge. The latch stays set until this write,
	 * and a set latch re-enters the handler as soon as it returns. */
	if(line == C3_CPU_TICK)
		c3_write(C3_INTC_CLEAR, 1UL << line);
}

mp_ret_t mp_interrupt_init(void) {
	int line;

	for(line = 0; line < 32; line++) {
		_slot[line].callback = NULL;
		_slot[line].user = NULL;
		_slot[line].who = NULL;
	}

	c3_write(C3_INTC_THRESH, 0);
	c3_write(C3_INTC_ENABLE, 0);
	c3_write(C3_INTC_TYPE, 0);
	__asm__ volatile ("csrw mtvec, %0" :: "r"(((unsigned long)_vectors & ~0xFFUL) | 1UL) : "memory");
	/* mie is not implemented. Any write is an illegal instruction, and
	 * the line gate is CPU_INT_ENABLE. mstatus.MIE is the global gate. */
	mp_interrupt_disable();
	return(TRUE);
}

mp_ret_t mp_interrupt_fini(void) {
	mp_interrupt_disable();
	c3_write(C3_INTC_ENABLE, 0);
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
	c3_write(C3_INTMTX + (unsigned long)vector * 4UL, (unsigned long)line);
	c3_write(C3_INTC_PRI + (unsigned long)line * 4UL, 1);
	if(vector == C3_SRC_SYSTIMER)
		c3_set(C3_INTC_TYPE, 1UL << line);
	else
		c3_clr(C3_INTC_TYPE, 1UL << line);
	c3_set(C3_INTC_ENABLE, 1UL << line);
	return(&_slot[line]);
}

mp_ret_t mp_interrupt_unset(int vector) {
	int line = _line(vector);

	if(line < 0)
		return(FALSE);
	c3_clr(C3_INTC_ENABLE, 1UL << line);
	_slot[line].callback = NULL;
	_slot[line].user = NULL;
	_slot[line].who = NULL;
	return(TRUE);
}

/**@}*/
