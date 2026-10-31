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
n of either type register is level when clear.

@{
*/

static mp_interrupt_t _slot[32];

/* The HP core takes vectored mode only: mtvec bit 0 stays set and the base
 * is forced to a 256-byte boundary. Each slot is one j to the common handler.
 */
__attribute__((naked, aligned(256), used))
static void _vectors(void) {
	__asm__ volatile (
		".rept 32\n"
		"j mp_trap\n"
		".endr\n"
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

static int _line(int source) {
	if(source == C6_SRC_USB)
		return(C6_CPU_USB);
	if(source == C6_SRC_SYSTIMER)
		return(C6_CPU_TICK);
	if(source == C6_SRC_GPIO)
		return(C6_CPU_GPIO);
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
	mp_interrupt_disable();
	return(TRUE);
}

mp_ret_t mp_interrupt_fini(void) {
	mp_interrupt_disable();
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
	c6_write(C6_PLIC + 0x10UL + (unsigned long)line * 4UL, 1);
	c6_clr(C6_PLIC + 0x4UL, 1UL << line);
	c6_set(C6_PLIC, 1UL << line);
	c6_write(C6_INTPRI + 0x0cUL + (unsigned long)line * 4UL, 1);
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

/**@}*/
