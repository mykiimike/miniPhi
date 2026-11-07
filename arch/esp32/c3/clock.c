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
@defgroup mpArchESP32C3Clock Clock

@ingroup mpArchESP32

The ROM leaves the CPU on the 40 MHz crystal and the 480 MHz PLL up,
because the USB Serial/JTAG PHY takes 48 MHz from that PLL. HIGH is
PLL/3 (160 MHz). MEDIUM, LOW and SLEEP are PLL/6 (80 MHz): the PLL has
no 40 MHz tap, and selecting the crystal would be the only way down.
The PLL stays selected on every level so the console does not drop.

The tick is SYSTIMER unit 0. Its clock is the crystal divided by 2.5,
16 MHz, and does not follow the CPU mux. One period is 16000 counts.

@{
*/

#define _CONF          (C3_SYSTIMER + 0x00UL)
#define _UNIT0_OP      (C3_SYSTIMER + 0x04UL)
#define _TARGET0_HI    (C3_SYSTIMER + 0x1CUL)
#define _TARGET0_LO    (C3_SYSTIMER + 0x20UL)
#define _TARGET0_CONF  (C3_SYSTIMER + 0x34UL)
#define _VALUE_HI      (C3_SYSTIMER + 0x40UL)
#define _VALUE_LO      (C3_SYSTIMER + 0x44UL)
#define _COMP0_LOAD    (C3_SYSTIMER + 0x50UL)
#define _INT_ENA       (C3_SYSTIMER + 0x64UL)
#define _INT_CLR       (C3_SYSTIMER + 0x6CUL)

#define _CPU_PER       (C3_SYSTEM + 0x08UL)
#define _CLK_EN0       (C3_SYSTEM + 0x10UL)
#define _RST_EN0       (C3_SYSTEM + 0x18UL)
#define _SYSCLK        (C3_SYSTEM + 0x58UL)

static volatile unsigned long _ticks;
static unsigned long _hz;
static mp_kernel_t *_kernel;

static unsigned long long _now(void) {
	unsigned long tries;

	/* Bit 29 is write-1-to-clear, bit 30 is write-triggered. A snapshot
	 * the ROM left valid is not the current count. A trigger issued
	 * before the counter clock is back from reset is discarded, so the
	 * request is repeated until the clock answers. */
	for(tries = 0; tries < 8; tries++) {
		unsigned long spin = 10000;

		c3_write(_UNIT0_OP, 1UL << 29);
		c3_write(_UNIT0_OP, 1UL << 30);
		while((c3_read(_UNIT0_OP) & (1UL << 29)) == 0 && spin)
			spin--;
		if(spin)
			break;
	}
	return(((unsigned long long)c3_read(_VALUE_HI) << 32) | c3_read(_VALUE_LO));
}

static void _tick(void *user) {
	(void)user;
	c3_write(_INT_CLR, 1UL);
	_ticks++;
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	/* A callback is still on the stack. The main loop cannot schedule
	 * until it returns, so a higher task has to run from here. */
	mp_task_slice();
#endif
}

static void _cpu(unsigned long hz) {
	unsigned long period = 0;
	unsigned long conf;

	if(hz >= C3_CPU_HIGH_HZ) {
		period = 1;
		hz = C3_CPU_HIGH_HZ;
	}
	else
		hz = C3_CPU_MEDIUM_HZ;

	/* bits 1:0 only. Bit 2 is PLL_FREQ_SEL and stays as the ROM left it. */
	conf = c3_read(_CPU_PER);
	conf = (conf & ~3UL) | period;
	c3_write(_CPU_PER, conf);

	conf = c3_read(_SYSCLK);
	conf = (conf & ~(3UL << 10)) | (1UL << 10);
	c3_write(_SYSCLK, conf);
	_hz = hz;
	if(_kernel)
		_kernel->arch.hz = hz;
}

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	unsigned long long target;
	unsigned long period = C3_SYSTIMER_HZ / C3_TICK_HZ;

	_kernel = kernel;
	_ticks = 0;
	_hz = C3_XTAL_HZ;

	c3_set(_CLK_EN0, 1UL << 29);
	/* Bit 0 forces the counter clock on. Pulse reset so a snapshot
	 * the ROM left mid-update cannot answer the next read. */
	c3_set(_RST_EN0, 1UL << 29);
	c3_clr(_RST_EN0, 1UL << 29);
	c3_set(_CONF, (1UL << 31) | (1UL << 30) | 1UL);

	target = _now() + period;
	c3_write(_TARGET0_LO, (unsigned long)target);
	c3_write(_TARGET0_HI, (unsigned long)(target >> 32));
	c3_write(_TARGET0_CONF, period | (1UL << 30));
	c3_write(_COMP0_LOAD, 1UL);
	c3_set(_CONF, 1UL << 24);
	c3_write(_INT_CLR, 1UL);
	c3_write(_INT_ENA, 1UL);

	if(!mp_interrupt_set(C3_SRC_SYSTIMER, _tick, NULL, "SYSTIMER"))
		return(FALSE);
	/* The comparator can match before the CPU line is enabled. The
	 * line is edge, so that first level is missed and wfi never wakes.
	 * Drop it; the next period is the edge the core takes. */
	c3_write(_INT_CLR, 1UL);
	c3_write(C3_INTC_CLEAR, 1UL << C3_CPU_TICK);

	_cpu(C3_CPU_HIGH_HZ);
	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	(void)kernel;
	c3_write(_INT_ENA, 0);
	c3_clr(_CONF, 1UL << 24);
	mp_interrupt_unset(C3_SRC_SYSTIMER);
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
	(void)kernel;
}

unsigned long mp_clock_ticks(void) {
	return(_ticks);
}

unsigned long mp_clock_tick_hz(void) {
	return(C3_TICK_HZ);
}

unsigned long mp_clock_get_speed(void) {
	return(_hz);
}

unsigned long mp_clock_us(void) {
	return((unsigned long)(_now() / (C3_SYSTIMER_HZ / 1000000UL)));
}

unsigned long mp_clock_level_hz(mp_power_level_t level) {
	if(level == MP_POWER_HIGH)
		return(C3_CPU_HIGH_HZ);
	return(C3_CPU_MEDIUM_HZ);
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
