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

/* Host test of the preemptive scheduler. The pend stub runs the
 * dispatcher on this stack. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define MP_HOST_TASK
#define MP_TASK_MAX 8
#define MP_TASK_SCHEDULER 2
#include "../task.c"
#include "../sched/preempt.c"

static int failed;
static mp_kernel_t kernel;
static mp_task_handler_t hdl;
static int depth;
static int deepest;
static mp_task_t *peerTask;
static mp_task_t *overTask;
static mp_task_t *midTask;
static mp_task_t *topTask;
static char trace[8];
static int ntrace;

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
	depth = 0;
	deepest = 0;
	ntrace = 0;
	memset(trace, 0, sizeof(trace));
	peerTask = NULL;
	overTask = NULL;
	midTask = NULL;
	topTask = NULL;
}

static void enter(void) {
	depth++;
	if(depth > deepest)
		deepest = depth;
}

static void leave(void) {
	depth--;
}

static MP_TASK(on_eq) {
	(void)task;
	enter();
	expect(depth == 1, "equal priority does not nest");
	leave();
}

static MP_TASK(on_peer) {
	(void)task;
	enter();
	leave();
}

static MP_TASK(on_over) {
	(void)task;
	enter();
	leave();
}

static MP_TASK(on_runner) {
	enter();
	mp_task_priority(task, 255);
	expect(task->priority == (unsigned char)(MP_TASK_PRIORITIES - 1), "priority clamps");
	mp_task_wake(peerTask);
	expect(peerTask->runs == 0, "equal to the priority at entry did not preempt");
	mp_task_wake(overTask);
	expect(overTask->runs == 1, "above the priority at entry preempted");
	expect(peerTask->runs == 0, "peer still waiting");
	leave();
}

static MP_TASK(on_mid) {
	(void)task;
	if(ntrace < (int)sizeof(trace) - 1)
		trace[ntrace++] = 'M';
}

static MP_TASK(on_top) {
	(void)task;
	if(ntrace < (int)sizeof(trace) - 1)
		trace[ntrace++] = 'T';
}

static MP_TASK(on_guard) {
	mp_task_lock_t key;

	(void)task;
	if(ntrace < (int)sizeof(trace) - 1)
		trace[ntrace++] = 'G';
	key = mp_task_lock(2);
	mp_task_wake(midTask);
	expect(midTask->runs == 0, "under the ceiling stays pending");
	mp_task_wake(topTask);
	expect(topTask->runs == 1, "above the ceiling runs");
	expect(midTask->runs == 0, "ceiling still holds the mid task");
	mp_task_unlock(key);
	expect(midTask->runs == 0, "unlock leaves dispatch to the tick");
	if(ntrace < (int)sizeof(trace) - 1)
		trace[ntrace++] = 'g';
}

static MP_TASK(on_idle) {
	if(task->signal == MP_TASK_SIG_STOP)
		task->signal = MP_TASK_SIG_DEAD;
}

int main(void) {
	mp_task_t *a;
	mp_task_t *b;
	mp_task_t *runner;
	mp_task_t *guard;
	mp_task_t *idle;

	reset(0);
	a = mp_task_create(&hdl, "a", on_eq, NULL, 0);
	b = mp_task_create(&hdl, "b", on_eq, NULL, 0);
	mp_task_priority(a, 1);
	mp_task_priority(b, 1);
	mp_task_tick(&hdl);
	expect(a->runs == 1 && b->runs == 1, "both equal tasks ran");
	expect(deepest == 1, "equal priority did not reenter");

	reset(0);
	runner = mp_task_create(&hdl, "r", on_runner, NULL, 0);
	peerTask = mp_task_create(&hdl, "p", on_peer, NULL, MP_TASK_FOREVER);
	overTask = mp_task_create(&hdl, "o", on_over, NULL, MP_TASK_FOREVER);
	mp_task_priority(runner, 1);
	mp_task_priority(peerTask, 1);
	mp_task_priority(overTask, 2);
	mp_task_signal(peerTask, MP_TASK_SIG_SLEEP);
	mp_task_signal(overTask, MP_TASK_SIG_SLEEP);
	mp_task_tick(&hdl);
	expect(runner->runs == 1 && overTask->runs == 1 && peerTask->runs == 1, "peer runs after the boosted task returns");
	expect(deepest == 2, "only the higher task nested");

	reset(0);
	guard = mp_task_create(&hdl, "g", on_guard, NULL, 0);
	midTask = mp_task_create(&hdl, "m", on_mid, NULL, MP_TASK_FOREVER);
	topTask = mp_task_create(&hdl, "t", on_top, NULL, MP_TASK_FOREVER);
	mp_task_priority(midTask, 1);
	mp_task_priority(topTask, 3);
	mp_task_signal(midTask, MP_TASK_SIG_SLEEP);
	mp_task_signal(topTask, MP_TASK_SIG_SLEEP);
	mp_task_tick(&hdl);
	expect(guard->runs == 1 && midTask->runs == 1 && topTask->runs == 1, "ceiling then the rest of the tick");
	expect(ntrace == 4 && trace[0] == 'G' && trace[1] == 'T' && trace[2] == 'g' && trace[3] == 'M', "ceiling order");

	reset(100);
	idle = mp_task_create(&hdl, "i", on_idle, NULL, 10);
	expect(mp_task_idle(&hdl) == 10, "priority 0 still arms idle");
	mp_test_clock(109);
	mp_task_tick(&hdl);
	expect(idle->runs == 0, "not due yet");
	expect(mp_task_idle(&hdl) == 1, "one tick left");
	mp_test_clock(110);
	mp_task_tick(&hdl);
	expect(idle->runs == 1, "priority 0 ran from the tick");

	if(failed) {
		fprintf(stderr, "%d failed, trace %.*s\n", failed, ntrace, trace);
		return(1);
	}
	printf("ok\n");
	return(0);
}
