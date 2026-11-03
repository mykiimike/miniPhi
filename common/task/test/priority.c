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

/* Host test of the priority scheduler. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define MP_HOST_TASK
#define MP_TASK_MAX 8
#define MP_TASK_SCHEDULER 1
#include "../task.c"
#include "../sched/priority.c"

static int failed;
static mp_kernel_t kernel;
static mp_task_handler_t hdl;
static char seen[16];
static int seenN;
static mp_task_t *highTask;
static int highAtWake;

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
	highTask = NULL;
	highAtWake = -1;
}

static void note(mp_task_t *task) {
	if(seenN < (int)sizeof(seen) - 1)
		seen[seenN++] = task->name[0];
}

static MP_TASK(on_task) {
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	note(task);
}

static MP_TASK(on_low) {
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	note(task);
	mp_task_wake(highTask);
	highAtWake = (int)highTask->runs;
}

static MP_TASK(on_high) {
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	note(task);
}

int main(void) {
	mp_task_t *low;
	mp_task_t *high;
	mp_task_t *a;
	mp_task_t *b;

	/* high sits later in usedList and still runs first */
	reset(0);
	low = mp_task_create(&hdl, "L", on_task, NULL, 0);
	high = mp_task_create(&hdl, "H", on_task, NULL, 0);
	expect(low->priority == 0, "default priority");
	mp_task_priority(high, 1);
	mp_task_tick(&hdl);
	expect(seenN == 2 && seen[0] == 'H' && seen[1] == 'L', "later high priority runs first");

	/* the earlier high task also runs first, so this is not the reverse walk */
	reset(0);
	high = mp_task_create(&hdl, "H", on_task, NULL, 0);
	low = mp_task_create(&hdl, "L", on_task, NULL, 0);
	mp_task_priority(high, 1);
	mp_task_tick(&hdl);
	expect(seenN == 2 && seen[0] == 'H' && seen[1] == 'L', "earlier high priority still runs first");

	/* same priority: earliest deadline, even if that task was created later */
	reset(0);
	a = mp_task_create(&hdl, "A", on_task, NULL, 20);
	b = mp_task_create(&hdl, "B", on_task, NULL, 5);
	mp_task_priority(a, 1);
	mp_task_priority(b, 1);
	mp_test_clock(20);
	mp_task_tick(&hdl);
	expect(seenN == 2 && seen[0] == 'B' && seen[1] == 'A', "earlier deadline wins the tie");

	/* a running callback is not interrupted */
	reset(0);
	low = mp_task_create(&hdl, "L", on_low, NULL, 0);
	highTask = mp_task_create(&hdl, "H", on_high, NULL, MP_TASK_FOREVER);
	mp_task_priority(highTask, 1);
	mp_task_signal(highTask, MP_TASK_SIG_SLEEP);
	mp_task_tick(&hdl);
	expect(highAtWake == 0, "priority does not preempt");
	expect(highTask->runs == 1 && low->runs == 1, "the woken task runs after the callback");
	expect(seenN == 2 && seen[0] == 'L' && seen[1] == 'H', "low then high");

	mp_task_priority(low, 255);
	expect(low->priority == (unsigned char)(MP_TASK_PRIORITIES - 1), "priority clamps");

	if(failed) {
		fprintf(stderr, "%d failed, seen %.*s\n", failed, seenN, seen);
		return(1);
	}
	printf("ok\n");
	return(0);
}
