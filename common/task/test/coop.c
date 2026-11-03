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

/* Host test of the cooperative scheduler. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define MP_HOST_TASK
#define MP_TASK_MAX 8
#define MP_TASK_SCHEDULER 0
#include "../task.c"
#include "../sched/coop.c"

static int failed;
static mp_kernel_t kernel;
static mp_task_handler_t hdl;
static char seen[16];
static int seenN;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void reset(unsigned long t) {
	memset(&kernel, 0, sizeof(kernel));
	mp_test_clock(t);
	mp_task_init(&kernel, &hdl);
	seenN = 0;
	memset(seen, 0, sizeof(seen));
}

static MP_TASK(on_task) {
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(seenN < (int)sizeof(seen) - 1)
		seen[seenN++] = task->name[0];
	if(task->signal == MP_TASK_SIG_PENDING)
		mp_task_signal(task, MP_TASK_SIG_OK);
}

int main(void) {
	mp_task_t *a;
	mp_task_t *b;
	mp_task_t *slot[MP_TASK_MAX];
	int i;

	reset(0);
	expect(mp_task_tick(&hdl) == MP_TASK_WORKING, "empty tick works");
	expect(mp_task_idle(&hdl) == MP_TASK_FOREVER, "nothing is waiting");
	mp_task_fini(&hdl);
	expect(hdl.signal == MP_TASK_SIG_OK, "fini on an empty pool leaves the signal");

	reset(1000);
	a = mp_task_create(&hdl, "a", on_task, NULL, 10);
	b = mp_task_create(&hdl, "b", on_task, NULL, 30);
	expect(a != NULL && b != NULL, "two tasks");
	expect(a->priority == 0 && b->priority == 0, "priority starts at 0");
	expect(hdl.usedNumber == 2, "both are used");
	expect(mp_task_idle(&hdl) == 10, "idle until a");

	mp_test_clock(1009);
	expect(mp_task_tick(&hdl) == MP_TASK_WORKING, "early tick");
	expect(seenN == 0, "neither is due at 1009");
	expect(mp_task_idle(&hdl) == 1, "one tick left");

	mp_test_clock(1010);
	mp_task_tick(&hdl);
	expect(seenN == 1 && seen[0] == 'a', "a is due, b is not");
	expect(a->runs == 1 && b->runs == 0, "a ran once");

	mp_test_clock(1020);
	mp_task_tick(&hdl);
	expect(seenN == 2 && seen[1] == 'a', "a period is 10");
	expect(b->runs == 0, "b still waiting");

	mp_test_clock(1030);
	mp_task_tick(&hdl);
	expect(seenN == 4 && seen[2] == 'b' && seen[3] == 'a', "last in the list runs first");
	expect(a->runs == 3 && b->runs == 1, "counts");

	reset(50);
	a = mp_task_create(&hdl, "a", on_task, NULL, 0);
	b = mp_task_create(&hdl, "b", on_task, NULL, 0);
	mp_task_priority(a, 3);
	mp_task_priority(b, 0);
	mp_test_clock(50);
	mp_task_tick(&hdl);
	expect(a->priority == 3, "priority is stored");
	expect(seenN == 2 && seen[0] == 'b' && seen[1] == 'a', "coop ignores priority");

	reset(0);
	a = mp_task_create(&hdl, "a", on_task, NULL, 10);
	mp_task_signal(a, MP_TASK_SIG_SLEEP);
	expect(hdl.sleepNumber == 1, "sleep counts");
	expect(mp_task_idle(&hdl) == 10, "sleep keeps the arm from create");
	mp_test_clock(10);
	mp_task_tick(&hdl);
	expect(seenN == 0 && a->runs == 0, "sleep does not run");
	expect(mp_task_idle(&hdl) == MP_TASK_FOREVER, "a tick drops a sleeper's deadline");
	mp_task_wake(a);
	expect(hdl.pendingNumber == 1 && hdl.sleepNumber == 0, "wake is pending");
	expect(mp_task_idle(&hdl) == 0, "pending keeps the kernel awake");
	mp_task_tick(&hdl);
	expect(seenN == 1 && seen[0] == 'a' && a->runs == 1, "pending runs");
	expect(hdl.pendingNumber == 0 && a->signal == MP_TASK_SIG_OK, "the callback cleared pending");

	reset(0);
	b = mp_task_create(&hdl, "b", on_task, NULL, 10);
	mp_task_destroy(b);
	expect(b->signal == MP_TASK_SIG_STOP, "destroy stops");
	mp_test_clock(9);
	mp_task_tick(&hdl);
	expect(hdl.usedNumber == 1, "stop waits for the deadline");
	mp_test_clock(10);
	expect(mp_task_tick(&hdl) == MP_TASK_WORKING, "the handler itself is not stopping");
	expect(hdl.usedNumber == 0, "dead task returned to the pool");

	reset(100);
	a = mp_task_create(&hdl, "a", on_task, NULL, 1000);
	mp_task_signal(a, MP_TASK_SIG_SLEEP);
	mp_test_clock(200);
	mp_task_delay(a, 50);
	expect(a->signal == MP_TASK_SIG_OK && a->check == 200 && a->delay == 50, "delay rearms from now");
	mp_test_clock(249);
	mp_task_tick(&hdl);
	expect(a->runs == 0, "delay has not elapsed");
	mp_test_clock(250);
	mp_task_tick(&hdl);
	expect(a->runs == 1 && seen[0] == 'a', "delay elapsed");

	reset(0);
	mp_task_create(&hdl, "a", on_task, NULL, 0);
	mp_task_create(&hdl, "b", on_task, NULL, 5);
	mp_task_fini(&hdl);
	expect(hdl.usedNumber == 0, "fini drains every task");

	reset(0);
	for(i = 0; i < MP_TASK_MAX; i++)
		slot[i] = mp_task_create(&hdl, "x", on_task, NULL, MP_TASK_FOREVER);
	expect(slot[MP_TASK_MAX - 1] != NULL, "the pool fills");
	expect(mp_task_create(&hdl, "y", on_task, NULL, 1) == NULL, "the pool is full");

	if(failed) {
		fprintf(stderr, "%d failed, seen %.*s\n", failed, seenN, seen);
		return(1);
	}
	printf("ok\n");
	return(0);
}
