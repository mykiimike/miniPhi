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

/* Four stacks a core. A live callback holds one until it returns. 1024 is
 * enough for a short callback and for the higher task the timer nests on top. */
#define _MP_TASK_SHARE_BYTES 1024
#define _MP_TASK_SHARE_SLOTS 4

/* Every core runs its own scheduler on the shared task list: its pass
 * marks, the callbacks it is inside, its private stacks and the switch
 * state. Only the claim of a task (task->cpu) crosses cores. */
typedef struct {
	unsigned char served[MP_TASK_MAX];
	unsigned char served_save[_MP_TASK_PASS_NEST][MP_TASK_MAX];
	mp_task_t *running[_MP_TASK_PASS_NEST];
	int running_n;
	int depth;
#if MP_TASK_SHARE
	unsigned char share_mem[_MP_TASK_SHARE_SLOTS][_MP_TASK_SHARE_BYTES]
		__attribute__((aligned(16)));
	unsigned char share_used[_MP_TASK_SHARE_SLOTS];
	mp_task_t *share_current;
	mp_task_t *share_launch;
	void *share_home;
	volatile int share_running;
	int share_in_irq;
	int isr;
#endif
} _mp_task_core_t;

static _mp_task_core_t _mp_task_core[MP_CPUS];

#define _MP_TASK_CORE (&_mp_task_core[mp_cpu_id()])
#define _mp_task_served       (_MP_TASK_CORE->served)
#define _mp_task_served_save  (_MP_TASK_CORE->served_save)
#define _mp_task_running      (_MP_TASK_CORE->running)
#define _mp_task_running_n    (_MP_TASK_CORE->running_n)
#define _mp_task_depth        (_MP_TASK_CORE->depth)
#if MP_TASK_SHARE
	#define _mp_task_share_mem     (_MP_TASK_CORE->share_mem)
	#define _mp_task_share_used    (_MP_TASK_CORE->share_used)
	#define _mp_task_share_current (_MP_TASK_CORE->share_current)
	#define _mp_task_share_launch  (_MP_TASK_CORE->share_launch)
	#define _mp_task_share_home    (_MP_TASK_CORE->share_home)
	#define _mp_task_share_running (_MP_TASK_CORE->share_running)
	#define _mp_task_share_in_irq  (_MP_TASK_CORE->share_in_irq)
	#define _mp_task_isr           (_MP_TASK_CORE->isr)
#endif

#if MP_CPUS > 1

/* The pass marks, the running list and the private stacks belong to
 * core 0. Another core only claims a task, runs it and gives it back,
 * all under this lock. */
static volatile int _mp_task_cpu_flag;
static volatile int _mp_task_cpu_owner = -1;
static unsigned int _mp_task_cpu_depth;
static mp_bool_t _mp_task_cpu_irq;

/* The ceiling lock of a task, across cores. */
static mp_cpu_mutex_t _mp_task_mutex = MP_CPU_MUTEX_INIT;

/* Cores other than 0 inside a pass. Core 0 does not sleep tickless while
 * one runs, so the tick they read keeps moving. _deep is set while core 0
 * sleeps for more than a tick. */
static volatile int _mp_task_cpu_busy;
static volatile int _mp_task_cpu_gate;
static volatile int _mp_task_cpu_deep;
static volatile int _mp_task_cpu_up = 1;
static mp_task_handler_t *_mp_task_cpu_hdl;

#define _MP_TASK_SMP_BEGIN mp_task_cpu_lock();
#define _MP_TASK_SMP_END   mp_task_cpu_unlock();
#define _MP_TASK_GUARD_BEGIN mp_task_cpu_lock();
#define _MP_TASK_GUARD_END   mp_task_cpu_unlock();

void mp_task_cpu_lock(void) {
	mp_bool_t irq = mp_interrupt_state();
	int me;

	mp_interrupt_disable();
	me = mp_cpu_id();
	if(_mp_task_cpu_owner == me) {
		_mp_task_cpu_depth++;
		return;
	}
	while(__atomic_exchange_n(&_mp_task_cpu_flag, 1, __ATOMIC_ACQUIRE))
		;
	_mp_task_cpu_owner = me;
	_mp_task_cpu_depth = 1;
	_mp_task_cpu_irq = irq;
}

void mp_task_cpu_unlock(void) {
	mp_bool_t irq;

	if(--_mp_task_cpu_depth)
		return;
	irq = _mp_task_cpu_irq;
	_mp_task_cpu_owner = -1;
	__atomic_store_n(&_mp_task_cpu_flag, 0, __ATOMIC_RELEASE);
	mp_interrupt_restore(irq);
}

/* Only the bookkeeping masks the local interrupts, so an interrupt on
 * this core never finds the mutex half taken. Interrupt masking here is
 * local on purpose: MP_INTERRUPT_SAFE is the kernel lock on SMP. */
int mp_cpu_mutex_try(mp_cpu_mutex_t *mutex) {
	mp_bool_t irq = mp_interrupt_state();
	int me;

	mp_interrupt_disable();
	me = mp_cpu_id();
	if(mutex->owner == me)
		mutex->depth++;
	else if(!__atomic_exchange_n(&mutex->flag, 1, __ATOMIC_ACQUIRE)) {
		mutex->owner = me;
		mutex->depth = 1;
	}
	else
		me = -1;
	mp_interrupt_restore(irq);
	return(me >= 0);
}

void mp_cpu_mutex_take(mp_cpu_mutex_t *mutex) {
	while(!mp_cpu_mutex_try(mutex))
		;
}

int mp_task_cpu_locked(void) {
	return(_mp_task_cpu_owner == mp_cpu_id());
}

void mp_cpu_mutex_give(mp_cpu_mutex_t *mutex) {
	mp_bool_t irq = mp_interrupt_state();

	mp_interrupt_disable();
	if(mutex->depth && !--mutex->depth) {
		mutex->owner = -1;
		__atomic_store_n(&mutex->flag, 0, __ATOMIC_RELEASE);
	}
	mp_interrupt_restore(irq);
}

void mp_task_cpu_mutex_take(void) {
	mp_cpu_mutex_take(&_mp_task_mutex);
}

void mp_task_cpu_mutex_give(void) {
	mp_cpu_mutex_give(&_mp_task_mutex);
}

int mp_task_cpus(void) {
	return(_mp_task_cpu_up);
}

/* Free for this core: allowed here, and nobody else inside it. */
static int _mp_task_cpu_free(mp_task_t *task, int me) {
	if(!(task->affinity & (1U << me)))
		return(0);
	return(task->cpu < 0 || task->cpu == me);
}

static int _mp_task_cpu_claim(mp_task_t *task) {
	int me;
	int ok;

	mp_task_cpu_lock();
	me = mp_cpu_id();
	ok = _mp_task_cpu_free(task, me);
	if(ok)
		task->cpu = (signed char)me;
	mp_task_cpu_unlock();
	return(ok);
}

/* A core sleeping on a stale deadline is woken for a task it may take:
 * the others in their WFI loop, core 0 when its tickless sleep is long. */
static volatile unsigned char _mp_task_cpu_sleeping[MP_CPUS];

static void _mp_task_cpu_notify(mp_task_t *task) {
	int me;
	int cpu;
	int asleep;

	if(_mp_task_cpu_up < 2)
		return;
	me = mp_cpu_id();
	for(cpu = 0; cpu < MP_CPUS; cpu++) {
		asleep = cpu ? _mp_task_cpu_sleeping[cpu] : _mp_task_cpu_deep;
		if(cpu != me && asleep && (task->affinity & (1U << cpu)))
			mp_cpu_kick(cpu);
	}
}

#else

#define _MP_TASK_SMP_BEGIN
#define _MP_TASK_SMP_END
#define _MP_TASK_GUARD_BEGIN MP_INTERRUPT_SAFE_BEGIN
#define _MP_TASK_GUARD_END   MP_INTERRUPT_SAFE_END

/* One core has no flag to spin on: the mask is the lock. The first entry
 * keeps the interrupt state the last unlock restores. */
static unsigned int _mp_task_cpu_depth;
static mp_bool_t _mp_task_cpu_irq;

void mp_task_cpu_lock(void) {
	mp_bool_t irq = mp_interrupt_state();

	mp_interrupt_disable();
	if(_mp_task_cpu_depth++)
		return;
	_mp_task_cpu_irq = irq;
}

void mp_task_cpu_unlock(void) {
	mp_bool_t irq;

	if(--_mp_task_cpu_depth)
		return;
	irq = _mp_task_cpu_irq;
	mp_interrupt_restore(irq);
}

int mp_task_cpu_locked(void) {
	return(_mp_task_cpu_depth != 0);
}

#endif

/* A returned callback. A dead task goes back to the free list, a live one
 * counts its delay from now. Either way no core holds it any more. */
static void _mp_task_retire(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now) {
	mp_task_cpu_lock();
	if(task->signal == MP_TASK_SIG_DEAD) {
		mp_list_remove(&hdl->usedList, &task->item);
		mp_list_add_last(&hdl->freeList, &task->item, task);
		hdl->usedNumber--;
	}
	else
		task->check = now;
	task->cpu = -1;
	mp_task_cpu_unlock();
}

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

	/* a flush reuses the handler while the other cores scan it */
	_MP_TASK_SMP_BEGIN
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
	_MP_TASK_SMP_END
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
	_MP_TASK_SMP_BEGIN

	/* assert */
	if(hdl->freeList.first == NULL) {
		_MP_TASK_SMP_END
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
	task->share_sp = NULL;
	task->share_stack = NULL;
	task->share_now = 0;
	task->share_live = 0;
	task->share_done = 0;
	task->share_ticks = 0;
	task->cpu = -1;
	task->affinity = MP_TASK_CPU_ANY;

	hdl->usedNumber++;

	/* add to usedList */
	mp_list_add_last(&hdl->usedList, &task->item, task);

	task->signal = MP_TASK_SIG_OK;
	mp_task_signal(task, MP_TASK_SIG_OK);

	_MP_TASK_SMP_END
	mp_task_unlock(key);
	return(task);
}

mp_ret_t mp_task_destroy(mp_task_t *task) {
	mp_task_signal(task, MP_TASK_SIG_STOP);
	return(TRUE);
}

void mp_task_rearm(mp_task_handler_t *hdl, mp_task_t *task) {
	_MP_TASK_GUARD_BEGIN
	_mp_task_arm(hdl, task);
	_MP_TASK_GUARD_END
}

void mp_task_rearm_all(mp_task_handler_t *hdl) {
	mp_list_item_t *item;
	mp_task_t *task;

	_MP_TASK_SMP_BEGIN
	for(item = hdl->usedList.first; item != NULL; item = item->next) {
		task = item->user;
		if(task->signal == MP_TASK_SIG_OK || task->signal == MP_TASK_SIG_STOP)
			mp_task_rearm(hdl, task);
	}
	_MP_TASK_SMP_END
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

int mp_task_running(void) {
	return(_mp_task_running_n);
}

/* A long callback keeps the outer pass open. A task that already returned
 * stays marked, so the timer would not run it again until that callback
 * ends. Clear those marks above the running priority. Tasks still inside
 * execute stay marked. */
void mp_task_served_open(mp_task_handler_t *hdl, int ceiling) {
	mp_list_item_t *item;
	unsigned char keep[MP_TASK_MAX];
	int i;
	int n;
	int slot;

	if(!hdl)
		return;
	memset(keep, 0, sizeof(keep));
	n = _mp_task_running_n;
	if(n > _MP_TASK_PASS_NEST)
		n = _MP_TASK_PASS_NEST;
	for(i = 0; i < n; i++) {
		mp_task_t *task = _mp_task_running[i];

		if(!task || task->handler != hdl)
			continue;
		slot = (int)(task - hdl->tasks);
		if(slot >= 0 && slot < MP_TASK_MAX)
			keep[slot] = 1;
	}
	_MP_TASK_SMP_BEGIN
	for(item = hdl->usedList.first; item != NULL; item = item->next) {
		mp_task_t *task = item->user;

		slot = (int)(task - hdl->tasks);
		if(slot < 0 || slot >= MP_TASK_MAX || keep[slot])
			continue;
		if(ceiling >= 0 && task->priority <= (unsigned char)ceiling)
			continue;
		_mp_task_served[slot] = 0;
	}
	_MP_TASK_SMP_END
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

	unsigned char *served = _mp_task_served;
#if MP_CPUS > 1
	int me = mp_cpu_id();
#endif

	best = NULL;
	_MP_TASK_SMP_BEGIN
	for(item = hdl->usedList.first; item != NULL; item = item->next) {
		task = item->user;
		slot = (int)(task - hdl->tasks);
		if(slot >= 0 && slot < MP_TASK_MAX && served[slot])
			continue;
#if MP_CPUS > 1
		if(!_mp_task_cpu_free(task, me))
			continue;
#endif
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
	_MP_TASK_SMP_END
	return(best);
}

#if MP_TASK_SHARE

/* Priority 0 never yields: a long callback keeps the processor until it
 * returns. Every level above yields every tick to a peer of that level. */
static unsigned _mp_task_share_quantum(unsigned char priority) {
	if(priority == 0)
		return(0);
	return(1);
}

#ifndef MP_ARCH_ISR_HW
#define MP_ARCH_ISR_HW 0
#endif

static int _mp_task_from_isr(void) {
	#if MP_ARCH_ISR_HW
		return(mp_interrupt_active());
	#else
		return(_mp_task_isr != 0 || _mp_task_share_in_irq != 0);
	#endif
}

void mp_task_isr_nest(int enter) {
	if(enter)
		_mp_task_isr++;
	else if(_mp_task_isr > 0)
		_mp_task_isr--;
}

static void *_mp_task_share_take(mp_task_t *task) {
	int i;

	if(task->share_stack)
		return((unsigned char *)task->share_stack + _MP_TASK_SHARE_BYTES);
	for(i = 0; i < _MP_TASK_SHARE_SLOTS; i++) {
		if(_mp_task_share_used[i])
			continue;
		_mp_task_share_used[i] = 1;
		task->share_stack = _mp_task_share_mem[i];
		return(_mp_task_share_mem[i] + _MP_TASK_SHARE_BYTES);
	}
	return(NULL);
}

static void _mp_task_share_release(mp_task_t *task) {
	int i;

	if(!task->share_stack)
		return;
	for(i = 0; i < _MP_TASK_SHARE_SLOTS; i++) {
		if(_mp_task_share_mem[i] == task->share_stack)
			_mp_task_share_used[i] = 0;
	}
	task->share_stack = NULL;
	task->share_sp = NULL;
}

static void _mp_task_share_finish(mp_task_t *task) {
	task->share_live = 0;
	task->share_done = 0;
	task->share_sp = NULL;
	_mp_task_retire(task->handler, task, task->share_now);
}

static void _mp_task_share_entry(void *arg) {
	mp_task_t *task = arg;

	task->wakeup(task);
	task->share_done = 1;
	while(task->share_live)
		mp_task_pend();
}

static int _mp_task_share_arm(mp_task_t *task, unsigned long now) {
	void *top;
	int slot;
#if MP_CPUS > 1
	/* execute() already holds its own task. A peer is claimed here and
	 * handed back when it cannot start. */
	int held = task->cpu == mp_cpu_id();

	if(!_mp_task_cpu_claim(task))
		return(0);
#endif
	top = _mp_task_share_take(task);
	if(top) {
		task->share_sp = mp_task_arch_forge(top, _mp_task_share_entry, task);
		if(!task->share_sp)
			_mp_task_share_release(task);
	}
	if(!top || !task->share_sp) {
#if MP_CPUS > 1
		if(!held)
			task->cpu = -1;
#endif
		return(0);
	}
	task->share_now = now;
	task->share_done = 0;
	task->share_ticks = 0;
	task->share_live = 1;
	task->runs++;
	slot = (int)(task - task->handler->tasks);
	if(slot >= 0 && slot < MP_TASK_MAX)
		_mp_task_served[slot] = 1;
	if(task->handler->signal == MP_TASK_SIG_STOP)
		mp_task_signal(task, MP_TASK_SIG_STOP);
	return(1);
}

static int _mp_task_share_candidate(mp_task_t *task, mp_task_t *skip, unsigned char priority, unsigned long now) {
	int slot;

	if(!task || task == skip || task->priority != priority)
		return(0);
	if(task->signal == MP_TASK_SIG_DEAD)
		return(0);
	/* The task execute() is waiting on stays in the running list while a
	 * peer has the processor. It is still the one to resume. */
#if MP_CPUS > 1
	/* a live stack on another core is that core's to switch */
	if(task->share_live && task->share_sp && !task->share_done)
		return(task->cpu == mp_cpu_id());
	if(!_mp_task_cpu_free(task, mp_cpu_id()))
		return(0);
#else
	if(task->share_live && task->share_sp && !task->share_done)
		return(1);
#endif
	slot = (int)(task - task->handler->tasks);
	if(slot < 0 || slot >= MP_TASK_MAX)
		return(0);
	if(_mp_task_served[slot])
		return(0);
	return(mp_task_ready(task->handler, task, now) == YES);
}

/* Next ready peer after skip, then wrap. The first list entry would
 * otherwise keep the processor and a later peer would never start. */
static mp_task_t *_mp_task_share_peer(mp_task_t *skip, unsigned char priority, unsigned long now) {
	mp_list_item_t *origin;
	mp_list_item_t *item;
	mp_task_handler_t *hdl;
	int wrapped;

	if(!skip)
		return(NULL);
	hdl = skip->handler;
	_MP_TASK_SMP_BEGIN
	origin = skip->item.next;
	if(origin == NULL)
		origin = hdl->usedList.first;
	item = origin;
	wrapped = 0;
	while(item != NULL) {
		mp_task_t *task = item->user;

		if(task != skip && _mp_task_share_candidate(task, skip, priority, now)) {
			_MP_TASK_SMP_END
			return(task);
		}
		item = item->next;
		if(item == NULL) {
			if(wrapped)
				break;
			wrapped = 1;
			item = hdl->usedList.first;
		}
		if(item == origin)
			break;
	}
	_MP_TASK_SMP_END
	return(NULL);
}

static void *_mp_task_share_resume(mp_task_t *task, unsigned long now) {
	if(task->share_live && task->share_sp && !task->share_done) {
		_mp_task_share_current = task;
		return(task->share_sp);
	}
	if(!_mp_task_share_arm(task, now))
		return(NULL);
	_mp_task_share_current = task;
	return(task->share_sp);
}

/* The interrupted frame belongs to the task that execute() is waiting on,
 * or to a peer the previous switch left running. A finished callback is
 * not resumed. Its slot stays taken until the next task has a different
 * one, because this IRQ is still running on that stack. */
static void *_mp_task_share_choose(void *sp) {
	mp_task_t *cur;
	mp_task_t *next;
	void *resumed;
	unsigned long now;
	unsigned char priority;

	if(_mp_task_share_launch && !_mp_task_share_running) {
		cur = _mp_task_share_launch;
		_mp_task_share_launch = NULL;
		_mp_task_share_home = sp;
		_mp_task_share_running = 1;
		_mp_task_share_current = cur;
		return(cur->share_sp);
	}

	cur = _mp_task_share_current;
	if(!_mp_task_share_running || !cur)
		return(sp);

	now = mp_clock_ticks();
	priority = cur->priority;
	if(cur->share_done) {
		_mp_task_share_finish(cur);
		next = _mp_task_share_peer(cur, priority, now);
		resumed = next ? _mp_task_share_resume(next, now) : NULL;
		if(resumed && next->share_stack != cur->share_stack) {
			_mp_task_share_release(cur);
			return(resumed);
		}
		if(resumed)
			return(resumed);
		_mp_task_share_release(cur);
		_mp_task_share_running = 0;
		_mp_task_share_current = NULL;
		return(_mp_task_share_home);
	}

	cur->share_sp = sp;
	if(mp_task_share_blocked(cur))
		return(sp);
	cur->share_ticks = 0;
	next = _mp_task_share_peer(cur, priority, now);
	if(!next)
		return(sp);
	resumed = _mp_task_share_resume(next, now);
	if(!resumed)
		return(cur->share_sp);
	return(resumed);
}

static int _mp_task_share_enter(mp_task_t *task, unsigned long now) {
	unsigned long t0;

	if(_mp_task_from_isr() || _mp_task_share_running || _mp_task_share_launch)
		return(0);
	if(!_mp_task_share_arm(task, now))
		return(0);
	_mp_task_share_launch = task;
	_mp_task_share_current = task;
	mp_task_pend();
	t0 = mp_clock_ticks();
	/* The switch suspends this loop. The body runs only when the sched
	 * line never took the stack. Drop the private stack and let execute
	 * call the callback on this one, so a missing line does not swallow it. */
	while(task->share_live) {
		if((mp_clock_ticks() - t0) <= 1000UL)
			continue;
		MP_INTERRUPT_SAFE_BEGIN
		task->share_live = 0;
		if(_mp_task_share_launch == task)
			_mp_task_share_launch = NULL;
		if(_mp_task_share_current == task && !_mp_task_share_running)
			_mp_task_share_current = NULL;
		if(!_mp_task_share_running)
			_mp_task_share_release(task);
		if(task->runs > 0)
			task->runs--;
		MP_INTERRUPT_SAFE_END
		return(0);
	}
	return(1);
}

void *mp_task_share_irq(void *sp) {
	_mp_task_share_in_irq++;
	mp_task_share_ack();
	mp_task_share_open();
	mp_task_sched_dispatch();
	mp_task_share_close();
	sp = _mp_task_share_choose(sp);
	_mp_task_share_in_irq--;
	return(sp);
}

void mp_task_share_tick(void) {
	mp_task_t *cur;

	if(_mp_task_share_in_irq)
		return;
	if(_mp_task_share_launch && !_mp_task_share_running) {
		mp_task_pend();
		return;
	}
	cur = _mp_task_share_current;
	if(!_mp_task_share_running || !cur || cur->share_done)
		return;
	if(mp_task_share_blocked(cur))
		return;
	if(!_mp_task_share_quantum(cur->priority))
		return;
	cur->share_ticks++;
	if(cur->share_ticks < _mp_task_share_quantum(cur->priority))
		return;
	cur->share_ticks = 0;
	if(!_mp_task_share_peer(cur, cur->priority, mp_clock_ticks()))
		return;
	mp_task_pend();
}

#endif

static void _mp_task_execute_held(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now);
#if MP_CPUS > 1
static int _mp_task_cpu_busy_enter(mp_task_t *task);
static void _mp_task_cpu_busy_leave(void);
#endif

void mp_task_execute(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now) {
#if MP_CPUS > 1
	if(!_mp_task_cpu_claim(task))
		return;
	if(_mp_task_running_n == 0 && mp_cpu_id() != 0) {
		if(!_mp_task_cpu_busy_enter(task)) {
			int slot = (int)(task - hdl->tasks);

			/* held out: left for after the hold, not again this pass */
			if(slot >= 0 && slot < MP_TASK_MAX)
				_mp_task_served[slot] = 1;
			return;
		}
		_mp_task_execute_held(hdl, task, now);
		_mp_task_cpu_busy_leave();
		return;
	}
#endif
	_mp_task_execute_held(hdl, task, now);
}

/* The task is claimed by this core. */
static void _mp_task_execute_held(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now) {
	int slot;

	slot = (int)(task - hdl->tasks);
	if(slot >= 0 && slot < MP_TASK_MAX)
		_mp_task_served[slot] = 1;

	if(_mp_task_running_n < _MP_TASK_PASS_NEST)
		_mp_task_running[_mp_task_running_n] = task;
	_mp_task_running_n++;

	if(hdl->signal == MP_TASK_SIG_STOP)
		mp_task_signal(task, MP_TASK_SIG_STOP);

#if MP_TASK_SHARE
	/* Outermost call, from thread mode. A nested call is the timer running
	 * a higher task on the stack it interrupted, and that one must return. */
	if(_mp_task_running_n == 1 && _mp_task_share_quantum(task->priority) &&
			_mp_task_share_enter(task, now)) {
		_mp_task_running_n--;
		return;
	}
#endif

	/* execute wakeup */
	task->runs++;
	task->wakeup(task);

	_mp_task_running_n--;
	_mp_task_retire(hdl, task, now);
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

	_MP_TASK_GUARD_BEGIN
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
	_MP_TASK_GUARD_END
#if MP_CPUS > 1
	_mp_task_cpu_notify(task);
#endif
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
	_MP_TASK_GUARD_BEGIN
	if(task->signal == MP_TASK_SIG_OK || task->signal == MP_TASK_SIG_SLEEP)
		mp_task_signal(task, MP_TASK_SIG_PENDING);
	_MP_TASK_GUARD_END
#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	if(task->signal == MP_TASK_SIG_PENDING && mp_task_sched_preempts(task))
		mp_task_pend();
#endif
#if MP_CPUS > 1
	/* a busy core running below it preempts as well, the first one in wins */
	if(task->signal == MP_TASK_SIG_PENDING && _mp_task_cpu_up > 1) {
		int me = mp_cpu_id();
		int cpu;

		for(cpu = 0; cpu < MP_CPUS; cpu++) {
			if(cpu != me && (task->affinity & (1U << cpu)) &&
					task->priority > mp_task_sched_runpri(cpu))
				mp_cpu_pend(cpu);
		}
	}
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
	_MP_TASK_GUARD_BEGIN
	task->check = mp_clock_ticks();
	task->delay = delay;
	if(task->signal == MP_TASK_SIG_OK || task->signal == MP_TASK_SIG_SLEEP || task->signal == MP_TASK_SIG_PENDING)
		mp_task_signal(task, MP_TASK_SIG_OK);
	_MP_TASK_GUARD_END
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
 * @brief Choose the cores that may run a task
 *
 * Bit n is core n. A new task is MP_TASK_CPU_ANY: the first free core
 * takes it. Pin one only when its callback needs a core, such as the
 * private stacks of core 0. A single-core build ignores it. A callback
 * already running finishes where it is.
 *
 * @param[in] task The task
 * @param[in] mask Cores, 0 falls back to core 0
 */
void mp_task_affinity(mp_task_t *task, unsigned char mask) {
	mask &= MP_TASK_CPU_ANY;
	if(!mask)
		mask = 1;
	_MP_TASK_GUARD_BEGIN
	task->affinity = mask;
	_MP_TASK_GUARD_END
#if MP_CPUS > 1
	_mp_task_cpu_notify(task);
#endif
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
#if MP_CPUS > 1
	/* The tick is core 0's. Tickless, it stops moving under the pass
	 * another core is running, so sleep one tick at a time. A core that
	 * starts a pass while this sleep is long wakes it (_mp_task_cpu_run). */
	{
		unsigned long wait = MP_TASK_FOREVER;

		mp_task_cpu_lock();
		if(_mp_task_cpu_busy)
			wait = 1;
		else if(hdl->wakeSet == YES) {
			left = (long)(hdl->wakeAt - mp_clock_ticks());
			wait = left > 0 ? (unsigned long)left : 0;
		}
		_mp_task_cpu_deep = wait > 1;
		mp_task_cpu_unlock();
		return(wait);
	}
#else
	if(hdl->wakeSet == NO)
		return(MP_TASK_FOREVER);
	left = (long)(hdl->wakeAt - mp_clock_ticks());
	return(left > 0 ? (unsigned long)left : 0);
#endif
}

#if MP_CPUS > 1

/* A ready task this core may take, not held by another core. */
static int _mp_task_cpu_due(mp_task_handler_t *hdl, int me) {
	mp_list_item_t *item;
	mp_task_t *task;
	unsigned long now;
	int due = 0;

	mp_task_cpu_lock();
	now = mp_clock_ticks();
	for(item = _mp_task_cpu_gate ? NULL : hdl->usedList.first; item != NULL && !due; item = item->next) {
		task = item->user;
		if(!(task->affinity & (1U << me)) || task->cpu >= 0)
			continue;
		if(task->signal == MP_TASK_SIG_DEAD)
			continue;
		due = mp_task_ready(hdl, task, now) == YES;
	}
	mp_task_cpu_unlock();
	return(due);
}

/* The same scheduler as core 0, on the same list: a pass, nested
 * preemption from this core's SysTick, private stacks switched by this
 * core's PendSV. Between passes it sleeps in WFI until a kick or its tick. */
void mp_task_cpu_run(mp_task_handler_t *hdl) {
	int me = mp_cpu_id();

	for(;;) {
		mp_interrupt_disable();
		_mp_task_cpu_sleeping[me] = 1;
		if(!_mp_task_cpu_due(hdl, me)) {
			mp_cpu_idle();
			_mp_task_cpu_sleeping[me] = 0;
			mp_interrupt_enable();
			continue;
		}
		_mp_task_cpu_sleeping[me] = 0;
		mp_interrupt_enable();
		mp_task_tick(hdl);
	}
}

/* Around the outermost callback of a core above 0, from its pass or from
 * its PendSV. Core 0 in a long tickless sleep is woken so the tick moves.
 * Under a hold the claim is given back and the callback does not run. */
static int _mp_task_cpu_busy_enter(mp_task_t *task) {
	int deep;

	mp_task_cpu_lock();
	if(_mp_task_cpu_gate) {
		task->cpu = -1;
		mp_task_cpu_unlock();
		return(0);
	}
	_mp_task_cpu_busy++;
	deep = _mp_task_cpu_deep;
	mp_task_cpu_unlock();
	if(deep)
		mp_cpu_kick(0);
	return(1);
}

/**
 * @brief Holds the other cores out of every task
 *
 * Core 0 code that is not a task, the boot and the machine states, has
 * always run with no task alongside it. It waits here for the callbacks
 * other cores are in to return, and none starts until mp_task_cpus_release().
 */
void mp_task_cpus_hold(void) {
	if(_mp_task_cpu_up < 2)
		return;
	mp_task_cpu_lock();
	_mp_task_cpu_gate++;
	mp_task_cpu_unlock();
	while(_mp_task_cpu_busy)
		;
}

void mp_task_cpus_release(void) {
	int cpu;

	if(_mp_task_cpu_up < 2)
		return;
	mp_task_cpu_lock();
	_mp_task_cpu_gate--;
	mp_task_cpu_unlock();
	for(cpu = 1; cpu < _mp_task_cpu_up; cpu++)
		mp_cpu_kick(cpu);
}

static void _mp_task_cpu_busy_leave(void) {
	mp_task_cpu_lock();
	_mp_task_cpu_busy--;
	mp_task_cpu_unlock();
}

static void _mp_task_cpu_entry(void) {
	mp_task_cpu_run(_mp_task_cpu_hdl);
}

int mp_task_cpus_start(mp_task_handler_t *hdl) {
	int cpu;

	if(_mp_task_cpu_up > 1)
		return(_mp_task_cpu_up);
	_mp_task_cpu_hdl = hdl;
	for(cpu = 1; cpu < MP_CPUS; cpu++) {
		if(mp_cpu_start(cpu, _mp_task_cpu_entry) != YES)
			break;
		_mp_task_cpu_up = cpu + 1;
	}
	return(_mp_task_cpu_up);
}

#endif

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

#if MP_TASK_SHARE

/* Two priority-1 callbacks that do not return until each has seen the other.
 * A direct call, nested on someone else's stack, returns immediately: spinning
 * there would pin that stack. The lock is held across three ticks, long enough
 * that a broken ceiling would let the peer move. */
static volatile unsigned _mp_task_share_a_n;
static volatile unsigned _mp_task_share_b_n;
static volatile unsigned char _mp_task_share_a_done;
static volatile unsigned char _mp_task_share_b_done;
static volatile unsigned char _mp_task_share_abort;
static volatile unsigned char _mp_task_share_bad;
static unsigned short _mp_task_share_wait;
static unsigned short _mp_task_share_abs;
static mp_task_t *_mp_task_share_a;
static mp_task_t *_mp_task_share_b;

static MP_TASK(_mp_task_share_a_run) {
	unsigned frozen;
	unsigned long t0;
	mp_task_lock_t key;

	if(task->signal == MP_TASK_SIG_STOP || _mp_task_share_abort) {
		_mp_task_share_a_done = 1;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	/* Nested on another callback's stack. Spinning here would not come back. */
	if(!task->share_live)
		return;

	while(!_mp_task_share_b_n && !_mp_task_share_abort)
		_mp_task_share_a_n++;
	if(_mp_task_share_abort) {
		_mp_task_share_a_done = 1;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	key = mp_task_lock(MP_TASK_CEILING_KERNEL);
	frozen = _mp_task_share_b_n;
	t0 = mp_clock_ticks();
	while((mp_clock_ticks() - t0) < 3UL && !_mp_task_share_abort)
		_mp_task_share_a_n++;
	if(_mp_task_share_b_n != frozen)
		_mp_task_share_bad = 1;
	mp_task_unlock(key);

	while(_mp_task_share_b_n == frozen && !_mp_task_share_abort)
		_mp_task_share_a_n++;
	_mp_task_share_a_done = 1;
	task->signal = MP_TASK_SIG_DEAD;
}

static MP_TASK(_mp_task_share_b_run) {
	if(task->signal == MP_TASK_SIG_STOP || _mp_task_share_abort) {
		_mp_task_share_b_done = 1;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	if(!task->share_live)
		return;

	while(!_mp_task_share_a_n && !_mp_task_share_abort)
		_mp_task_share_b_n++;
	while(!_mp_task_share_a_done && !_mp_task_share_abort)
		_mp_task_share_b_n++;
	_mp_task_share_b_done = 1;
	task->signal = MP_TASK_SIG_DEAD;
}

static MP_TASK(_mp_task_share_watch) {
	if(task->signal == MP_TASK_SIG_STOP) {
		_mp_task_share_abort = 1;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(!_mp_task_share_a) {
		_mp_task_share_abort = 0;
		_mp_task_share_bad = 0;
		_mp_task_share_a_n = 0;
		_mp_task_share_b_n = 0;
		_mp_task_share_a_done = 0;
		_mp_task_share_b_done = 0;
		_mp_task_share_wait = 0;
		_mp_task_share_abs = 0;
		_mp_task_share_a = mp_task_create(task->handler, "ShareA", _mp_task_share_a_run, NULL, 0);
		_mp_task_share_b = mp_task_create(task->handler, "ShareB", _mp_task_share_b_run, NULL, 0);
		if(!_mp_task_share_a || !_mp_task_share_b) {
			mp_printk("SELFTEST: share failed");
			_mp_task_share_abort = 1;
			task->signal = MP_TASK_SIG_DEAD;
			return;
		}
		/* peers on one core: the switch between them is what this measures */
		mp_task_affinity(_mp_task_share_a, 1);
		mp_task_affinity(_mp_task_share_b, 1);
		mp_task_priority(_mp_task_share_a, 1);
		mp_task_priority(_mp_task_share_b, 1);
		mp_task_delay(task, 1);
		return;
	}

	if(_mp_task_share_a_done && _mp_task_share_b_done) {
		if(!_mp_task_share_abort && !_mp_task_share_bad && _mp_task_share_a_n && _mp_task_share_b_n)
			mp_printk("SELFTEST: share passed");
		else
			mp_printk("SELFTEST: share failed");
		_mp_task_share_a = NULL;
		_mp_task_share_b = NULL;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	_mp_task_share_abs++;
	if(_mp_task_share_a->share_live || _mp_task_share_b->share_live)
		_mp_task_share_wait++;
	if(_mp_task_share_wait > 400 || _mp_task_share_abs > _MP_TASK_SELF_WAIT)
		_mp_task_share_abort = 1;
	if(_mp_task_share_abort && _mp_task_share_abs > _MP_TASK_SELF_WAIT + 100) {
		mp_printk("SELFTEST: share failed");
		_mp_task_share_a = NULL;
		_mp_task_share_b = NULL;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	mp_task_delay(task, 1);
}

static void _mp_task_share_selftest(mp_task_handler_t *hdl) {
	mp_task_t *task;

	if(MP_TASK_PRIORITIES < 3) {
		mp_printk("SELFTEST: share skipped");
		return;
	}
	task = mp_task_create(hdl, "Share", _mp_task_share_watch, NULL, 1);
	if(!task) {
		mp_printk("SELFTEST: share failed");
		return;
	}
	mp_task_priority(task, (unsigned char)(MP_TASK_PRIORITIES - 1));
}

#endif

#if MP_CPUS > 1

/* One task pinned to core 1, one any core may take. Both add to a counter
 * under mp_task_lock with a split read-modify-write, so a lock that does
 * not exclude the other core loses counts. Pass: the pinned task only ever
 * ran on core 1 and the sum is exact. */
#define _MP_TASK_SMP_RUNS 32
#define _MP_TASK_SMP_ADDS 500

static volatile unsigned _mp_task_smp_mask_a;
static volatile unsigned _mp_task_smp_mask_b;
static volatile unsigned _mp_task_smp_runs_a;
static volatile unsigned _mp_task_smp_runs_b;
static volatile unsigned long _mp_task_smp_sum;
static volatile unsigned char _mp_task_smp_abort;
static unsigned short _mp_task_smp_wait;
static mp_task_t *_mp_task_smp_a;

static void _mp_task_smp_add(void) {
	mp_task_lock_t key;
	unsigned long v;
	int i;

	key = mp_task_lock(MP_TASK_CEILING_KERNEL);
	for(i = 0; i < _MP_TASK_SMP_ADDS; i++) {
		v = _mp_task_smp_sum;
		__asm volatile ("" ::: "memory");
		_mp_task_smp_sum = v + 1;
	}
	mp_task_unlock(key);
}

static MP_TASK(_mp_task_smp_a_run) {
	if(task->signal == MP_TASK_SIG_STOP || _mp_task_smp_abort) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	_mp_task_smp_mask_a |= 1U << mp_cpu_id();
	_mp_task_smp_add();
	if(++_mp_task_smp_runs_a >= _MP_TASK_SMP_RUNS)
		task->signal = MP_TASK_SIG_DEAD;
}

static MP_TASK(_mp_task_smp_b_run) {
	if(task->signal == MP_TASK_SIG_STOP || _mp_task_smp_abort) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	_mp_task_smp_mask_b |= 1U << mp_cpu_id();
	_mp_task_smp_add();
	if(++_mp_task_smp_runs_b >= _MP_TASK_SMP_RUNS)
		task->signal = MP_TASK_SIG_DEAD;
}

static MP_TASK(_mp_task_smp_watch) {
	mp_task_t *b;

	if(task->signal == MP_TASK_SIG_STOP) {
		_mp_task_smp_abort = 1;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(!_mp_task_smp_a) {
		if(mp_task_cpus() < 2) {
			mp_printk("SELFTEST: smp failed (core 1 silent)");
			task->signal = MP_TASK_SIG_DEAD;
			return;
		}
		_mp_task_smp_a = mp_task_create(task->handler, "SmpPin", _mp_task_smp_a_run, NULL, 1);
		b = mp_task_create(task->handler, "SmpAny", _mp_task_smp_b_run, NULL, 1);
		if(!_mp_task_smp_a || !b) {
			mp_printk("SELFTEST: smp failed");
			_mp_task_smp_abort = 1;
			task->signal = MP_TASK_SIG_DEAD;
			return;
		}
		mp_task_affinity(_mp_task_smp_a, 1U << 1);
		mp_task_affinity(b, MP_TASK_CPU_ANY);
		mp_task_delay(task, 10);
		return;
	}

	if(_mp_task_smp_runs_a >= _MP_TASK_SMP_RUNS && _mp_task_smp_runs_b >= _MP_TASK_SMP_RUNS) {
		if(_mp_task_smp_mask_a == (1U << 1) &&
				_mp_task_smp_sum == 2UL * _MP_TASK_SMP_RUNS * _MP_TASK_SMP_ADDS)
			mp_printk("SELFTEST: smp passed (%d cores, any ran on 0x%x)",
				mp_task_cpus(), _mp_task_smp_mask_b);
		else
			mp_printk("SELFTEST: smp failed (pin 0x%x, sum %lu)",
				_mp_task_smp_mask_a, _mp_task_smp_sum);
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(++_mp_task_smp_wait > 2000) {
		mp_printk("SELFTEST: smp failed (pin %u runs, any %u runs)",
			_mp_task_smp_runs_a, _mp_task_smp_runs_b);
		_mp_task_smp_abort = 1;
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}
	mp_task_delay(task, 10);
}

static void _mp_task_smp_selftest(mp_task_handler_t *hdl) {
	mp_task_t *task;

	_mp_task_smp_mask_a = 0;
	_mp_task_smp_mask_b = 0;
	_mp_task_smp_runs_a = 0;
	_mp_task_smp_runs_b = 0;
	_mp_task_smp_sum = 0;
	_mp_task_smp_abort = 0;
	_mp_task_smp_wait = 0;
	_mp_task_smp_a = NULL;
	task = mp_task_create(hdl, "Smp", _mp_task_smp_watch, NULL, 1);
	if(!task)
		mp_printk("SELFTEST: smp failed");
}

#endif

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
	#if MP_TASK_SHARE
		_mp_task_share_selftest(hdl);
	#else
		mp_printk("SELFTEST: share skipped");
	#endif
	#if MP_CPUS > 1
		_mp_task_smp_selftest(hdl);
	#endif
}

mp_task_tick_t mp_task_tick(mp_task_handler_t *hdl) {
#if MP_CPUS > 1
	if(mp_cpu_id() == 0)
		_mp_task_cpu_deep = 0;
#endif
	/* assert */
	if(hdl->usedNumber == 0) {
		if(hdl->signal == MP_TASK_SIG_STOP)
			return(MP_TASK_STOPPED);
		return(MP_TASK_WORKING);
	}

	return(mp_task_sched_tick(hdl));
}

/**@}*/
