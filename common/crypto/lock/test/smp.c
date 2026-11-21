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

/* Host-only. MP_CPUS is 2, so the crypto lock is the real cpu mutex.
 * Two pthreads use distinct cpu ids. The host interrupt mask is a no-op,
 * so this does not prove same-core exclusion. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#define MP_HOST_TASK
#define MP_TASK_MAX 4
#define MP_TASK_SCHEDULER 2
#define MP_CPUS 2
#define SUPPORT_COMMON_CRYPTO_LOCK

#include <mp.h>

static inline void mp_interrupt_enable(void) { }

#include "../lock.h"
#include "../../../task/task.c"
#include "../../../task/sched/preempt.c"
#include "../lock.c"

static int failed;
static mp_kernel_t kernel;
static mp_task_handler_t hdl;
static __thread int _cpu;
static volatile unsigned long counter;

#define HAMMER 100000

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

int mp_cpu_id(void) {
	return(_cpu);
}

mp_bool_t mp_cpu_start(int cpu, void (*entry)(void)) {
	(void)cpu;
	(void)entry;
	return(NO);
}

void mp_cpu_kick(int cpu) {
	(void)cpu;
}

void mp_cpu_pend(int cpu) {
	(void)cpu;
}

void mp_cpu_idle(void) {
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "smp: %s\n", msg);
}

static void *hammer(void *arg) {
	int i;

	_cpu = (int)(intptr_t)arg;
	for(i = 0; i < HAMMER; i++) {
		mp_crypto_lock_t outer = mp_crypto_lock();
		mp_crypto_lock_t inner = mp_crypto_lock();

		counter++;
		counter++;
		mp_crypto_unlock(inner);
		mp_crypto_unlock(outer);
	}
	return(NULL);
}

int main(void) {
	mp_crypto_lock_t outer, inner;
	pthread_t t0, t1;

	_cpu = 0;
	memset(&kernel, 0, sizeof(kernel));
	mp_task_init(&kernel, &hdl);
	expect(mp_task_sched_runpri(0) == 0, "boot");
	expect(_mp_crypto_mutex.depth == 0, "depth 0");
	expect(_mp_crypto_mutex.owner == -1, "owner free");

	outer = mp_crypto_lock();
	expect(_mp_crypto_mutex.depth == 1, "depth 1");
	expect(_mp_crypto_mutex.owner == 0, "owner 0");
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "ceiling");
	inner = mp_crypto_lock();
	expect(_mp_crypto_mutex.depth == 2, "depth 2");
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "nested pri");
	mp_crypto_unlock(inner);
	expect(_mp_crypto_mutex.depth == 1, "depth back");
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "outer pri");
	mp_crypto_unlock(outer);
	expect(_mp_crypto_mutex.depth == 0, "depth clear");
	expect(_mp_crypto_mutex.owner == -1, "owner clear");
	expect(mp_task_sched_runpri(0) == 0, "pri clear");

	counter = 0;
	if(pthread_create(&t0, NULL, hammer, (void *)(intptr_t)0) != 0)
		expect(0, "thread 0");
	if(pthread_create(&t1, NULL, hammer, (void *)(intptr_t)1) != 0)
		expect(0, "thread 1");
	if(pthread_join(t0, NULL) != 0)
		expect(0, "join 0");
	if(pthread_join(t1, NULL) != 0)
		expect(0, "join 1");
	expect(counter == (unsigned long)HAMMER * 4ul, "counter");
	expect(_mp_crypto_mutex.depth == 0, "depth after");
	expect(_mp_crypto_mutex.owner == -1, "owner after");
	expect(mp_task_sched_runpri(0) == 0, "pri 0 after");
	expect(mp_task_sched_runpri(1) == 0, "pri 1 after");
	return(failed ? 1 : 0);
}
