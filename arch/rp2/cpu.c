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
@defgroup mpArchRP2350Cpu Second core

@ingroup mpArchRP2350

Core 1 waits in the bootrom until core 0 hands it a vector table, a stack
and an entry through the SIO mailbox (datasheet 5.3). Each core then has
its own NVIC, SysTick and PendSV. The mailbox interrupt is the wake-up
line between them: one word in the other core's FIFO. The SIO hardware
spinlocks are left alone (erratum RP2350-E2), the task lock uses the
exclusive monitor on SRAM instead.

@{
*/

#if MP_CPUS > 1

#define _MP_CPU_STACK 8192
#define _MP_CPU_SPINS 2000000UL

static unsigned char _mp_cpu_stack[_MP_CPU_STACK] __attribute__((aligned(16)));
static void (*_mp_cpu_entry)(void);
static uint32_t _mp_cpu_vtor;
static unsigned long _mp_cpu_counts;

int mp_cpu_id(void) {
	return((int)SIO_CPUID);
}

/* Microseconds each core spent in WFI. Only its own core adds to it. */
static volatile unsigned long _mp_cpu_idle_us[MP_CPUS];
static unsigned long _mp_cpu_seen_idle[MP_CPUS];
static unsigned long _mp_cpu_seen_at[MP_CPUS];
static int _mp_cpu_load[MP_CPUS] = { [0 ... MP_CPUS - 1] = -1 };

void mp_cpu_idle_note(unsigned long us) {
	_mp_cpu_idle_us[SIO_CPUID] += us;
}

void mp_cpu_idle(void) {
	unsigned long from = mp_clock_us();

	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("wfi");
	__asm volatile ("isb");
	mp_cpu_idle_note(mp_clock_us() - from);
}

/* Busy share since the previous sample, a sample every 250 ms at most.
 * Interrupt time counts as busy: the WFI ends before the handler runs. */
int mp_cpu_load(int cpu) {
	unsigned long now;
	unsigned long span;
	unsigned long idle;
	int load;

	if(cpu < 0 || cpu >= mp_task_cpus())
		return(-1);
	MP_INTERRUPT_SAFE_BEGIN
	now = mp_clock_us();
	span = now - _mp_cpu_seen_at[cpu];
	if(!_mp_cpu_seen_at[cpu] || span >= 250000UL) {
		idle = _mp_cpu_idle_us[cpu] - _mp_cpu_seen_idle[cpu];
		if(_mp_cpu_seen_at[cpu] && span) {
			if(idle > span)
				idle = span;
			_mp_cpu_load[cpu] = (int)(100UL - (unsigned long)(((unsigned long long)idle * 100ULL) / span));
		}
		_mp_cpu_seen_at[cpu] = now ? now : 1;
		_mp_cpu_seen_idle[cpu] = _mp_cpu_idle_us[cpu];
	}
	load = _mp_cpu_load[cpu];
	MP_INTERRUPT_SAFE_END
	return(load);
}

/* Mailbox words. A line request carries the vector and on/off. */
#define _MP_CPU_MSG_KICK 1UL
#define _MP_CPU_MSG_PARK 2UL
#define _MP_CPU_MSG_PEND 3UL
#define _MP_CPU_MSG_TICK 4UL
#define _MP_CPU_MSG_LINE 0x80000000UL

static volatile int _mp_cpu_launched;
static volatile int _mp_cpu_hold;
static volatile int _mp_cpu_parked;

static int _mp_cpu_push(uint32_t word);

/* Two cores: the FIFO of this one only reaches the other. A full FIFO
 * already holds a wake-up. */
void mp_cpu_kick(int cpu) {
	if(cpu == (int)SIO_CPUID)
		return;
	if(SIO_FIFO_ST & SIO_FIFO_ST_RDY)
		SIO_FIFO_WR = _MP_CPU_MSG_KICK;
	__asm volatile ("sev");
}

/* A pend must land, unlike a wake-up: wait for room in the FIFO. */
void mp_cpu_pend(int cpu) {
	if(cpu == (int)SIO_CPUID)
		return;
	(void)_mp_cpu_push(_MP_CPU_MSG_PEND);
}

void mp_cpu_line(int vector, int on) {
	(void)_mp_cpu_push(_MP_CPU_MSG_LINE | (on ? 0x100UL : 0UL) | ((uint32_t)vector & 0xffUL));
}

/* Runs from RAM with this core's interrupts masked: the other core has the
 * flash off the bus, and a vector fetch would come from it. */
__attribute__((noinline, long_call, aligned(4), section(".data.ramfunc")))
static void _mp_cpu_park_ram(void) {
	uint32_t primask;

	__asm volatile ("mrs %0, primask" : "=r" (primask));
	__asm volatile ("cpsid i" ::: "memory");
	_mp_cpu_parked = 1;
	__asm volatile ("dsb" ::: "memory");
	while(_mp_cpu_hold)
		__asm volatile ("wfe");
	_mp_cpu_parked = 0;
	__asm volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

mp_bool_t mp_cpu_park(void) {
	unsigned long spins = _MP_CPU_SPINS;

	if(!_mp_cpu_launched)
		return(YES);
	_mp_cpu_parked = 0;
	_mp_cpu_hold = 1;
	__asm volatile ("dsb" ::: "memory");
	if(!_mp_cpu_push(_MP_CPU_MSG_PARK)) {
		_mp_cpu_hold = 0;
		return(NO);
	}
	while(!_mp_cpu_parked) {
		if(!spins--) {
			_mp_cpu_hold = 0;
			__asm volatile ("sev");
			return(NO);
		}
	}
	return(YES);
}

void mp_cpu_unpark(void) {
	_mp_cpu_hold = 0;
	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("sev");
}

void mp_cpu_retick(void) {
	if(mp_task_cpus() < 2)
		return;
	if(!(SIO_FIFO_ST & SIO_FIFO_ST_RDY))
		return;
	SIO_FIFO_WR = _MP_CPU_MSG_TICK;
	__asm volatile ("sev");
}

void SIO_IRQ_FIFO_Handler(void) {
	uint32_t word;
	int pend = 0;

	while(SIO_FIFO_ST & SIO_FIFO_ST_VLD) {
		word = SIO_FIFO_RD;
		if(word == _MP_CPU_MSG_PARK)
			_mp_cpu_park_ram();
		else if(word & _MP_CPU_MSG_LINE) {
			int vector = (int)(word & 0xffUL);

			if(vector < MP_ARCH_IRQ_COUNT) {
				if(word & 0x100UL) {
					NVIC->ICPR[vector >> 5] = 1UL << (vector & 31);
					NVIC->ISER[vector >> 5] = 1UL << (vector & 31);
				}
				else
					NVIC->ICER[vector >> 5] = 1UL << (vector & 31);
			}
		}
		else if(word == _MP_CPU_MSG_PEND)
			pend = 1;
		else if(word == _MP_CPU_MSG_TICK)
			mp_clock_core_tick();
	}
	SIO_FIFO_ST = 0xff;
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	/* the other core woke a task above this one's running priority */
	if(pend)
		mp_task_pend();
#else
	(void)pend;
#endif
}

static void _mp_cpu_fifo_irq(void) {
	while(SIO_FIFO_ST & SIO_FIFO_ST_VLD)
		(void)SIO_FIFO_RD;
	SIO_FIFO_ST = 0xff;
	NVIC->IP[SIO_IRQ_FIFO] = 0x80;
	NVIC->ICPR[SIO_IRQ_FIFO >> 5] = 1UL << (SIO_IRQ_FIFO & 31);
	NVIC->ISER[SIO_IRQ_FIFO >> 5] = 1UL << (SIO_IRQ_FIFO & 31);
}

/* Core 1 runs this on the stack the bootrom set. Banked state first:
 * FPU access, fault handlers, priorities, its own SysTick that only
 * wakes it every tick. Core 0 keeps the tick count. */
static void __attribute__((used)) _mp_cpu1_main(void) {
	ACTLR |= ACTLR_EXTEXCLALL;
	CPACR |= CPACR_FPU_FULL;
	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("isb" ::: "memory");
	FPCCR = (FPCCR | FPCCR_ASPEN) & ~FPCCR_LSPEN;
	SCB->SHCSR |= (1UL << 16) | (1UL << 17) | (1UL << 18);
	SCB->VTOR = _mp_cpu_vtor;
	SCB->AIRCR = (0x05FAUL << 16) | (3UL << 8);

	_mp_cpu_fifo_irq();
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	/* the scheduler line, under every interrupt like core 0's */
	SCB->SHP[10] = 0xF0;
	SCB->ICSR = SCB_ICSR_PENDSVCLR;
#endif

	SysTick->CTRL = 0;
	SysTick->LOAD = _mp_cpu_counts - 1;
	SysTick->VAL = 0;
	SCB->SHP[11] = 0x40;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;

	__asm volatile ("cpsie i" ::: "memory");
	_mp_cpu_entry();
	for(;;)
		mp_cpu_idle();
}

/* The bootrom may leave a stack limit behind, as it does on core 0. */
static void __attribute__((naked, used)) _mp_cpu1_reset(void) {
	__asm volatile(
		"movs r0, #0\n"
		"msr msplim, r0\n"
		"msr psplim, r0\n"
		"b _mp_cpu1_main\n"
	);
}

static int _mp_cpu_push(uint32_t word) {
	unsigned long spins = _MP_CPU_SPINS;

	while(!(SIO_FIFO_ST & SIO_FIFO_ST_RDY))
		if(!spins--)
			return(0);
	SIO_FIFO_WR = word;
	__asm volatile ("sev");
	return(1);
}

static int _mp_cpu_pop(uint32_t *word) {
	unsigned long spins = _MP_CPU_SPINS;

	while(!(SIO_FIFO_ST & SIO_FIFO_ST_VLD))
		if(!spins--)
			return(0);
	*word = SIO_FIFO_RD;
	return(1);
}

mp_bool_t mp_cpu_start(int cpu, void (*entry)(void)) {
	uint32_t seq[6];
	uint32_t answer;
	unsigned long spins;
	int step;
	int tries;

	if(cpu != 1 || !entry)
		return(NO);

	_mp_cpu_entry = entry;
	_mp_cpu_vtor = SCB->VTOR;
	_mp_cpu_counts = mp_clock_get_speed() / mp_clock_tick_hz();
	if(_mp_cpu_counts < 2)
		_mp_cpu_counts = 2;

	/* Power cycle core 1 so it sits in the bootrom whatever ran before.
	 * Out of reset it drains its FIFO and posts a 0 to ours. */
	PSM_FRCE_OFF_SET = PSM_PROC1;
	spins = _MP_CPU_SPINS;
	while(!(PSM_FRCE_OFF & PSM_PROC1) && spins--)
		;
	PSM_FRCE_OFF_CLR = PSM_PROC1;
	(void)_mp_cpu_pop(&answer);

	seq[0] = 0;
	seq[1] = 0;
	seq[2] = 1;
	seq[3] = _mp_cpu_vtor;
	seq[4] = (uint32_t)(_mp_cpu_stack + _MP_CPU_STACK);
	seq[5] = (uint32_t)_mp_cpu1_reset;

	step = 0;
	for(tries = 0; step < 6 && tries < 64; tries++) {
		if(seq[step] == 0) {
			while(SIO_FIFO_ST & SIO_FIFO_ST_VLD)
				(void)SIO_FIFO_RD;
			__asm volatile ("sev");
		}
		if(!_mp_cpu_push(seq[step]) || !_mp_cpu_pop(&answer)) {
			step = 0;
			continue;
		}
		step = answer == seq[step] ? step + 1 : 0;
	}
	if(step < 6)
		return(NO);

	_mp_cpu_fifo_irq();
	_mp_cpu_launched = 1;
	return(YES);
}

#endif

/**@}*/
