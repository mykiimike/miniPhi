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

/* Internal contract between task.c and the files in sched/.
 * Include <mp.h> first. Drivers and the app never include this.
 */

#ifndef _HAVE_TASK_SCHED_H
	#define _HAVE_TASK_SCHED_H

	/* one of sched/coop.c, priority.c, preempt.c provides these */
	void mp_task_sched_boot(mp_task_handler_t *hdl);
	mp_task_tick_t mp_task_sched_tick(mp_task_handler_t *hdl);

	/* shared path. exact < 0 is any priority. ceiling < 0 is no ceiling,
	 * otherwise the task priority must be greater than ceiling. */
	mp_bool_t mp_task_ready(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now);
	void mp_task_execute(mp_task_handler_t *hdl, mp_task_t *task, unsigned long now);
	void mp_task_rearm(mp_task_handler_t *hdl, mp_task_t *task);
	void mp_task_rearm_all(mp_task_handler_t *hdl);
	mp_task_tick_t mp_task_sched_result(mp_task_handler_t *hdl);
	mp_task_t *mp_task_pick(mp_task_handler_t *hdl, unsigned long now, int exact, int ceiling);
	void mp_task_pass_enter(void);
	void mp_task_pass_leave(void);
	/** Tasks inside execute. 0 when the scheduler is between callbacks. */
	int mp_task_running(void);
	/** Forget served marks above ceiling for tasks that have returned. */
	void mp_task_served_open(mp_task_handler_t *hdl, int ceiling);

	#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PREEMPT
	/* IRQ entry. The series software interrupt calls this. The weak
	 * mp_task_pend runs it on the caller stack while that line is absent. */
	void mp_task_sched_dispatch(void);
	int mp_task_sched_preempts(mp_task_t *task);
	/** 1 when a ceiling lock is held above this task, so a peer must not take its stack. */
	int mp_task_share_blocked(mp_task_t *task);
	#endif

#endif
