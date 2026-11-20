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

/* Host test of the power governor's calm debounce. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_POWER
#define MP_POWER_WINDOW 1
#define MP_TASK_FOREVER 0xFFFFFFFFUL
#include <mp.h>

static unsigned long _us;
static mp_kernel_t kernel;
static int failed;

static const unsigned long _hz[MP_POWER_LEVELS] = {
	40000000UL, 80000000UL, 160000000UL, 240000000UL
};

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

unsigned long mp_clock_us(void) {
	return(_us);
}

unsigned long mp_clock_level_hz(mp_power_level_t level) {
	if((unsigned int)level >= MP_POWER_LEVELS)
		return(_hz[MP_POWER_HIGH]);
	return(_hz[level]);
}

mp_ret_t mp_clock_level(mp_kernel_t *k, mp_power_level_t level) {
	(void)k;
	(void)level;
	return(TRUE);
}

void mp_clock_schedule(mp_kernel_t *k, unsigned long limit) {
	(void)k;
	(void)limit;
}

const char *mp_clock_blocker(void) {
	return(NULL);
}

#include "../power.c"

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void boot(void) {
	_us = 0;
	memset(&kernel, 0, sizeof(kernel));
	mp_power_init(&kernel);
}

/* span microseconds, of which busy were awake. windowAt is rewritten so the span is exact. */
static void window(unsigned long busy, unsigned long span) {
	if(busy > span)
		busy = span;
	kernel.power.windowAt = _us;
	kernel.power.wokeAt = _us + (span - busy);
	_us += span;
	mp_power_schedule(&kernel);
}

int main(void) {
	int i;

	expect(MP_POWER_DOWN_DELAY == 5000, "default calm is 5 s");

	boot();
	window(0, 1500);
	expect(kernel.power.governed == MP_POWER_HIGH, "one short window stays up");
	expect(kernel.power.calm == 1 && kernel.power.calmUs == 500, "1500 us is 1 ms and 500 us");
	window(0, 1500);
	expect(kernel.power.calm == 3 && kernel.power.calmUs == 0, "the remainder is not dropped");
	expect(kernel.power.governed == MP_POWER_HIGH, "3 ms is not the delay");

	boot();
	for(i = 0; i < 3333; i++)
		window(0, 1500);
	expect(kernel.power.governed == MP_POWER_HIGH, "4999.5 ms is still High");
	expect(kernel.power.calm == 4999 && kernel.power.calmUs == 500, "1500 us windows add up");
	window(0, 1500);
	expect(kernel.power.governed == MP_POWER_SLEEP && kernel.power.calm == 0, "5000 ms steps straight to the target");
	expect(kernel.power.level == MP_POWER_SLEEP, "the step is applied");

	/* back up, in the window that asks for it, not after another calm */
	expect(kernel.power.governed == MP_POWER_SLEEP, "still asleep before the rise");
	window(1000, 1000);
	expect(kernel.power.governed == MP_POWER_SLEEP, "one busy window at Sleep still fits Sleep");
	window(1000, 1000);
	expect(kernel.power.governed == MP_POWER_LOW && kernel.power.calm == 0, "the rise is immediate");

	/* a target that comes back up to the governed level clears the wait, including the leftover us */
	boot();
	window(0, 1500);
	window(0, 1500);
	window(0, 1500);
	expect(kernel.power.calm == 4 && kernel.power.calmUs == 500, "calm is part way");
	window(1000, 1000);
	expect(kernel.power.governed == MP_POWER_HIGH && kernel.power.calm == 0 && kernel.power.calmUs == 0,
		"returning to the governed level clears the wait");
	window(0, 1500);
	expect(kernel.power.calm == 1 && kernel.power.calmUs == 500, "the cleared remainder does not come back");

	/* still below, so the wait keeps what it had */
	boot();
	window(0, 10000);
	expect(kernel.power.calm == 10 && kernel.power.target == MP_POWER_SLEEP, "idle asks for Sleep");
	window(4000, 10000);
	expect(kernel.power.target == MP_POWER_LOW && kernel.power.governed == MP_POWER_HIGH, "Low is still under High");
	expect(kernel.power.calm == 20 && kernel.power.calmUs == 0, "a rise that stays below does not clear the wait");

	/* above the governed level: up at once, from a half-finished calm */
	boot();
	kernel.power.governed = MP_POWER_LOW;
	kernel.power.calm = 2000;
	kernel.power.calmUs = 400;
	window(1000, 1000);
	expect(kernel.power.governed == MP_POWER_HIGH && kernel.power.calm == 0 && kernel.power.calmUs == 0,
		"a target above the governed level moves up at once");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
