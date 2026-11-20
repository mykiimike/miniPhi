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
@defgroup mpArchESP32ClassicCpu Second core

@ingroup mpArchESP32

The APP CPU stays in the reset taken at boot. mp_cpu_start writes its
entry into DPORT, turns its clock on and releases the reset. It answers
once its own stack, VECBASE and the two level-2 lines are up. TIMG1
wakes it every tick. FROM_CPU_1 is the scheduler line. Core 0 keeps the
tick count. A park request freezes this core in a RAM loop with
interrupts masked, which is what a later flash driver needs while the
cache is off the bus.

@{
*/

#if MP_CPUS > 1

#define _TCONFIG  (CL_TIMG1 + 0x00UL)
#define _TLO      (CL_TIMG1 + 0x04UL)
#define _THI      (CL_TIMG1 + 0x08UL)
#define _TUPDATE  (CL_TIMG1 + 0x0CUL)
#define _TALARMLO (CL_TIMG1 + 0x10UL)
#define _TALARMHI (CL_TIMG1 + 0x14UL)
#define _TLOADLO  (CL_TIMG1 + 0x18UL)
#define _TLOADHI  (CL_TIMG1 + 0x1CUL)
#define _TLOAD    (CL_TIMG1 + 0x20UL)
#define _TENA     (CL_TIMG1 + 0x98UL)
#define _TCLR     (CL_TIMG1 + 0xA4UL)
#define _TDIV     (cl_apb_hz() / CL_TIMER_HZ)
#define _TPERIOD  (CL_TIMER_HZ / CL_TICK_HZ)

static void (*_entry)(void);
static volatile int _up;
static volatile int _hold;
static volatile int _parked;
static unsigned long long _alarm;

static volatile unsigned long _idle_us[MP_CPUS];
static unsigned long _seen_idle[MP_CPUS];
static unsigned long _seen_at[MP_CPUS];
static int _load[MP_CPUS] = { [0 ... MP_CPUS - 1] = -1 };

extern void cl_cpu1_entry(void);

static void _memw(void) {
	__asm__ volatile ("memw" ::: "memory");
}

/* The reset vector. Windowed ABI, VECBASE, then the C entry. The stack
 * symbol is the top, the same way _start hands Reset_Handler its stack. */
__asm__(
	".section .text\n"
	".align 4\n"
	".global cl_cpu1_entry\n"
	"cl_cpu1_entry:\n"
	"j .Lcpu1go\n"
	".align 4\n"
	".Lcpu1vec:\n"
	".word _vecbase\n"
	".Lcpu1sp:\n"
	".word cl_cpu1_stack_top\n"
	".Lcpu1go:\n"
	"movi a0, 0\n"
	"wsr a0, intenable\n"
	"wsr a0, windowbase\n"
	"rsync\n"
	"movi a0, 1\n"
	"wsr a0, windowstart\n"
	"rsync\n"
	"movi a0, 1\n"
	"slli a0, a0, 18\n"
	"wsr a0, ps\n"
	"rsync\n"
	"l32r a0, .Lcpu1vec\n"
	"wsr a0, vecbase\n"
	"rsync\n"
	"l32r a1, .Lcpu1sp\n"
	"call4 cl_cpu1_main\n"
	"j .\n"
	".section .bss\n"
	".align 16\n"
	"cl_cpu1_stack:\n"
	".skip 8192\n"
	".global cl_cpu1_stack_top\n"
	"cl_cpu1_stack_top:\n"
	".section .text\n"
);

static unsigned long long _now(void) {
	cl_write(_TUPDATE, 1);
	return(((unsigned long long)cl_read(_THI) << 32) | cl_read(_TLO));
}

static void _arm(unsigned long long when) {
	cl_write(_TALARMLO, (unsigned long)when);
	cl_write(_TALARMHI, (unsigned long)(when >> 32));
	_alarm = when;
	cl_set(_TCONFIG, 1UL << 10);
}

static void _wdt_stop(void) {
	cl_write(CL_TIMG1 + 0x64UL, CL_WDT_KEY);
	cl_clr(CL_TIMG1 + 0x48UL, (1UL << 31) | (1UL << 14));
	cl_write(CL_TIMG1 + 0x64UL, 0);
}

static void _timer(void) {
	cl_periph(CL_TG1_CLK, 1);
	/* Releasing the peripheral reset rearms the flash-boot watchdog. */
	_wdt_stop();
	cl_write(_TCONFIG, 0);
	cl_write(_TCONFIG, (1UL << 30) | (1UL << 11) | (_TDIV << 13));
	cl_write(_TLOADLO, 0);
	cl_write(_TLOADHI, 0);
	cl_write(_TLOAD, 1);
	cl_write(_TCLR, 1UL);
	cl_write(_TENA, 1UL);
	cl_write(_TCONFIG, (1UL << 31) | (1UL << 30) | (1UL << 11) | (_TDIV << 13));
	_arm(_TPERIOD);
}

/* core 0 calls this inside the tree change; a stopped timer takes the
 * divider when it starts */
void cl_cpu1_rate(void) {
	unsigned long conf = cl_read(_TCONFIG);

	if(!(conf & (1UL << 31)))
		return;
	cl_write(_TCONFIG, conf & ~(1UL << 31));
	conf &= ~(0xFFFFUL << 13);
	conf |= _TDIV << 13;
	cl_write(_TCONFIG, conf);
}

/* Route this core's timer and scheduler onto its level-2 lines, and keep
 * the other core's sources off those lines. A default map lands on line
 * 16, which this core never enables, but the two lines it does enable
 * must not alias. */
static void _matrix(void) {
	cl_write(CL_INTMTX_APP + (unsigned long)CL_SRC_TG1_T0 * 4UL, CL_CPU_TICK);
	cl_write(CL_INTMTX_APP + (unsigned long)CL_SRC_FROM_CPU1 * 4UL, CL_CPU_SCHED);
	cl_write(CL_INTMTX_APP + (unsigned long)CL_SRC_TG0_T0 * 4UL, 31);
	cl_write(CL_INTMTX_APP + (unsigned long)CL_SRC_FROM_CPU * 4UL, 31);
	cl_write(CL_INTMTX + (unsigned long)CL_SRC_TG1_T0 * 4UL, 31);
	cl_write(CL_INTMTX + (unsigned long)CL_SRC_FROM_CPU1 * 4UL, 31);
}

static void _signal(int cpu) {
	if(cpu == 0)
		cl_write(CL_FROM_CPU, 1);
	else if(cpu == 1)
		cl_write(CL_FROM_CPU1, 1);
}

/* RTC software stall is a second hold, beside the DPORT reset. The ROM
 * leaves it clear. Clearing it here is the unstall IDF does before release. */
static void _rtc_unstall(void) {
	cl_clr(CL_RTC + 0x00UL, 3UL);
	cl_clr(CL_RTC + 0xACUL, 0x3FUL << 20);
}

/* The ROM helper only stores the address at CTRL_D. The CPU samples it
 * once it is out of reset, so the store comes after the clock is on and
 * the reset bit has been released. A second reset pulse covers the case
 * where the sample happened on the way out of reset. */
static void _release(void) {
	_rtc_unstall();
	cl_write(CL_APP_B, 1);
	cl_write(CL_APP_C, 0);
	cl_write(CL_APP_A, 1);
	(void)cl_read(CL_APP_A);
	cl_write(CL_APP_A, 0);
	(void)cl_read(CL_APP_A);
	cl_write(CL_APP_D, (unsigned long)cl_cpu1_entry);
	_memw();
	cl_write(CL_APP_A, 1);
	(void)cl_read(CL_APP_A);
	cl_write(CL_APP_A, 0);
	_memw();
}

static void _reset(void) {
	cl_write(CL_APP_A, 1);
	cl_write(CL_APP_C, 1);
	cl_write(CL_APP_B, 0);
}

void cl_cpu1_main(void) {
	unsigned long lines = (1UL << CL_CPU_TICK) | (1UL << CL_CPU_SCHED);

	mp_interrupt_disable();
	__asm__ volatile ("wsr.intenable %0" :: "r"(lines) : "memory");
	_memw();
	_up = 1;
	_memw();
	if(_entry)
		_entry();
	for(;;)
		mp_cpu_idle();
}

void cl_cpu1_tick(void) {
	unsigned long long now;
	unsigned long long next;

	cl_write(_TCLR, 1UL);
	now = _now();
	next = _alarm + _TPERIOD;
	if(next <= now)
		next = now + _TPERIOD;
	_arm(next);
	if(cl_cpu_should_park()) {
		cl_cpu_park_spin();
		return;
	}
	mp_task_slice();
}

int cl_cpu_should_park(void) {
	return(_hold != 0);
}

void cl_cpu_park_spin(void) {
	unsigned long ps;

	__asm__ volatile ("rsil %0, 15" : "=r"(ps) :: "memory");
	_parked = 1;
	_memw();
	while(_hold)
		;
	_parked = 0;
	_memw();
	__asm__ volatile ("wsr %0, ps\n\trsync" :: "r"(ps) : "memory");
}

int mp_cpu_id(void) {
	return(cl_cpu_id());
}

void mp_cpu_idle_note(unsigned long us) {
	_idle_us[cl_cpu_id()] += us;
}

void mp_cpu_idle(void) {
	unsigned long from = mp_clock_us();

	__asm__ volatile ("waiti 0" ::: "memory");
	mp_cpu_idle_note(mp_clock_us() - from);
}

int mp_cpu_load(int cpu) {
	unsigned long now;
	unsigned long span;
	unsigned long idle;
	int load;

	if(cpu < 0 || cpu >= mp_task_cpus())
		return(-1);
	MP_INTERRUPT_SAFE_BEGIN
	now = mp_clock_us();
	span = now - _seen_at[cpu];
	if(!_seen_at[cpu] || span >= 250000UL) {
		idle = _idle_us[cpu] - _seen_idle[cpu];
		if(_seen_at[cpu] && span) {
			if(idle > span)
				idle = span;
			_load[cpu] = (int)(100UL - (unsigned long)(((unsigned long long)idle * 100ULL) / span));
		}
		_seen_at[cpu] = now ? now : 1;
		_seen_idle[cpu] = _idle_us[cpu];
	}
	load = _load[cpu];
	MP_INTERRUPT_SAFE_END
	return(load);
}

void mp_cpu_kick(int cpu) {
	if(cpu == cl_cpu_id())
		return;
	_signal(cpu);
}

void mp_cpu_pend(int cpu) {
	if(cpu == cl_cpu_id())
		return;
	_signal(cpu);
}

mp_bool_t mp_cpu_park(void) {
	unsigned long until;

	if(!_up)
		return(YES);
	_parked = 0;
	_memw();
	_hold = 1;
	_memw();
	_signal(1);
	until = mp_clock_ticks() + 200;
	while(!_parked) {
		if((long)(mp_clock_ticks() - until) >= 0) {
			_hold = 0;
			_memw();
			return(NO);
		}
	}
	return(YES);
}

void mp_cpu_unpark(void) {
	_hold = 0;
	_memw();
}

void mp_cpu_retick(void) {
}

mp_bool_t mp_cpu_start(int cpu, void (*entry)(void)) {
	unsigned long until;

	if(cpu != 1 || !entry)
		return(NO);
	if(_up)
		return(YES);
	_entry = entry;
	_hold = 0;
	_memw();
	_matrix();
	_timer();
	_release();
	until = mp_clock_ticks() + 50;
	while(!_up) {
		if((long)(mp_clock_ticks() - until) >= 0) {
			_reset();
			_entry = NULL;
			return(NO);
		}
	}
	return(YES);
}

#else

void cl_cpu1_tick(void) {
}

int cl_cpu_should_park(void) {
	return(0);
}

void cl_cpu_park_spin(void) {
}

#endif

/**@}*/
