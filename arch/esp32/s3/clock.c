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
@defgroup mpArchESP32S3Clock Clock

@ingroup mpArchESP32

The ROM leaves the CPU on the 40 MHz crystal and the 480 MHz PLL up,
because the USB Serial/JTAG PHY takes 48 MHz from that PLL. HIGH is
240 MHz, MEDIUM is 160 MHz, LOW and SLEEP are 80 MHz. The PLL stays
selected on every level so the console does not drop.

The tick is SYSTIMER unit 0. Its clock is the crystal divided by 2.5,
16 MHz, and does not follow the CPU mux. One period is 16000 counts.

@{
*/

#define _CONF          (S3_SYSTIMER + 0x00UL)
#define _UNIT0_OP      (S3_SYSTIMER + 0x04UL)
#define _TARGET0_HI    (S3_SYSTIMER + 0x1CUL)
#define _TARGET0_LO    (S3_SYSTIMER + 0x20UL)
#define _TARGET0_CONF  (S3_SYSTIMER + 0x34UL)
#define _VALUE_HI      (S3_SYSTIMER + 0x40UL)
#define _VALUE_LO      (S3_SYSTIMER + 0x44UL)
#define _COMP0_LOAD    (S3_SYSTIMER + 0x50UL)
#define _INT_ENA       (S3_SYSTIMER + 0x64UL)
#define _INT_CLR       (S3_SYSTIMER + 0x6CUL)

#define _CPU_PER       (S3_SYSTEM + 0x10UL)
#define _CLK_EN0       (S3_SYSTEM + 0x18UL)
#define _SYSCLK        (S3_SYSTEM + 0x60UL)

static volatile unsigned long _ticks;
static unsigned long _hz;
static mp_kernel_t *_kernel;

static unsigned long long _now(void) {
	s3_write(_UNIT0_OP, 1UL << 30);
	while((s3_read(_UNIT0_OP) & (1UL << 29)) == 0)
		;
	return(((unsigned long long)s3_read(_VALUE_HI) << 32) | s3_read(_VALUE_LO));
}

static void _tick(void *user) {
	(void)user;
	s3_write(_INT_CLR, 1UL);
	_ticks++;
}

static void _cpu(unsigned long hz) {
	unsigned long period = 0;
	unsigned long conf;

	if(hz >= S3_CPU_HIGH_HZ)
		period = 2;
	else if(hz >= S3_CPU_MEDIUM_HZ)
		period = 1;

	conf = s3_read(_CPU_PER);
	conf = (conf & ~3UL) | period;
	s3_write(_CPU_PER, conf);

	conf = s3_read(_SYSCLK);
	conf = (conf & ~(3UL << 10)) | (1UL << 10);
	s3_write(_SYSCLK, conf);
	_hz = hz;
	if(_kernel)
		_kernel->arch.hz = hz;
}

static void _wait(void) {
	__asm__ volatile ("waiti 0" ::: "memory");
}

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	unsigned long long target;
	unsigned long period = S3_SYSTIMER_HZ / S3_TICK_HZ;

	_kernel = kernel;
	_ticks = 0;
	_hz = S3_XTAL_HZ;

	s3_set(_CLK_EN0, 1UL << 29);
	s3_set(_CONF, (1UL << 31) | (1UL << 30));

	target = _now() + period;
	s3_write(_TARGET0_LO, (unsigned long)target);
	s3_write(_TARGET0_HI, (unsigned long)(target >> 32));
	s3_write(_TARGET0_CONF, period | (1UL << 30));
	s3_write(_COMP0_LOAD, 1UL);
	s3_set(_CONF, 1UL << 24);
	s3_write(_INT_CLR, 1UL);
	s3_write(_INT_ENA, 1UL);

	if(!mp_interrupt_set(S3_SRC_SYSTIMER, _tick, NULL, "SYSTIMER"))
		return(FALSE);

	_cpu(S3_CPU_HIGH_HZ);
	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	(void)kernel;
	s3_write(_INT_ENA, 0);
	s3_clr(_CONF, 1UL << 24);
	mp_interrupt_unset(S3_SRC_SYSTIMER);
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
	(void)kernel;
}

unsigned long mp_clock_ticks(void) {
	return(_ticks);
}

unsigned long mp_clock_tick_hz(void) {
	return(S3_TICK_HZ);
}

unsigned long mp_clock_get_speed(void) {
	return(_hz);
}

unsigned long mp_clock_us(void) {
	return((unsigned long)(_now() / (S3_SYSTIMER_HZ / 1000000UL)));
}

unsigned long mp_clock_level_hz(mp_power_level_t level) {
	if(level == MP_POWER_HIGH)
		return(S3_CPU_HIGH_HZ);
	if(level == MP_POWER_MEDIUM)
		return(S3_CPU_MEDIUM_HZ);
	return(S3_CPU_LOW_HZ);
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
	_wait();
}

const char *mp_clock_deep(void) {
	return("WAITI");
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
		_wait();
}

/**@}*/
