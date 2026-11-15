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
#include "../sched.h"

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT

/* One handler. mp_task_init publishes it. Each core has its running
 * priority: the ceiling of a lock holds back that core's own tasks. */
static mp_task_handler_t *_mp_task_live;
static unsigned char _mp_task_runpri_of[MP_CPUS];
#define _mp_task_runpri (_mp_task_runpri_of[mp_cpu_id()])

void mp_task_sched_boot(mp_task_handler_t *hdl) {
	_mp_task_live = hdl;
	_mp_task_runpri = 0;
}

int mp_task_sched_preempts(mp_task_t *task) {
	return(task->priority > _mp_task_runpri);
}

int mp_task_sched_runpri(int cpu) {
	if(cpu < 0 || cpu >= MP_CPUS)
		return(MP_TASK_PRIORITIES);
	return(_mp_task_runpri_of[cpu]);
}

static mp_task_lock_t _mp_task_raise(unsigned char ceiling) {
	mp_task_lock_t key = _mp_task_runpri;

	if(ceiling > key)
		_mp_task_runpri = ceiling;
	return(key);
}

/* The ceiling goes up before the mutex is taken: a task of this core
 * that preempted in between would walk into a mutex its own core holds. */
mp_task_lock_t mp_task_lock(unsigned char ceiling) {
	mp_task_lock_t key;

	if(ceiling >= MP_TASK_PRIORITIES)
		ceiling = (unsigned char)(MP_TASK_PRIORITIES - 1);
	if(_mp_task_live == NULL)
		return(0);
	key = _mp_task_raise(ceiling);
#if MP_CPUS > 1
	mp_task_cpu_mutex_take();
#endif
	return(key);
}

void mp_task_unlock(mp_task_lock_t key) {
	if(_mp_task_live == NULL)
		return;
#if MP_CPUS > 1
	mp_task_cpu_mutex_give();
#endif
	_mp_task_runpri = key;
}

#if MP_CPUS > 1
mp_task_lock_t mp_task_mutex_lock(mp_cpu_mutex_t *mutex) {
	mp_task_lock_t key = _mp_task_raise(MP_TASK_CEILING_KERNEL);

	mp_cpu_mutex_take(mutex);
	return(key);
}

int mp_task_mutex_try(mp_cpu_mutex_t *mutex, mp_task_lock_t *key) {
	*key = _mp_task_raise(MP_TASK_CEILING_KERNEL);
	if(mp_cpu_mutex_try(mutex))
		return(1);
	_mp_task_runpri = *key;
	return(0);
}

void mp_task_mutex_unlock(mp_cpu_mutex_t *mutex, mp_task_lock_t key) {
	mp_cpu_mutex_give(mutex);
	_mp_task_runpri = key;
}
#endif

int mp_task_share_blocked(mp_task_t *task) {
	if(!task)
		return(1);
	return(_mp_task_runpri > task->priority);
}

/* Snapshot the priority at entry. A raise requested while the callback
 * runs stays out of _mp_task_runpri until this returns. */
static void _mp_task_enter(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now) {
	unsigned char saved = _mp_task_runpri;

	if(task->priority > _mp_task_runpri)
		_mp_task_runpri = task->priority;
	mp_task_execute(hdl, task, now);
	_mp_task_runpri = saved;
}

void mp_task_sched_dispatch(void) {
	mp_task_handler_t *hdl = _mp_task_live;
	unsigned long now;
	mp_task_t *task;

	if(hdl == NULL)
		return;
	now = mp_clock_ticks();
	mp_task_pass_enter();
	while((task = mp_task_pick(hdl, now, -1, (int)_mp_task_runpri)) != NULL)
		_mp_task_enter(hdl, task, now);
	mp_task_pass_leave();
}

/* Host tests and a series that has not wired its line use this stub, which
 * runs the dispatcher on the caller stack. A strong mp_task_pend from the
 * series replaces it. That line sits under the tick. */
void __attribute__((weak)) mp_task_pend(void) {
	mp_task_sched_dispatch();
}

/* The timer interrupt calls this. A long callback holds the main loop,
 * so the higher task would otherwise wait until that callback returns.
 * Nobody running: leave it to mp_task_tick, which posts the scheduler. */
void mp_task_slice(void) {
	mp_task_handler_t *hdl = _mp_task_live;
	unsigned long now;

	if(hdl == NULL || mp_task_running() == 0)
		return;
	/* The callback under us owns the pass. Open the marks above it so a
	 * task that already returned can be due again before the callback ends. */
	mp_task_served_open(hdl, (int)_mp_task_runpri);
	now = mp_clock_ticks();
	if(mp_task_pick(hdl, now, -1, (int)_mp_task_runpri) == NULL)
		return;
	mp_task_sched_dispatch();
	mp_task_served_open(hdl, (int)_mp_task_runpri);
}

/* Priority 0 stays on mp_task_tick so the idle path keeps its deadline.
 * Without sharing, anything above the running priority goes through
 * mp_task_pend. With sharing, every ready task runs here in thread mode
 * so the sched line can switch it onto its own stack. */
mp_task_tick_t mp_task_sched_tick(mp_task_handler_t *hdl) {
	unsigned long now;
	mp_task_t *task;

	now = mp_clock_ticks();

#if MP_CPUS == 1
	/* Shared across cores, the deadline is only rebuilt in one step below:
	 * cleared here, core 0 could read "nothing due" while another core is
	 * in its pass. */
	MP_INTERRUPT_SAFE_BEGIN
	hdl->wakeSet = NO;
	MP_INTERRUPT_SAFE_END
#endif

	mp_task_pass_enter();
	if(hdl->signal == MP_TASK_SIG_STOP) {
		while((task = mp_task_pick(hdl, now, -1, -1)) != NULL)
			_mp_task_enter(hdl, task, now);
	}
#if MP_TASK_SHARE
	/* Inline, from thread mode. execute() of a priority above 0 arms a
	 * private stack and waits here until the switch gives it back. The
	 * sched line sits under the tick, so this wait is the one place that
	 * line can run. A call from the tick stays a direct callback. */
	else {
		while((task = mp_task_pick(hdl, now, -1, -1)) != NULL)
			_mp_task_enter(hdl, task, now);
	}
#else
	else {
		if(mp_task_pick(hdl, now, -1, (int)_mp_task_runpri) != NULL)
			mp_task_pend();
		while((task = mp_task_pick(hdl, now, 0, -1)) != NULL) {
			_mp_task_enter(hdl, task, now);
			if(mp_task_pick(hdl, now, -1, (int)_mp_task_runpri) != NULL)
				mp_task_pend();
		}
	}
#endif
	mp_task_pass_leave();

	MP_INTERRUPT_SAFE_BEGIN
	hdl->wakeSet = NO;
	mp_task_rearm_all(hdl);
	MP_INTERRUPT_SAFE_END
	return(mp_task_sched_result(hdl));
}

#endif
