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

#ifndef _HAVE_TASK_H
	#define _HAVE_TASK_H

	/**
	 * @defgroup mpCommonTask
	 * @{
	 */

	typedef struct mp_task_handler_s mp_task_handler_t;
	typedef struct mp_task_s mp_task_t;

	typedef enum {
		MP_TASK_SIG_OK,
		MP_TASK_SIG_SLEEP,
		MP_TASK_SIG_PENDING,
		MP_TASK_SIG_STOP,
		MP_TASK_SIG_DEAD,
	} mp_task_signal_t;

	typedef enum {
		MP_TASK_STOPPED,
		MP_TASK_WORKING,
		MP_TASK_DESTROYING,
	} mp_task_tick_t;

	typedef void (*mp_task_wakeup_t)(mp_task_t *task);

	/* mp_task_idle() when nothing is due */
	#define MP_TASK_FOREVER 0xFFFFFFFFUL

	/* Numeric: generate.js writes `#define MP_TASK_SCHEDULER 0`, which #if can see. */
	#define MP_TASK_SCHED_COOP     0
	#define MP_TASK_SCHED_PRIORITY 1
	#define MP_TASK_SCHED_PREEMPT  2
	#ifndef MP_TASK_SCHEDULER
	#define MP_TASK_SCHEDULER MP_TASK_SCHED_COOP
	#endif
	#ifndef MP_TASK_PRIORITIES
	#define MP_TASK_PRIORITIES 4
	#endif

	/** The scheduler this build compiled. */
	static inline const char *mp_task_scheduler_name(void) {
		#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PRIORITY
			return("priority");
		#elif MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
			return("preemptive");
		#else
			return("cooperative");
		#endif
	}

	struct mp_task_s {
		/** Connected to handler */
		mp_task_handler_t *handler;

		/** task name */
		char *name;

		/** user pointer */
		void *user;

		/** task scheduled delay */
		unsigned long delay;

		/** last checking moment */
		unsigned long check;

		/** task wake up callback */
		mp_task_wakeup_t wakeup;

		/** actual signal */
		volatile mp_task_signal_t signal;

		/** wake ups since creation */
		unsigned long runs;

		/** 0 default, ignored by coop */
		unsigned char priority;

		/** list input */
		mp_list_item_t item;
	};

	struct mp_task_handler_s {
		/** Kernel connection */
		mp_kernel_t *kernel;

		/** inline tasks */
		mp_task_t tasks[MP_TASK_MAX];

		/** used organized list */
		mp_list_t usedList;

		/** number of items into used list */
		unsigned int usedNumber;

		/** Number of sleeping tasks */
		unsigned int sleepNumber;

		/** Number of pending tasks */
		volatile unsigned int pendingNumber;

		/** soonest deadline of the tasks, when wakeSet is YES */
		volatile unsigned long wakeAt;
		volatile mp_bool_t wakeSet;

		/** free organized list */
		mp_list_t freeList;

		/** global signal */
		mp_task_signal_t signal;
	};

	/** @} */

	void mp_task_init(mp_kernel_t *kernel, mp_task_handler_t *hdl);
	void mp_task_fini(mp_task_handler_t *hdl);
	void mp_task_flush(mp_task_handler_t *hdl);

	mp_task_t *mp_task_create(mp_task_handler_t *hdl, char *name, mp_task_wakeup_t wakeup, void *user, unsigned long delay);
	mp_ret_t mp_task_destroy(mp_task_t *task);

	void mp_task_signal(mp_task_t *task, mp_task_signal_t signal);
	void mp_task_wake(mp_task_t *task);
	void mp_task_delay(mp_task_t *task, unsigned long delay);
	void mp_task_priority(mp_task_t *task, unsigned char priority);
	unsigned long mp_task_idle(mp_task_handler_t *hdl);

	mp_task_tick_t mp_task_tick(mp_task_handler_t *hdl);

	/** Eight passes of a priority-0 task and a priority-2 task.
	 * Prints SELFTEST: sched passed, or sched failed. The tasks die. */
	void mp_task_selftest_start(mp_task_handler_t *hdl);

	/* Ceiling lock. Coop and priority compile this to nothing.
	 * Preempt raises the running priority so only a higher task can preempt.
	 * Tasks never block, so the lock cannot deadlock. */
	typedef unsigned char mp_task_lock_t;
	#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
		mp_task_lock_t mp_task_lock(unsigned char ceiling);
		void mp_task_unlock(mp_task_lock_t key);
		/* The series pends its software interrupt. */
		void mp_task_pend(void);
		/**
		 * From the timer interrupt. Runs a task already due above the one
		 * this interrupt cut. The main loop does not reach mp_task_tick
		 * while a callback is still on the stack.
		 */
		void mp_task_slice(void);
	#else
		#define mp_task_lock(ceiling) ((mp_task_lock_t)0)
		#define mp_task_unlock(key)   ((void)(key))
	#endif
	#define MP_TASK_CEILING_KERNEL ((unsigned char)(MP_TASK_PRIORITIES - 1))

	#define MP_TASK(name) void name(mp_task_t *task)

#endif
