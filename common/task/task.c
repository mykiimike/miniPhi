/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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
#include "sched.h"

/**
@defgroup mpCommonTask Task manager

@ingroup mpCommon

@brief Create and control kernel task

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

@{
*/

/* one pass marks a task so a delay of 0 cannot be picked again.
 * A nested pass keeps its own marks and restores the outer ones on
 * the way out. Tasks still inside execute stay marked, so a long
 * wakeup is not re-entered, and a task that already returned can run
 * again once its delay has elapsed. */
#define _MP_TASK_PASS_NEST 4
static unsigned char _mp_task_served[MP_TASK_MAX];
static unsigned char _mp_task_served_save[_MP_TASK_PASS_NEST][MP_TASK_MAX];
static mp_task_t *_mp_task_running[_MP_TASK_PASS_NEST];
static int _mp_task_running_n;
static int _mp_task_depth;

static void _mp_task_soon(mp_task_handler_t *hdl, unsigned long at) {
	if(hdl->wakeSet == NO || (long)(at - hdl->wakeAt) < 0) {
		hdl->wakeAt = at;
		hdl->wakeSet = YES;
	}
}

/* FOREVER added to the last check wraps into the past, so the kernel would never sleep */
static void _mp_task_arm(mp_task_handler_t *hdl, mp_task_t *task) {
	if(task->delay == MP_TASK_FOREVER)
		return;
	_mp_task_soon(hdl, task->check + task->delay);
}

void mp_task_init(mp_kernel_t *kernel, mp_task_handler_t *hdl) {
	mp_task_t *task;
	int a;

	memset(hdl, 0, sizeof(*hdl));
	_mp_task_depth = 0;
	_mp_task_running_n = 0;
	for(a = 0; a < MP_TASK_MAX; a++)
		_mp_task_served[a] = 0;

	hdl->kernel = kernel;

	for(a=0; a<MP_TASK_MAX; a++) {
		task = &hdl->tasks[a];

		/* add task into the free list */
		mp_list_add_last(&hdl->freeList, &task->item, task);
	}

	/* set global start */
	hdl->signal = MP_TASK_SIG_OK;
	mp_task_sched_boot(hdl);
}

void mp_task_fini(mp_task_handler_t *hdl) {
	/* assert */
	if(hdl->usedList.first == NULL)
		return;

	/* set global end */
	hdl->signal = MP_TASK_SIG_STOP;

	/* assert */
	while(mp_task_tick(hdl) != MP_TASK_STOPPED);
}

void mp_task_flush(mp_task_handler_t *hdl) {
	/* assert */
	if(hdl->usedList.first == NULL)
		return;

	hdl->signal = MP_TASK_SIG_STOP;

	/* assert */
	while(mp_task_tick(hdl) != MP_TASK_STOPPED);

	mp_task_init(hdl->kernel, hdl);
}

mp_task_t *mp_task_create(mp_task_handler_t *hdl, char *name, mp_task_wakeup_t wakeup, void *user, unsigned long delay) {
	mp_task_t *task;
	mp_task_lock_t key;

	key = mp_task_lock(MP_TASK_CEILING_KERNEL);

	/* assert */
	if(hdl->freeList.first == NULL) {
		mp_task_unlock(key);
		return(NULL);
	}

	/* allocate task */
	task = hdl->freeList.last->user;

	/* remove from freeList */
	mp_list_remove(&hdl->freeList, &task->item);

	/* prepare task */

	task->handler = hdl;
	task->check = mp_clock_ticks();
	task->delay = delay;
	task->name = name;
	task->wakeup = wakeup;
	task->user = user;
	task->runs = 0;
	task->priority = 0;

	hdl->usedNumber++;

	/* add to usedList */
	mp_list_add_last(&hdl->usedList, &task->item, task);

	task->signal = MP_TASK_SIG_OK;
	mp_task_signal(task, MP_TASK_SIG_OK);

	mp_task_unlock(key);
	return(task);
}

mp_ret_t mp_task_destroy(mp_task_t *task) {
	mp_task_signal(task, MP_TASK_SIG_STOP);
	return(TRUE);
}

void mp_task_rearm(mp_task_handler_t *hdl, mp_task_t *task) {
	MP_INTERRUPT_SAFE_BEGIN
	_mp_task_arm(hdl, task);
	MP_INTERRUPT_SAFE_END
}

void mp_task_rearm_all(mp_task_handler_t *hdl) {
	mp_list_item_t *item;
	mp_task_t *task;

	for(item = hdl->usedList.first; item != NULL; item = item->next) {
		task = item->user;
		if(task->signal == MP_TASK_SIG_OK || task->signal == MP_TASK_SIG_STOP)
			mp_task_rearm(hdl, task);
	}
}

mp_bool_t mp_task_ready(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now) {
	if(hdl->signal == MP_TASK_SIG_STOP)
		return(YES);
	if(task->signal == MP_TASK_SIG_PENDING)
		return(YES);
	if(task->signal == MP_TASK_SIG_SLEEP)
		return(NO);
	if(now-task->check >= task->delay)
		return(YES);
	return(NO);
}

static void _mp_task_served_clear(void) {
	int i;

	for(i = 0; i < MP_TASK_MAX; i++)
		_mp_task_served[i] = 0;
}

static void _mp_task_served_hold(void) {
	int i;
	int n;
	mp_task_t *task;
	mp_task_handler_t *hdl;
	int slot;

	n = _mp_task_running_n;
	if(n > _MP_TASK_PASS_NEST)
		n = _MP_TASK_PASS_NEST;
	for(i = 0; i < n; i++) {
		task = _mp_task_running[i];
		if(!task || !task->handler)
			continue;
		hdl = task->handler;
		slot = (int)(task - hdl->tasks);
		if(slot >= 0 && slot < MP_TASK_MAX)
			_mp_task_served[slot] = 1;
	}
}

void mp_task_pass_enter(void) {
	int i;

	/* Only a preemption inside execute gets a fresh mark set. A pend
	 * with nobody running is still this pass and must keep the marks. */
	if(_mp_task_depth > 0 && _mp_task_depth < _MP_TASK_PASS_NEST && _mp_task_running_n > 0) {
		for(i = 0; i < MP_TASK_MAX; i++)
			_mp_task_served_save[_mp_task_depth][i] = _mp_task_served[i];
		_mp_task_served_clear();
		_mp_task_served_hold();
	}
	else if(_mp_task_depth == 0)
		_mp_task_served_clear();
	_mp_task_depth++;
}

void mp_task_pass_leave(void) {
	int i;

	if(_mp_task_depth > 0)
		_mp_task_depth--;
	if(_mp_task_depth == 0) {
		_mp_task_served_clear();
		return;
	}
	if(_mp_task_depth >= _MP_TASK_PASS_NEST || _mp_task_running_n == 0)
		return;
	/* A task that ran in the nested pass stays marked for the outer one. */
	for(i = 0; i < MP_TASK_MAX; i++)
		_mp_task_served[i] |= _mp_task_served_save[_mp_task_depth][i];
}

mp_task_t *mp_task_pick(mp_task_handler_t *hdl, unsigned long now, int exact, int ceiling) {
	mp_list_item_t *item;
	mp_task_t *best;
	mp_task_t *task;
	int slot;
	unsigned long due;
	unsigned long bestDue;

	best = NULL;
	for(item = hdl->usedList.first; item != NULL; item = item->next) {
		task = item->user;
		slot = (int)(task - hdl->tasks);
		if(slot >= 0 && slot < MP_TASK_MAX && _mp_task_served[slot])
			continue;
		if(exact >= 0 && task->priority != (unsigned char)exact)
			continue;
		if(ceiling >= 0 && task->priority <= (unsigned char)ceiling)
			continue;
		if(mp_task_ready(hdl, task, now) != YES)
			continue;
		if(best == NULL || task->priority > best->priority)
			best = task;
		else if(task->priority == best->priority) {
			due = task->check + task->delay;
			bestDue = best->check + best->delay;
			if((long)(due - bestDue) < 0)
				best = task;
		}
	}
	return(best);
}

void mp_task_execute(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now) {
	int slot;
	mp_task_lock_t key;

	slot = (int)(task - hdl->tasks);
	if(slot >= 0 && slot < MP_TASK_MAX)
		_mp_task_served[slot] = 1;

	if(_mp_task_running_n < _MP_TASK_PASS_NEST)
		_mp_task_running[_mp_task_running_n] = task;
	_mp_task_running_n++;

	if(hdl->signal == MP_TASK_SIG_STOP)
		mp_task_signal(task, MP_TASK_SIG_STOP);

	/* execute wakeup */
	task->runs++;
	task->wakeup(task);

	_mp_task_running_n--;

	/* acknowledge stop dead message */
	if(task->signal == MP_TASK_SIG_DEAD) {
		key = mp_task_lock(MP_TASK_CEILING_KERNEL);
		mp_list_remove(&hdl->usedList, &task->item);
		mp_list_add_last(&hdl->freeList, &task->item, task);
		hdl->usedNumber--;
		mp_task_unlock(key);
	}
	/* recycle check delay */
	else
		task->check = now;
}

mp_task_tick_t mp_task_sched_result(mp_task_handler_t *hdl) {
	if(hdl->signal == MP_TASK_SIG_STOP && hdl->usedNumber == 0)
		return(MP_TASK_STOPPED);
	else if(hdl->signal == MP_TASK_SIG_STOP && hdl->usedNumber != 0)
		return(MP_TASK_DESTROYING);
	return(MP_TASK_WORKING);
}

/**
 * @brief Change the state of a task
 *
 * Interrupt safe.
 *
 * @param[in] task The task
 * @param[in] signal New state
 */
void mp_task_signal(mp_task_t *task, mp_task_signal_t signal) {
	mp_task_handler_t *hdl = task->handler;

	MP_INTERRUPT_SAFE_BEGIN
	if(task->signal != MP_TASK_SIG_SLEEP && signal == MP_TASK_SIG_SLEEP)
		hdl->sleepNumber++;
	else if(task->signal == MP_TASK_SIG_SLEEP && signal != MP_TASK_SIG_SLEEP)
		hdl->sleepNumber--;

	if(task->signal != MP_TASK_SIG_PENDING && signal == MP_TASK_SIG_PENDING)
		hdl->pendingNumber++;
	else if(task->signal == MP_TASK_SIG_PENDING && signal != MP_TASK_SIG_PENDING)
		hdl->pendingNumber--;

	task->signal = signal;
	if(signal == MP_TASK_SIG_OK || signal == MP_TASK_SIG_STOP)
		_mp_task_arm(hdl, task);
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Run a task on the next pass
 *
 * For an interrupt that has work for a task. A stopping task is left alone.
 * Interrupt safe. A preemptive build pends when the task is ready above
 * the running priority. mp_task_signal() alone waits for the next tick.
 *
 * @param[in] task The task
 */
void mp_task_wake(mp_task_t *task) {
	MP_INTERRUPT_SAFE_BEGIN
	if(task->signal == MP_TASK_SIG_OK || task->signal == MP_TASK_SIG_SLEEP)
		mp_task_signal(task, MP_TASK_SIG_PENDING);
	MP_INTERRUPT_SAFE_END
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	if(task->signal == MP_TASK_SIG_PENDING && mp_task_sched_preempts(task))
		mp_task_pend();
#endif
}

/**
 * @brief Run a task delay ticks from now, then every delay ticks
 *
 * A sleeping or pending task goes back to OK. Interrupt safe.
 *
 * @param[in] task The task
 * @param[in] delay Ticks
 */
void mp_task_delay(mp_task_t *task, unsigned long delay) {
	MP_INTERRUPT_SAFE_BEGIN
	task->check = mp_clock_ticks();
	task->delay = delay;
	if(task->signal == MP_TASK_SIG_OK || task->signal == MP_TASK_SIG_SLEEP || task->signal == MP_TASK_SIG_PENDING)
		mp_task_signal(task, MP_TASK_SIG_OK);
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Set the scheduling priority
 *
 * 0 is the default. Clamped to MP_TASK_PRIORITIES-1. The cooperative
 * scheduler ignores it. Priority and preemptive scheduling notice the
 * new value the next time this task returns.
 *
 * @param[in] task The task
 * @param[in] priority Level
 */
void mp_task_priority(mp_task_t *task, unsigned char priority) {
	if(priority >= MP_TASK_PRIORITIES)
		priority = (unsigned char)(MP_TASK_PRIORITIES - 1);
	task->priority = priority;
}

/**
 * @brief Ticks the kernel can sleep before a task is due
 *
 * The arch calls it with interrupts masked, before it sleeps.
 *
 * @param[in] hdl Task handler
 * @return 0 when a task is due or pending, MP_TASK_FOREVER when none waits on time
 */
unsigned long mp_task_idle(mp_task_handler_t *hdl) {
	long left;

	if(hdl->pendingNumber > 0 || hdl->signal == MP_TASK_SIG_STOP)
		return(0);
	if(hdl->wakeSet == NO)
		return(MP_TASK_FOREVER);
	left = (long)(hdl->wakeAt - mp_clock_ticks());
	return(left > 0 ? (unsigned long)left : 0);
}

/* Eight create/run/die passes while the rest of the system is live.
 * Priority 2 and priority 0 must both run. Coop ignores the priority
 * and still has to run both. A priority-0 probe cannot cut in front of
 * the running callback, and a KEM self-test holds that callback for
 * several seconds, so the wait is in ticks, not a short poll count. */
#define _MP_TASK_SELF_ROUNDS 8
#define _MP_TASK_SELF_WAIT   20000

static volatile unsigned char _mp_task_self_lo;
static volatile unsigned char _mp_task_self_hi;
static unsigned char _mp_task_self_round;
static unsigned short _mp_task_self_wait;
static mp_task_t *_mp_task_self_lo_task;
static mp_task_t *_mp_task_self_hi_task;

static MP_TASK(_mp_task_self_lo_run) {
	_mp_task_self_lo = 1;
	task->signal = MP_TASK_SIG_DEAD;
}

static MP_TASK(_mp_task_self_hi_run) {
	_mp_task_self_hi = 1;
	task->signal = MP_TASK_SIG_DEAD;
}

static MP_TASK(_mp_task_self_watch) {
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(_mp_task_self_lo_task == NULL) {
		_mp_task_self_lo = 0;
		_mp_task_self_hi = 0;
		_mp_task_self_wait = 0;
		_mp_task_self_lo_task = mp_task_create(task->handler, "SchedLo", _mp_task_self_lo_run, NULL, 0);
		_mp_task_self_hi_task = mp_task_create(task->handler, "SchedHi", _mp_task_self_hi_run, NULL, 0);
		if(!_mp_task_self_lo_task || !_mp_task_self_hi_task) {
			mp_printk("SELFTEST: sched failed");
			task->signal = MP_TASK_SIG_DEAD;
			return;
		}
		mp_task_priority(_mp_task_self_lo_task, 0);
		mp_task_priority(_mp_task_self_hi_task, 2);
		mp_task_delay(task, 1);
		return;
	}

	if(_mp_task_self_lo && _mp_task_self_hi) {
		_mp_task_self_lo_task = NULL;
		_mp_task_self_hi_task = NULL;
		_mp_task_self_round++;
		if(_mp_task_self_round >= _MP_TASK_SELF_ROUNDS) {
			mp_printk("SELFTEST: sched passed");
			task->signal = MP_TASK_SIG_DEAD;
			return;
		}
		mp_task_delay(task, 1);
		return;
	}

	_mp_task_self_wait++;
	if(_mp_task_self_wait > _MP_TASK_SELF_WAIT) {
		mp_printk("SELFTEST: sched failed");
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	mp_task_delay(task, 1);
}

void mp_task_selftest_start(mp_task_handler_t *hdl) {
	mp_task_t *task;

	if(!hdl)
		return;
	_mp_task_self_round = 0;
	_mp_task_self_wait = 0;
	_mp_task_self_lo = 0;
	_mp_task_self_hi = 0;
	_mp_task_self_lo_task = NULL;
	_mp_task_self_hi_task = NULL;
	task = mp_task_create(hdl, "Sched", _mp_task_self_watch, NULL, 1);
	if(!task) {
		mp_printk("SELFTEST: sched failed");
		return;
	}
	mp_task_priority(task, 1);
}

mp_task_tick_t mp_task_tick(mp_task_handler_t *hdl) {
	/* assert */
	if(hdl->usedNumber == 0) {
		if(hdl->signal == MP_TASK_SIG_STOP)
			return(MP_TASK_STOPPED);
		return(MP_TASK_WORKING);
	}

	return(mp_task_sched_tick(hdl));
}

/**@}*/
