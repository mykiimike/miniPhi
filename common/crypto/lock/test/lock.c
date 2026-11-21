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

/* Host test of the crypto lock on one core. MP_CPUS is 1, so this is the
 * preemptive ceiling, not the SMP mutex. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define MP_HOST_TASK
#define MP_TASK_MAX 4
#define MP_TASK_SCHEDULER 2
#define MP_CPUS 1
#define SUPPORT_COMMON_CRYPTO_LOCK

#include <mp.h>
#include "../lock.h"
#include "../../../task/task.c"
#include "../../../task/sched/preempt.c"
#include "../lock.c"

static int failed;
static mp_kernel_t kernel;
static mp_task_handler_t hdl;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "lock: %s\n", msg);
}

int main(void) {
	mp_crypto_lock_t outer, inner;
	mp_task_lock_t low, dip;

	/* The ceiling is a no-op until the scheduler is up. */
	outer = mp_crypto_lock();
	expect(outer == 0, "early key");
	mp_crypto_unlock(outer);
	expect(mp_task_sched_runpri(0) == 0, "early pri");

	memset(&kernel, 0, sizeof(kernel));
	mp_task_init(&kernel, &hdl);
	expect(mp_task_sched_runpri(0) == 0, "boot");

	low = mp_task_lock(1);
	expect(low == 0, "low key");
	expect(mp_task_sched_runpri(0) == 1, "low pri");

	outer = mp_crypto_lock();
	expect(outer == 1, "outer key");
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "ceiling");

	inner = mp_crypto_lock();
	expect(inner == MP_TASK_CEILING_KERNEL, "inner key");
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "nested");

	/* A lower ceiling must not drop the one already held. */
	dip = mp_task_lock(1);
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "no drop");
	mp_task_unlock(dip);
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "dip restored");

	mp_crypto_unlock(inner);
	expect(mp_task_sched_runpri(0) == MP_TASK_CEILING_KERNEL, "outer remains");
	mp_crypto_unlock(outer);
	expect(mp_task_sched_runpri(0) == 1, "back to low");
	mp_task_unlock(low);
	expect(mp_task_sched_runpri(0) == 0, "fully open");

	outer = mp_crypto_lock();
	inner = mp_crypto_lock();
	mp_crypto_unlock(inner);
	mp_crypto_unlock(outer);
	expect(mp_task_sched_runpri(0) == 0, "second nest");

	outer = mp_crypto_lock();
	mp_crypto_unlock(outer);
	outer = mp_crypto_lock();
	mp_crypto_unlock(outer);
	expect(mp_task_sched_runpri(0) == 0, "two visits");
	return(failed ? 1 : 0);
}
