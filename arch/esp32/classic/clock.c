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
@defgroup mpArchESP32ClassicClock Clock

@ingroup mpArchESP32

The ROM may already have selected the PLL. This port writes the APB
divider to 1 and SOC_CLK_SEL to the crystal first, then powers the
BBPLL and programs it for 480 MHz through the ROM's internal I2C
writer, with the values ESP-IDF uses for a 40 MHz crystal. The PLL then
stays up, so a level change is only the CPU period, the mux and the
core voltage.

HIGH is 240 MHz from the PLL, APB 80 MHz, at the efuse HP voltage.
MEDIUM, LOW and SLEEP are the 40 MHz crystal, APB 40 MHz, at 1.10 V.
REF_TICK is set to 1 MHz on both trees, so a UART on it keeps its baud.
A UART still on APB refuses the change.

There is no SYSTIMER. TIMG0 timer 0 divides APB down to 1 MHz, and the
alarm is 1000 counts later. Its divider, and TIMG1's on the APP CPU,
are rewritten with each tree. The alarm enable bit clears itself; the
handler writes the next count and sets it again. One period is one
scheduler tick.

@{
*/

#define _T0CONFIG   (CL_TIMG0 + 0x00UL)
#define _T0LO       (CL_TIMG0 + 0x04UL)
#define _T0HI       (CL_TIMG0 + 0x08UL)
#define _T0UPDATE   (CL_TIMG0 + 0x0CUL)
#define _T0ALARMLO  (CL_TIMG0 + 0x10UL)
#define _T0ALARMHI  (CL_TIMG0 + 0x14UL)
#define _T0LOADLO   (CL_TIMG0 + 0x18UL)
#define _T0LOADHI   (CL_TIMG0 + 0x1CUL)
#define _T0LOAD     (CL_TIMG0 + 0x20UL)
#define _INT_ENA    (CL_TIMG0 + 0x98UL)
#define _INT_CLR    (CL_TIMG0 + 0xA4UL)

#define _OPTIONS0   (CL_RTC + 0x00UL)
#define _CLK_CONF   (CL_RTC + 0x70UL)
#define _RTC_REG    (CL_RTC + 0x7CUL)
#define _CPU_PER    (CL_DPORT + 0x3CUL)
#define _XTAL_TICK  (CL_APBCTL + 0x04UL)
#define _PLL_TICK   (CL_APBCTL + 0x08UL)
#define _EFUSE_R5   (CL_EFUSE + 0x14UL)
#define _PERIOD     (CL_TIMER_HZ / CL_TICK_HZ)

/* OPTIONS0: BIAS_I2C 18, BB_I2C 6, BBPLL 10, BBPLL_I2C 8 force power-down */
#define _PLL_PD     ((1UL << 18) | (1UL << 6) | (1UL << 10) | (1UL << 8))
/* RTC_CNTL_REG DIG_DBIAS_WAK is bits 13:11; 4 is 1.10 V, 7 is 1.25 V */
#define _DBIAS_XTAL 4UL
#define _DBIAS_1V25 7UL
/* CPUPERIOD_SEL 2 is 240 MHz on the 480 MHz PLL */
#define _CPU_240    2UL

#define _BBPLL      0x66
#define _BBPLL_HOST 4

typedef void (*_rom_i2c_write_t)(unsigned char, unsigned char, unsigned char, unsigned char);
/* rom_i2c_writeReg in the ESP32 ROM */
#define _rom_i2c_write ((_rom_i2c_write_t)0x400041A4UL)

static volatile unsigned long _ticks;
static unsigned long long _alarm;
static mp_kernel_t *_kernel;
static unsigned long _apb = CL_XTAL_HZ;
static unsigned long _cpu = CL_XTAL_HZ;
static int _pll;
static const char *_blocker;

unsigned long cl_apb_hz(void) {
	return(_apb);
}

static unsigned long long _now(void) {
	cl_write(_T0UPDATE, 1);
	return(((unsigned long long)cl_read(_T0HI) << 32) | cl_read(_T0LO));
}

static void _arm(unsigned long long when) {
	cl_write(_T0ALARMLO, (unsigned long)when);
	cl_write(_T0ALARMHI, (unsigned long)(when >> 32));
	_alarm = when;
	cl_set(_T0CONFIG, 1UL << 10);
}

static void _tick(void *user) {
	unsigned long long now;
	unsigned long long next;

	(void)user;
#if MP_CPUS > 1
	/* Core 1 has TIMG1. Core 0 keeps the tick count on TIMG0. */
	if(cl_cpu_id()) {
		cl_cpu1_tick();
		return;
	}
#endif
	cl_write(_INT_CLR, 1UL);
	_ticks++;
	now = _now();
	next = _alarm + _PERIOD;
	if(next <= now)
		next = now + _PERIOD;
	_arm(next);
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	cl_write(CL_FROM_CPU, 1);
#endif
}

/* PRE_DIV_CNT is bits 9:0 and the divider is that field plus one.
 * SOC_CLK_SEL is bits 28:27: 0 is the crystal. The divider is written
 * on its own, then the mux. */
static void _xtal(void) {
	unsigned long conf;

	conf = cl_read(CL_APBCTL);
	conf &= ~0x3FFUL;
	cl_write(CL_APBCTL, conf);
	conf = cl_read(_CLK_CONF);
	conf &= ~(3UL << 27);
	cl_write(_CLK_CONF, conf);
}

/* TIMG0 counts microseconds whichever tree runs */
static void _spin(unsigned long us) {
	unsigned long long until = _now() + us;

	while(_now() < until)
		;
}

static void _bbpll(unsigned char reg, unsigned char value) {
	_rom_i2c_write(_BBPLL, _BBPLL_HOST, reg, value);
}

static void _dbias(unsigned long level) {
	unsigned long reg = cl_read(_RTC_REG);

	reg &= ~(7UL << 11);
	reg |= level << 11;
	cl_write(_RTC_REG, reg);
}

/* the efuse lowers 1.25 V by its HP_INV steps on chips that need less */
static unsigned long _dbias_hp(void) {
	return(_DBIAS_1V25 - ((cl_read(_EFUSE_R5) >> 22) & 3UL));
}

/* ESP-IDF rtc_clk_bbpll_enable, then rtc_clk_bbpll_configure for 480 MHz on 40 MHz */
static void _pll_up(void) {
	cl_clr(_OPTIONS0, _PLL_PD);
	_bbpll(0, 0x18);
	_bbpll(1, 0x20);
	_bbpll(4, 0x9A);
	_bbpll(10, 0x00);
	_bbpll(12, 0x00);
	_spin(10);
	_dbias(_dbias_hp());
	_spin(3);
	_bbpll(11, 0xC3);
	_bbpll(9, 0x74);
	/* lref 0, div10_8 0, div_ref 0; div7_0 28; bw 3, dcur 6 */
	_bbpll(2, 0x00);
	_bbpll(3, 28);
	_bbpll(5, (3U << 6) | 6U);
	_spin(160);
	_dbias(_DBIAS_XTAL);
	_pll = 1;
}

/* the divider is only written with the timer stopped; the count is kept */
static void _timer_rate(void) {
	unsigned long conf = cl_read(_T0CONFIG);

	cl_write(_T0CONFIG, conf & ~(1UL << 31));
	conf &= ~(0xFFFFUL << 13);
	conf |= (_apb / CL_TIMER_HZ) << 13;
	cl_write(_T0CONFIG, conf);
}

static void _tree(unsigned long hz) {
	unsigned long ps;
	unsigned long conf;

	__asm__ volatile ("rsil %0, 3" : "=r"(ps) :: "memory");
	conf = cl_read(_CLK_CONF) & ~(3UL << 27);
	if(hz > CL_XTAL_HZ) {
		_dbias(_dbias_hp());
		_spin(3);
		cl_write(_CPU_PER, _CPU_240);
		cl_write(_CLK_CONF, conf | (1UL << 27));
		_apb = CL_PLL_APB_HZ;
	}
	else {
		cl_write(_CLK_CONF, conf);
		_apb = CL_XTAL_HZ;
	}
	_timer_rate();
#if MP_CPUS > 1
	cl_cpu1_rate();
#endif
	if(hz <= CL_XTAL_HZ)
		_dbias(_DBIAS_XTAL);
	_cpu = hz;
	if(_kernel)
		_kernel->arch.hz = hz;
	__asm__ volatile ("wsr %0, ps\n\trsync" :: "r"(ps) : "memory");
}

static void _wait(void) {
#if MP_CPUS > 1
	unsigned long from = mp_clock_us();
#endif
	__asm__ volatile ("waiti 0" ::: "memory");
#if MP_CPUS > 1
	mp_cpu_idle_note(mp_clock_us() - from);
#endif
}

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	_kernel = kernel;
	_ticks = 0;

	_xtal();
	_apb = CL_XTAL_HZ;
	_cpu = CL_XTAL_HZ;
	_blocker = NULL;
	if(_kernel)
		_kernel->arch.hz = CL_XTAL_HZ;
	/* REF_TICK is 1 MHz from either tree: the count is the divider minus one */
	cl_write(_XTAL_TICK, CL_XTAL_HZ / CL_REF_TICK_HZ - 1UL);
	cl_write(_PLL_TICK, CL_PLL_APB_HZ / CL_REF_TICK_HZ - 1UL);

	/* TIMERGROUP_CLK_EN is bit 13. The reset bit is the same place. */
	cl_periph(1UL << 13, 1);
	cl_write(_T0CONFIG, 0);
	/* Bit 11 is the level interrupt. The matrix source is TG0_T0_LEVEL.
	 * Bit 10, the alarm enable, is cleared by the hardware when it fires. */
	cl_write(_T0CONFIG, (1UL << 30) | (1UL << 11) | ((_apb / CL_TIMER_HZ) << 13));
	cl_write(_T0LOADLO, 0);
	cl_write(_T0LOADHI, 0);
	cl_write(_T0LOAD, 1);
	cl_write(_INT_CLR, 1UL);
	cl_write(_INT_ENA, 1UL);
	cl_write(_T0CONFIG, (1UL << 31) | (1UL << 30) | (1UL << 11) | ((_apb / CL_TIMER_HZ) << 13));
	_arm(_PERIOD);

	if(!mp_interrupt_set(CL_SRC_TG0_T0, _tick, NULL, "TIMG0"))
		return(FALSE);

	/* mp_power_init() takes the boot tree as HIGH */
	if(!_pll)
		_pll_up();
	if(!cl_uart_apb())
		_tree(CL_PLL_CPU_HZ);
	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	(void)kernel;
	cl_write(_INT_ENA, 0);
	cl_write(_T0CONFIG, 0);
	mp_interrupt_unset(CL_SRC_TG0_T0);
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
	(void)kernel;
}

unsigned long mp_clock_ticks(void) {
	return(_ticks);
}

unsigned long mp_clock_tick_hz(void) {
	return(CL_TICK_HZ);
}

unsigned long mp_clock_get_speed(void) {
	return(_cpu);
}

unsigned long mp_clock_us(void) {
	return((unsigned long)_now());
}

unsigned long mp_clock_level_hz(mp_power_level_t level) {
	if(level == MP_POWER_HIGH)
		return(CL_PLL_CPU_HZ);
	return(CL_XTAL_HZ);
}

mp_ret_t mp_clock_level(mp_kernel_t *kernel, mp_power_level_t level) {
	unsigned long hz;

	(void)kernel;
	if(level > MP_POWER_HIGH)
		return(FALSE);
	hz = mp_clock_level_hz(level);
	if(hz == _cpu)
		return(TRUE);
	if(!_pll) {
		_blocker = "PLL";
		return(FALSE);
	}
	if(cl_uart_apb()) {
		_blocker = "UART";
		return(FALSE);
	}
	_blocker = NULL;
	_tree(hz);
	return(TRUE);
}

void mp_clock_schedule(mp_kernel_t *kernel, unsigned long limit) {
	(void)kernel;
	(void)limit;
	_wait();
}

const char *mp_clock_deep(void) {
	return("WAITI");
}

const char *mp_clock_blocker(void) {
	return(_blocker);
}

void mp_clock_delay(int delay) {
	unsigned long start;

	if(delay <= 0)
		return;
	start = _ticks;
	while((long)(_ticks - start) < delay)
		_wait();
}

/**@}*/
