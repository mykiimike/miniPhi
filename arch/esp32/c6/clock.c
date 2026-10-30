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
@defgroup mpArchESP32C6Clock Clock

@ingroup mpArchESP32

The ROM leaves the CPU on the 40 MHz crystal and the 480 MHz PLL up,
because the USB Serial/JTAG PHY takes 48 MHz from that PLL. HIGH is
PLL/3 (160 MHz), MEDIUM is 80 MHz, LOW and SLEEP are 40 MHz. The PLL
stays up on every level so the console does not drop.

The tick is SYSTIMER unit 0. Its clock is the crystal divided by 2.5,
16 MHz, and does not follow the CPU mux. One period is 16000 counts.

@{
*/

#define _CONF          (C6_SYSTIMER + 0x00UL)
#define _UNIT0_OP      (C6_SYSTIMER + 0x04UL)
#define _TARGET0_HI    (C6_SYSTIMER + 0x1CUL)
#define _TARGET0_LO    (C6_SYSTIMER + 0x20UL)
#define _TARGET0_CONF  (C6_SYSTIMER + 0x34UL)
#define _VALUE_HI      (C6_SYSTIMER + 0x40UL)
#define _VALUE_LO      (C6_SYSTIMER + 0x44UL)
#define _COMP0_LOAD    (C6_SYSTIMER + 0x50UL)
#define _INT_ENA       (C6_SYSTIMER + 0x64UL)
#define _INT_CLR       (C6_SYSTIMER + 0x6CUL)

#define _CPU_FREQ      (C6_PCR + 0x118UL)
#define _SYSCLK        (C6_PCR + 0x110UL)

static volatile unsigned long _ticks;
static unsigned long _hz;
static mp_kernel_t *_kernel;

static unsigned long long _now(void) {
	c6_write(_UNIT0_OP, 1UL << 29);
	c6_write(_UNIT0_OP, 1UL << 30);
	while((c6_read(_UNIT0_OP) & (1UL << 29)) == 0)
		;
	return(((unsigned long long)c6_read(_VALUE_HI) << 32) | c6_read(_VALUE_LO));
}

static void _tick(void *user) {
	(void)user;
	c6_write(_INT_CLR, 1UL);
	_ticks++;
}

static void _cpu(unsigned long hz) {
	unsigned long div = 3;
	unsigned long conf;
	unsigned long sys;

	if(hz >= C6_CPU_HIGH_HZ)
		div = 0;
	else if(hz >= C6_CPU_MEDIUM_HZ)
		div = 1;

	conf = c6_read(_CPU_FREQ);
	conf &= ~((0xFFUL << 8) | (1UL << 16));
	conf |= div << 8;
	c6_write(_CPU_FREQ, conf);

	sys = c6_read(_SYSCLK);
	sys = (sys & ~(3UL << 16)) | (1UL << 16);
	c6_write(_SYSCLK, sys);
	_hz = hz;
	if(_kernel)
		_kernel->arch.hz = hz;
}

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	unsigned long long target;
	unsigned long period = C6_SYSTIMER_HZ / C6_TICK_HZ;

	_kernel = kernel;
	_ticks = 0;
	_hz = C6_XTAL_HZ;

	c6_set(C6_PCR + 0x54UL, 1UL);
	c6_set(C6_PCR + 0x58UL, 1UL << 22);
	c6_set(_CONF, (1UL << 31) | (1UL << 30));

	target = _now() + period;
	c6_write(_TARGET0_LO, (unsigned long)target);
	c6_write(_TARGET0_HI, (unsigned long)(target >> 32));
	c6_write(_TARGET0_CONF, period | (1UL << 30));
	c6_write(_COMP0_LOAD, 1UL);
	c6_set(_CONF, 1UL << 24);
	c6_write(_INT_CLR, 1UL);
	c6_write(_INT_ENA, 1UL);

	if(!mp_interrupt_set(C6_SRC_SYSTIMER, _tick, NULL, "SYSTIMER"))
		return(FALSE);

	_cpu(C6_CPU_HIGH_HZ);
	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	(void)kernel;
	c6_write(_INT_ENA, 0);
	c6_clr(_CONF, 1UL << 24);
	mp_interrupt_unset(C6_SRC_SYSTIMER);
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
	(void)kernel;
}

unsigned long mp_clock_ticks(void) {
	return(_ticks);
}

unsigned long mp_clock_tick_hz(void) {
	return(C6_TICK_HZ);
}

unsigned long mp_clock_get_speed(void) {
	return(_hz);
}

unsigned long mp_clock_us(void) {
	return((unsigned long)(_now() / (C6_SYSTIMER_HZ / 1000000UL)));
}

unsigned long mp_clock_level_hz(mp_power_level_t level) {
	if(level == MP_POWER_HIGH)
		return(C6_CPU_HIGH_HZ);
	if(level == MP_POWER_MEDIUM)
		return(C6_CPU_MEDIUM_HZ);
	return(C6_CPU_LOW_HZ);
}

mp_ret_t mp_clock_level(mp_kernel_t *kernel, mp_power_level_t level) {
	(void)kernel;
	if(level > MP_POWER_HIGH)
		return(FALSE);
	_cpu(mp_clock_level_hz(level));
	return(TRUE);
}

void mp_clock_schedule(mp_kernel_t *kernel, unsigned long limit) {
	(void)kernel;
	(void)limit;
	__asm__ volatile ("wfi" ::: "memory");
}

const char *mp_clock_deep(void) {
	return("WFI");
}

const char *mp_clock_blocker(void) {
	return(NULL);
}

void mp_clock_delay(int delay) {
	unsigned long start;

	if(delay <= 0)
		return;
	start = _ticks;
	while((long)(_ticks - start) < delay)
		__asm__ volatile ("wfi" ::: "memory");
}

/**@}*/
