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
@defgroup mpArchESP32S3Interrupt Interrupts

@ingroup mpArchESP32

Level-1 sources arrive through the exception vector with EXCCAUSE 4.
The matrix register for a peripheral source selects the CPU line, and
INTENABLE unmasks that line. There is no PLIC and no threshold.

@{
*/

mp_interrupt_t s3_slot[32];

static int _line(int source) {
	if(source == S3_SRC_USB)
		return(S3_CPU_USB);
	if(source == S3_SRC_SYSTIMER)
		return(S3_CPU_TICK);
	if(source == S3_SRC_GPIO)
		return(S3_CPU_GPIO);
	return(-1);
}

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
		s3_slot[line].callback = NULL;
		s3_slot[line].user = NULL;
		s3_slot[line].who = NULL;
	}

	__asm__ volatile ("wsr.intenable %0" :: "r"(zero) : "memory");
	mp_interrupt_disable();
	return(TRUE);
}

mp_ret_t mp_interrupt_fini(void) {
	unsigned long zero = 0;

	mp_interrupt_disable();
	__asm__ volatile ("wsr.intenable %0" :: "r"(zero) : "memory");
	return(TRUE);
}

mp_interrupt_t *mp_interrupt_set(int vector, mp_interrupt_cb_t in, void *user, char *who) {
	int line = _line(vector);

	if(line < 0) {
		mp_printk("Interrupt %d has no CPU line", vector);
		return(NULL);
	}

	s3_slot[line].callback = in;
	s3_slot[line].user = user;
	s3_slot[line].who = who;
	s3_write(S3_INTMTX + (unsigned long)vector * 4UL, (unsigned long)line);
	_intenable(line, 1);
	return(&s3_slot[line]);
}

mp_ret_t mp_interrupt_unset(int vector) {
	int line = _line(vector);

	if(line < 0)
		return(FALSE);
	_intenable(line, 0);
	s3_slot[line].callback = NULL;
	s3_slot[line].user = NULL;
	s3_slot[line].who = NULL;
	return(TRUE);
}

/**@}*/
