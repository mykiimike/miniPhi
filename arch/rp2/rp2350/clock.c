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

/* arch/rp2/usb.c. YES once the host has set a configuration. */
mp_bool_t mp_usb_linked(void);

/**
@defgroup mpArchRP2350Clock Clock

@ingroup mpArchRP2350

clk_ref is the 12 MHz crystal. clk_usb stays on PLL_USB at 48 MHz.
clk_sys is one of three trees the governor selects: the crystal at
12 MHz, PLL_USB at 48 MHz, or PLL_SYS at 150 MHz. Boot finishes on
PLL_SYS. The flash divisor follows the rate so XIP does not speed up
with the mux. Below 48 MHz the USB device misses descriptors, so an
enumerated console holds the floor at 48. With the cable unplugged,
SLEEP can stop the crystal; the always-on timer wakes it.

@{
*/

#define _SYSTICK_MAX 0xFFFFFFUL

static volatile unsigned long __ticks = 0;
static unsigned long __sysclk = RP2350_XOSC_HZ;
static volatile unsigned long __tickCounts = RP2350_XOSC_HZ / RP2350_TICK_RATE_HZ;
static const char *__blocker = NULL;
static mp_bool_t __usb48 = NO;
static int __timerOk;
static mp_kernel_t *__kernel;
/* 0 crystal, 1 PLL_USB, 2 PLL_SYS. Boot has not switched yet. */
static int __src;
static uint32_t __aon_span;
static int __pll_fails;
static volatile unsigned long __us_skip;

static int _mp_clock_wait(volatile uint32_t *reg, uint32_t mask) {
	unsigned int spins = 10000000;

	while(spins-- && !(*reg & mask));
	return((*reg & mask) != 0);
}

/* Crystal on, clock tree untouched. */
static int _mp_clock_xosc_on(void) {
	if(XOSC_STATUS & XOSC_STATUS_STABLE)
		return(1);
	XOSC_STARTUP = XOSC_STARTUP_DELAY;
	XOSC_CTRL = XOSC_CTRL_ENABLE | XOSC_CTRL_RANGE_12MHZ;
	return(_mp_clock_wait(&XOSC_STATUS, XOSC_STATUS_STABLE));
}

/* clk_sys in Hz: core cycles while the crystal counter runs down 1 ms.
 * 0 when the counter does not behave. */
static unsigned long _mp_clock_measure(void) {
	uint32_t start;
	uint32_t cycles;
	unsigned long hz;

	XOSC_COUNT = RP2350_XOSC_HZ / 1000UL;
	if(XOSC_COUNT == 0 || XOSC_COUNT > RP2350_XOSC_HZ / 1000UL)
		return(0);
	start = DWT->CYCCNT;
	while(XOSC_COUNT != 0) {
		if(DWT->CYCCNT - start > 2000000UL)
			return(0);
	}
	cycles = DWT->CYCCNT - start;
	hz = (unsigned long)cycles * 1000UL;
	if(hz < 1000000UL || hz > 300000000UL)
		return(0);
	return(hz);
}

/* clk_ref onto the crystal. clk_sys is parked on the ring oscillator
 * first when it still follows clk_ref, so the mux change does not glitch
 * the XIP fetch. PLL_SYS is not started. */
static int _mp_clock_ref_xosc(void) {
	if(CLOCKS_CLK_REF_SELECTED & (1UL << CLOCKS_REF_SRC_XOSC))
		return(1);
	if((CLOCKS_CLK_SYS_CTRL & 1UL) == CLOCKS_SYS_SRC_REF) {
		CLOCKS_CLK_SYS_CTRL = (CLOCKS_SYS_AUX_ROSC << 5) | CLOCKS_SYS_SRC_REF;
		CLOCKS_CLK_SYS_CTRL = (CLOCKS_SYS_AUX_ROSC << 5) | CLOCKS_SYS_SRC_AUX;
		if(!_mp_clock_wait(&CLOCKS_CLK_SYS_SELECTED, 1UL << CLOCKS_SYS_SRC_AUX))
			return(0);
	}
	CLOCKS_CLK_REF_DIV = CLOCKS_DIV_INT1;
	CLOCKS_CLK_REF_CTRL = CLOCKS_REF_SRC_XOSC;
	if(!_mp_clock_wait(&CLOCKS_CLK_REF_SELECTED, 1UL << CLOCKS_REF_SRC_XOSC))
		return(0);
	CLOCKS_CLK_SYS_DIV = CLOCKS_DIV_INT1;
	CLOCKS_CLK_SYS_CTRL = (CLOCKS_SYS_AUX_ROSC << 5) | CLOCKS_SYS_SRC_REF;
	return(_mp_clock_wait(&CLOCKS_CLK_SYS_SELECTED, 1UL << CLOCKS_SYS_SRC_REF) != 0);
}

/* QMI window 0. CLKDIV is clk_sys cycles per flash SCK period. Reset is 4. */
#define _QMI_M0_TIMING RP2350_REG(0x400d0000UL, 0x0c)
#define _SYS_AUX_PLL_USB 1UL
#define _SYS_HZ_USB 48000000UL

/* Keep the flash SCK period. CLKDIV counts clk_sys cycles per period.
 * A larger divisor is not in force until a window access completes, so
 * the read below happens before the caller speeds clk_sys up. */
static void _mp_clock_qmi_commit(unsigned long div) {
	uint32_t timing;

	if(div < 2)
		div = 2;
	if(div > 255)
		div = 255;
	timing = _QMI_M0_TIMING;
	_QMI_M0_TIMING = (timing & ~0xffUL) | (uint32_t)div;
	(void)*(volatile uint32_t *)0x10000000UL;
	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("isb" ::: "memory");
}

static void _mp_clock_qmi_scale(unsigned long was, unsigned long now) {
	unsigned long div;

	if(!was || !now)
		return;
	div = _QMI_M0_TIMING & 0xffUL;
	if(!div)
		div = 4;
	if(now > was)
		div = (unsigned long)(((unsigned long long)div * now + was - 1ULL) / was);
	else
		div = (unsigned long)(((unsigned long long)div * now) / was);
	_mp_clock_qmi_commit(div);
}

/* clk_sys onto PLL_USB. The glitchless mux has to leave its aux input
 * before AUXSRC changes: the bootrom already sits on that input (PLL_SYS),
 * so a wait for "aux selected" would succeed without the clock moving.
 * The tick would then be programmed for 48 MHz while the core still ran
 * at 150, and every 5 s page would last a bit over a second. */
static int _mp_clock_sys_on_usb(void) {
	if((CLOCKS_CLK_SYS_CTRL & 1UL) != CLOCKS_SYS_SRC_REF) {
		uint32_t keep = (CLOCKS_CLK_SYS_CTRL >> 5) & 7UL;

		CLOCKS_CLK_SYS_CTRL = (keep << 5) | CLOCKS_SYS_SRC_REF;
		if(!_mp_clock_wait(&CLOCKS_CLK_SYS_SELECTED, 1UL << CLOCKS_SYS_SRC_REF))
			return(0);
	}
	CLOCKS_CLK_SYS_CTRL = (_SYS_AUX_PLL_USB << 5) | CLOCKS_SYS_SRC_REF;
	CLOCKS_CLK_SYS_CTRL = (_SYS_AUX_PLL_USB << 5) | CLOCKS_SYS_SRC_AUX;
	return(_mp_clock_wait(&CLOCKS_CLK_SYS_SELECTED, 1UL << CLOCKS_SYS_SRC_AUX) != 0);
}

/* The USB device controller answers the host from clk_sys. At 12 MHz it
 * connects and then misses the descriptor. PLL_USB is already 48 MHz.
 * The flash divisor moves only once the new rate is known, and only in
 * the direction that does not make XIP faster mid-switch. */
static int _mp_clock_sys_48(unsigned long *hz) {
	unsigned long was = *hz;
	unsigned long measured;
	int slowing;

	if(!was)
		was = RP2350_XOSC_HZ;
	slowing = was > _SYS_HZ_USB;
	if(!slowing)
		_mp_clock_qmi_scale(was, _SYS_HZ_USB);
	if(!_mp_clock_sys_on_usb())
		return(0);
	measured = _mp_clock_measure();
	if(!measured)
		measured = _SYS_HZ_USB;
	if(slowing && measured < was)
		_mp_clock_qmi_scale(was, measured);
	*hz = measured;
	return(1);
}

/* REFDIV 1, FBDIV 125, VCO 1500 MHz, POSTDIV1 5, POSTDIV2 2.
 * The mux stays where it is: a running aux input must be left before
 * AUXSRC changes, and the caller does that. */
static int _mp_clock_pll_start(void) {
	if((PLL_SYS_CS & PLL_CS_LOCK) && !(PLL_SYS_PWR & PLL_PWR_PD))
		return(1);

	RESETS_RESET |= RESETS_PLL_SYS;
	RESETS_RESET &= ~RESETS_PLL_SYS;
	if(!_mp_clock_wait(&RESETS_RESET_DONE, RESETS_PLL_SYS))
		return(0);

	PLL_SYS_CS = 1;
	PLL_SYS_FBDIV_INT = 125;
	PLL_SYS_PWR &= ~(PLL_PWR_PD | PLL_PWR_VCOPD);
	if(!_mp_clock_wait(&PLL_SYS_CS, PLL_CS_LOCK))
		return(0);

	PLL_SYS_PRIM = (5UL << PLL_PRIM_POSTDIV1_LSB) | (2UL << PLL_PRIM_POSTDIV2_LSB);
	PLL_SYS_PWR &= ~PLL_PWR_POSTDIVPD;
	return(1);
}

/* Glitchless mux. AUXSRC is written only while clk_sys is on clk_ref. */
static int _mp_clock_on_ref(void) {
	if((CLOCKS_CLK_SYS_CTRL & 1UL) == CLOCKS_SYS_SRC_REF)
		return(1);

	{
		uint32_t keep = (CLOCKS_CLK_SYS_CTRL >> 5) & 7UL;

		CLOCKS_CLK_SYS_CTRL = (keep << 5) | CLOCKS_SYS_SRC_REF;
	}
	return(_mp_clock_wait(&CLOCKS_CLK_SYS_SELECTED, 1UL << CLOCKS_SYS_SRC_REF) != 0);
}

static int _mp_clock_on_aux(uint32_t aux) {
	if(!_mp_clock_on_ref())
		return(0);
	CLOCKS_CLK_SYS_CTRL = (aux << 5) | CLOCKS_SYS_SRC_REF;
	CLOCKS_CLK_SYS_CTRL = (aux << 5) | CLOCKS_SYS_SRC_AUX;
	if(!_mp_clock_wait(&CLOCKS_CLK_SYS_SELECTED, 1UL << CLOCKS_SYS_SRC_AUX))
		return(0);
	if(__usb48 == YES) {
		CLOCKS_CLK_PERI_CTRL = CLOCKS_PERI_ENABLE | (CLOCKS_PERI_AUX_USB << 5);
		CLOCKS_CLK_PERI_DIV = CLOCKS_DIV_INT1;
	}
	return(1);
}

/* REFDIV 1, FBDIV 100, VCO 1200 MHz, POSTDIV1 5, POSTDIV2 5.
 * 480 MHz (FBDIV 40) is under the 750 MHz VCO floor, so it is not used.
 * AUXSRC 0 is PLL_USB already; ENABLE starts clk_usb. CLK_USB_CTRL is
 * CLOCKS+0x60 because CLK_HSTX sits at 0x54 on this chip. */
int mp_arch_pll_usb_init(void) {
	if(__usb48 == YES)
		return(1);

	RESETS_RESET |= RESETS_PLL_USB;
	RESETS_RESET &= ~RESETS_PLL_USB;
	if(!_mp_clock_wait(&RESETS_RESET_DONE, RESETS_PLL_USB))
		return(0);

	PLL_USB_CS = 1;
	PLL_USB_FBDIV_INT = 100;
	PLL_USB_PWR &= ~(PLL_PWR_PD | PLL_PWR_VCOPD);
	if(!_mp_clock_wait(&PLL_USB_CS, PLL_CS_LOCK))
		return(0);

	PLL_USB_PRIM = (5UL << PLL_PRIM_POSTDIV1_LSB) | (5UL << PLL_PRIM_POSTDIV2_LSB);
	PLL_USB_PWR &= ~PLL_PWR_POSTDIVPD;

	CLOCKS_CLK_USB_DIV = CLOCKS_DIV_INT1;
	CLOCKS_CLK_USB_CTRL = CLOCKS_USB_ENABLE | (CLOCKS_USB_AUX_PLL << 5);
	if(!_mp_clock_wait(&CLOCKS_CLK_USB_CTRL, CLOCKS_USB_ENABLED))
		return(0);

	__usb48 = YES;
	/* SPI and UART stay on this 48 MHz clock. Following clk_sys up to
	 * 150 MHz makes the panel's status poll miss and the glass crawl. */
	CLOCKS_CLK_PERI_CTRL = CLOCKS_PERI_ENABLE | (CLOCKS_PERI_AUX_USB << 5);
	CLOCKS_CLK_PERI_DIV = CLOCKS_DIV_INT1;
	return(1);
}

static void _mp_clock_tick(unsigned long hz) {
	__sysclk = hz;
	__tickCounts = hz / RP2350_TICK_RATE_HZ;
	if(__tickCounts < 2)
		__tickCounts = 2;

	SysTick->CTRL = 0;
	SysTick->LOAD = __tickCounts - 1;
	SysTick->VAL = 0;
	SCB->SHP[11] = 0x40;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;
}

/* This core only. Core 1 is banked and follows through the mailbox. */
static void _mp_clock_tick_retime(void) {
	unsigned long counts = __sysclk / RP2350_TICK_RATE_HZ;
	unsigned long left;

	if(counts < 2)
		counts = 2;

	MP_INTERRUPT_SAFE_BEGIN
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE;
	left = (unsigned long)((unsigned long long)SysTick->VAL * counts / __tickCounts);
	if(left < 2)
		left = 2;
	SysTick->LOAD = left - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE;
	__tickCounts = counts;
	SysTick->LOAD = counts - 1;
	MP_INTERRUPT_SAFE_END
#if MP_CPUS > 1
	mp_cpu_retick();
#endif
}

void mp_clock_core_tick(void) {
	unsigned long counts = __tickCounts;

	if(counts < 2)
		return;
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE;
	SysTick->LOAD = counts - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;
}

#define _HZ_XOSC 12000000UL
#define _HZ_PLL  150000000UL

static unsigned long _mp_clock_nominal(int src) {
	if(src == 2)
		return(_HZ_PLL);
	if(src == 1)
		return(_SYS_HZ_USB);
	return(_HZ_XOSC);
}

static int _mp_clock_mux(int src) {
	if(src == 2) {
		if(!_mp_clock_pll_start())
			return(0);
		return(_mp_clock_on_aux(CLOCKS_SYS_AUX_PLL));
	}
	if(src == 1) {
		if(!mp_arch_pll_usb_init())
			return(0);
		return(_mp_clock_on_aux(_SYS_AUX_PLL_USB));
	}
	if(!_mp_clock_on_ref())
		return(0);
	if(__usb48 == YES) {
		CLOCKS_CLK_PERI_CTRL = CLOCKS_PERI_ENABLE | (CLOCKS_PERI_AUX_USB << 5);
		CLOCKS_CLK_PERI_DIV = CLOCKS_DIV_INT1;
	}
	return(1);
}

static int _mp_clock_close(unsigned long target, unsigned long measured) {
	unsigned long low = target - target / 5;
	unsigned long high = target + target / 5;

	return(measured >= low && measured <= high);
}

/* Runs with the kernel lock held. On a bad measurement the previous
 * tree is put back and the flash divisor with it. */
static int _mp_clock_set_locked(int src) {
	unsigned long was = __sysclk;
	unsigned long target = _mp_clock_nominal(src);
	unsigned long measured;
	int prev = __src;
	int slowing;
	mp_bool_t state;

	if(src == prev)
		return(1);
	if(!was)
		was = target;
	slowing = was > target;
	if(!slowing)
		_mp_clock_qmi_scale(was, target);
	if(!_mp_clock_mux(src)) {
		(void)_mp_clock_mux(prev);
		if(!slowing)
			_mp_clock_qmi_scale(target, was);
		__blocker = "clk_sys";
		if(src == 2)
			__pll_fails++;
		return(0);
	}

	state = mp_interrupt_state();
	mp_interrupt_disable();
	measured = _mp_clock_measure();
	mp_interrupt_restore(state);
	if(!_mp_clock_close(target, measured)) {
		(void)_mp_clock_mux(prev);
		if(!slowing)
			_mp_clock_qmi_scale(target, was);
		__blocker = "measure";
		if(src == 2)
			__pll_fails++;
		return(0);
	}
	if(slowing)
		_mp_clock_qmi_scale(was, measured);
	__sysclk = measured;
	__src = src;
	if(__kernel)
		__kernel->arch.sysclk = measured;
	_mp_clock_tick_retime();
	mp_uart_retime();
	mp_i2c_retime();
	mp_spi_retime();
	mp_timer_retime();
	if(src == 2)
		__pll_fails = 0;
	__blocker = NULL;
	return(1);
}

static int _mp_clock_set(int src) {
	int ok;

	mp_task_cpu_lock();
	ok = _mp_clock_set_locked(src);
	mp_task_cpu_unlock();
	return(ok);
}

/* POWMAN is the always-on domain. Writes carry the password in 31:16.
 * The timer is whatever the reset LPOSC calibration makes it; the span
 * is ticks per 2 ms, measured, so the alarm does not depend on a guess. */
#define _POWMAN 0x40100000UL
#define _POWMAN_PASS 0x5afe0000UL
#define _XOSC_DORMANT RP2350_REG(RP2350_XOSC_BASE, 0x08)
#define _XOSC_DORMANT_STOP 0x636f6d61UL

static void _mp_powman(unsigned off, uint32_t value) {
	RP2350_REG(_POWMAN, off) = _POWMAN_PASS | value;
}

static uint32_t _mp_powman_read(unsigned off) {
	return(RP2350_REG(_POWMAN, off));
}

static void _mp_clock_aon_probe(void) {
	uint32_t start;

	__aon_span = 0;
	if(!__timerOk)
		return;
	_mp_powman(0x6c, 0);
	_mp_powman(0x68, 0);
	_mp_powman(0x64, 0);
	_mp_powman(0x60, 0);
	_mp_powman(0x88, 2);
	start = TIMER0_TIMERAWL;
	while((uint32_t)(TIMER0_TIMERAWL - start) < 2000)
		;
	if(_mp_powman_read(0x74) >= 1)
		__aon_span = _mp_powman_read(0x74);
}

static void _mp_clock_alarm_off(void) {
	uint32_t ctrl = _mp_powman_read(0x88);

	_mp_powman(0x88, (ctrl & ~(0x10UL | 0x20UL)) | 0x40UL | 2UL);
}

static int _mp_clock_alarm(unsigned long ms) {
	uint32_t upper;
	uint32_t lower;
	uint32_t again;
	uint64_t now;
	uint64_t at;
	uint64_t add;

	if(!__aon_span || !ms)
		return(0);
	upper = _mp_powman_read(0x70);
	lower = _mp_powman_read(0x74);
	again = _mp_powman_read(0x70);
	if(again != upper) {
		upper = again;
		lower = _mp_powman_read(0x74);
	}
	now = ((uint64_t)upper << 32) | lower;
	add = (uint64_t)ms * __aon_span / 2ULL;
	if(!add)
		add = 1;
	at = now + add;
	_mp_clock_alarm_off();
	_mp_powman(0x84, (uint32_t)(at & 0xffffULL));
	_mp_powman(0x80, (uint32_t)((at >> 16) & 0xffffULL));
	_mp_powman(0x7c, (uint32_t)((at >> 32) & 0xffffULL));
	_mp_powman(0x78, (uint32_t)((at >> 48) & 0xffffULL));
	_mp_powman(0x88, 2UL | 0x10UL | 0x20UL);
	return((_mp_powman_read(0x88) & 0x10UL) != 0);
}

void POWMAN_IRQ_TIMER_Handler(void) {
	_mp_clock_alarm_off();
}

/* Crystal off. Core 1 is parked first so it is not fetching when the
 * clocks stop. The alarm is armed only after the mux is on the ring,
 * and a rejected password skips the stop. */
static void _mp_clock_dormant(unsigned long ms) {
	int parked = 0;
	int src = __src;

	if(ms > 500)
		ms = 500;
	if(ms < 2 || src != 0)
		return;
#if MP_CPUS > 1
	if(mp_task_cpus() > 1) {
		if(mp_cpu_park() == NO)
			return;
		parked = 1;
	}
#endif
	if(!_mp_clock_on_aux(CLOCKS_SYS_AUX_ROSC))
		goto out;
	CLOCKS_CLK_REF_CTRL = 0;
	if(!_mp_clock_wait(&CLOCKS_CLK_REF_SELECTED, 1UL))
		goto restore;
	if(!_mp_clock_alarm(ms))
		goto restore;

	NVIC->ISER[POWMAN_IRQ_TIMER >> 5] = 1UL << (POWMAN_IRQ_TIMER & 31);
	PLL_SYS_PWR |= PLL_PWR_PD | PLL_PWR_VCOPD | PLL_PWR_POSTDIVPD;
	PLL_USB_PWR |= PLL_PWR_PD | PLL_PWR_VCOPD | PLL_PWR_POSTDIVPD;
	__usb48 = NO;
	SysTick->CTRL = 0;
	{
		uint32_t upper = _mp_powman_read(0x70);
		uint32_t lower = _mp_powman_read(0x74);
		uint64_t began = ((uint64_t)upper << 32) | lower;
		uint64_t ended;
		unsigned long slept;

		_XOSC_DORMANT = _XOSC_DORMANT_STOP;
		_mp_clock_alarm_off();
		upper = _mp_powman_read(0x70);
		lower = _mp_powman_read(0x74);
		ended = ((uint64_t)upper << 32) | lower;
		slept = (unsigned long)((ended - began) * 2ULL / __aon_span);
		__ticks += slept;
		__us_skip += slept * 1000UL;
	}

restore:
	(void)_mp_clock_xosc_on();
	if(!_mp_clock_ref_xosc())
		goto out;
	__src = 0;
	{
		unsigned long measured = _mp_clock_measure();

		__sysclk = measured ? measured : _HZ_XOSC;
		_mp_clock_tick(__sysclk);
		if(__kernel)
			__kernel->arch.sysclk = __sysclk;
	}
out:
#if MP_CPUS > 1
	if(parked)
		mp_cpu_unpark();
#else
	(void)parked;
#endif
}

/* TIMER0 counts microseconds off the crystal, for every core and through
 * sleep. Checked against clk_sys over 1 ms before mp_clock_us trusts it. */
static void _mp_clock_timer(void) {
	uint32_t t0, c0, d;

	if(!(CLOCKS_CLK_REF_SELECTED & (1UL << CLOCKS_REF_SRC_XOSC)))
		return;
	RP2350_REG(RP2350_RESETS_BASE, 0x3000) = RESETS_TIMER0;
	if(!_mp_clock_wait(&RESETS_RESET_DONE, RESETS_TIMER0))
		return;
	TICKS_TIMER0_CTRL = 0;
	TICKS_TIMER0_CYCLES = RP2350_XOSC_HZ / 1000000UL;
	TICKS_TIMER0_CTRL = 1;
	t0 = TIMER0_TIMERAWL;
	c0 = DWT->CYCCNT;
	while(DWT->CYCCNT - c0 < __sysclk / 1000UL)
		;
	d = TIMER0_TIMERAWL - t0;
	__timerOk = d >= 900 && d <= 1100;
}

/* Idle time of the calling core, for its load. */
static void _mp_clock_idle_note(unsigned long from) {
#if MP_CPUS > 1
	mp_cpu_idle_note(mp_clock_us() - from);
#else
	(void)from;
#endif
}

static void _mp_clock_sleep(unsigned long idle) {
	unsigned long counts = __tickCounts;
	unsigned long reload;
	unsigned long done;
	unsigned long whole;
	unsigned long left;
	unsigned long from = mp_clock_us();
	uint32_t ctrl;

	if(idle > _SYSTICK_MAX / counts)
		idle = _SYSTICK_MAX / counts;
	if(idle < 2) {
		__asm volatile ("wfi");
		_mp_clock_idle_note(from);
		return;
	}

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE;
	if(SCB->ICSR & SCB_ICSR_PENDSTSET) {
		SysTick->CTRL |= SysTick_CTRL_ENABLE;
		return;
	}
	reload = SysTick->VAL + counts * (idle - 1);
	SysTick->LOAD = reload;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE;

	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("wfi");
	__asm volatile ("isb");
	_mp_clock_idle_note(from);

	ctrl = SysTick->CTRL;
	SysTick->CTRL = ctrl & ~SysTick_CTRL_ENABLE;
	if(ctrl & SysTick_CTRL_COUNTFLAG) {
		done = reload - SysTick->VAL;
		left = done < counts - 1 ? counts - 1 - done : counts - 1;
		whole = idle - 1;
	}
	else {
		done = idle * counts - SysTick->VAL;
		whole = done / counts;
		left = (whole + 1) * counts - done;
	}
	if(left < 2)
		left = 2;

	SysTick->LOAD = left - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE;
	__ticks += whole;
	SysTick->LOAD = counts - 1;
}

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	unsigned long hz = RP2350_XOSC_HZ;

	(void)kernel;
	__blocker = NULL;

	DWT_LAR = DWT_LAR_UNLOCK;
	DEMCR |= DEMCR_TRCENA;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA;

	/* Crystal, then PLL_USB so clk_usb is 48 MHz, then clk_sys on that
	 * 48 MHz clock. The tick is programmed there, and PLL_SYS takes
	 * clk_sys to 150 MHz once the measurement agrees. */
	__kernel = kernel;
	if(_mp_clock_xosc_on() && _mp_clock_ref_xosc()) {
		unsigned long measured = _mp_clock_measure();

		if(measured)
			hz = measured;
		else
			__blocker = "XOSC count";
		if(!mp_arch_pll_usb_init())
			__blocker = "PLL_USB";
		else if(!_mp_clock_sys_48(&hz))
			__blocker = "clk_sys";
	}
	else
		__blocker = "XOSC";

	kernel->arch.sysclk = hz;
	kernel->arch.hse = NO;
	__sysclk = hz;
	__src = 1;
	_mp_clock_tick(hz);
	_mp_clock_timer();
	if(!_mp_clock_set(2))
		__blocker = __blocker ? __blocker : "PLL_SYS";
	_mp_clock_aon_probe();

	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	(void)kernel;
	SysTick->CTRL = 0;
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
	(void)kernel;
}

static int _mp_clock_src_of(mp_power_level_t level) {
	unsigned long hz;

	if(level >= MP_POWER_HIGH)
		hz = _HZ_PLL;
	else if(level == MP_POWER_MEDIUM)
		hz = _SYS_HZ_USB;
	else
		hz = _HZ_XOSC;
	/* The device controller answers the host from clk_sys. At 12 MHz it
	 * enumerates and then misses the descriptors. */
	if(hz < _SYS_HZ_USB && mp_usb_linked() == YES)
		hz = _SYS_HZ_USB;
	if(hz >= _HZ_PLL)
		return(2);
	if(hz >= _SYS_HZ_USB)
		return(1);
	return(0);
}

static mp_bool_t _mp_clock_quiet(void) {
	__blocker = NULL;
	if(mp_uart_quiet() == NO)
		__blocker = "UART";
	else if(mp_i2c_quiet() == NO)
		__blocker = "I2C";
	else if(mp_spi_quiet() == NO)
		__blocker = "SPI";
	else if((SPI0->CR1 & SPI_CR1_SSE) && (SPI0->SR & SPI_SR_BSY))
		__blocker = "SPI";
	else if((SPI1->CR1 & SPI_CR1_SSE) && (SPI1->SR & SPI_SR_BSY))
		__blocker = "SPI";
	return(__blocker ? NO : YES);
}

mp_ret_t mp_clock_level(mp_kernel_t *kernel, mp_power_level_t level) {
	int src = _mp_clock_src_of(level);

	__kernel = kernel;
	if(src == __src) {
		__blocker = NULL;
		return(TRUE);
	}
	if(_mp_clock_quiet() == NO)
		return(FALSE);
	if(!_mp_clock_set(src))
		return(FALSE);
	return(TRUE);
}

unsigned long mp_clock_level_hz(mp_power_level_t level) {
	return(_mp_clock_nominal(_mp_clock_src_of(level)));
}

void mp_clock_usb_floor(void) {
	if(__sysclk >= _SYS_HZ_USB - _SYS_HZ_USB / 5)
		return;
	(void)_mp_clock_set(1);
}

const char *mp_clock_blocker(void) {
	return(__blocker);
}

void mp_clock_schedule(mp_kernel_t *kernel, unsigned long limit) {
	unsigned long idle;
	int src;

	mp_i2c_watch();

	src = _mp_clock_src_of(kernel->power.level);
	if(src != __src && !(src == 2 && __pll_fails >= 3))
		(void)mp_clock_level(kernel, kernel->power.level);

	__asm volatile ("cpsid i" ::: "memory");
	idle = mp_task_idle(&kernel->tasks);
	if(limit < idle)
		idle = limit;
	if(idle > 0 && mp_power_deep(kernel) == YES && idle >= MP_POWER_STOP_MIN && __aon_span && mp_usb_linked() == NO && __src == 0)
		_mp_clock_dormant(idle);
	else if(idle > 0)
		_mp_clock_sleep(idle);
	__asm volatile ("cpsie i" ::: "memory");
}

const char *mp_clock_deep(void) {
	if(__aon_span && mp_usb_linked() == NO)
		return("Dormant, LPOSC");
	if(mp_usb_linked() == YES)
		return("WFI, USB");
	return("WFI, tickless");
}

void SysTick_Handler(void) {
#if MP_CPUS > 1
	/* every core preempts and shares on its own SysTick, core 0 counts */
	if(SIO_CPUID != 0) {
		unsigned long counts = __tickCounts;

		if(counts >= 2 && SysTick->LOAD != counts - 1)
			SysTick->LOAD = counts - 1;
	}
	else
		__ticks++;
#else
	__ticks++;
#endif
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	mp_task_slice();
#endif
#if MP_TASK_SHARE
	mp_task_share_tick();
#endif
}

unsigned long mp_clock_ticks() {
	return(__ticks);
}

unsigned long mp_clock_tick_hz() {
	return(RP2350_TICK_RATE_HZ);
}

unsigned long mp_clock_us(void) {
	unsigned long ticks;
	unsigned long val;
	unsigned long counts;

	/* one count for every core, through sleep */
	if(__timerOk)
		return(TIMER0_TIMERAWL + __us_skip);
#if MP_CPUS > 1
	/* SysTick is banked: this core's VAL says nothing about core 0's tick */
	if(SIO_CPUID != 0)
		return(__us_skip + __ticks * 1000UL);
#endif
	MP_INTERRUPT_SAFE_BEGIN
	ticks = __ticks;
	val = SysTick->VAL;
	counts = __tickCounts;
	if(SCB->ICSR & SCB_ICSR_PENDSTSET) {
		ticks++;
		val = SysTick->VAL;
	}
	MP_INTERRUPT_SAFE_END

	if(val >= counts)
		val = counts - 1;
	return(__us_skip + ticks * 1000UL + (counts - 1 - val) * 1000UL / counts);
}

unsigned long mp_clock_get_speed() {
	return(__sysclk);
}

unsigned long mp_clock_hclk(void) {
	return(__sysclk);
}

unsigned long mp_clock_peri(void) {
	/* The bit clocks stay on PLL_USB. clk_sys is the core, not the panel. */
	if(__usb48 == YES)
		return(_SYS_HZ_USB);
	return(__sysclk);
}

const char *mp_clock_name(mp_clock_freq_t clock) {
	switch(clock) {
		case MHZ12_t: return("12 MHz");
		case MHZ48_t: return("48 MHz");
		case MHZ150_t: return("150 MHz");
	}
	return("unknown");
}

mp_bool_t mp_clock_usb48(void) {
	return(__usb48);
}

void mp_clock_delay(int delay) {
	unsigned long start;
	int seen = 0;

	if(delay <= 0)
		return;

	if(mp_interrupt_state() == TRUE) {
		start = __ticks;
		while((long)(__ticks - start) < delay)
			__asm volatile ("wfi");
		return;
	}

	(void)SysTick->CTRL;
	while(seen < delay)
		if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG)
			seen++;
}

void mp_clock_nanoDelay(unsigned long delay) {
	uint32_t cycles = (uint32_t)(((uint64_t)delay * (__sysclk / 1000000UL) + 999) / 1000);
	uint32_t start = DWT->CYCCNT;

	while(DWT->CYCCNT - start < cycles);
}

/**@}*/
