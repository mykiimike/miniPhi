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

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_PRIORITY

void mp_task_sched_boot(mp_task_handler_t *hdl) {
	(void)hdl;
}

/* After each wakeup, search again. Highest priority wins, then the
 * earliest deadline, then the earlier usedList position. A running
 * callback is never interrupted. */
mp_task_tick_t mp_task_sched_tick(mp_task_handler_t *hdl) {
	unsigned long now;
	mp_task_t *task;

	now = mp_clock_ticks();

	MP_INTERRUPT_SAFE_BEGIN
	hdl->wakeSet = NO;
	MP_INTERRUPT_SAFE_END

	mp_task_pass_enter();
	while((task = mp_task_pick(hdl, now, -1, -1)) != NULL)
		mp_task_execute(hdl, task, now);
	mp_task_pass_leave();

	/* check was recycled; drop arms taken mid-pass and recompute */
	MP_INTERRUPT_SAFE_BEGIN
	hdl->wakeSet = NO;
	MP_INTERRUPT_SAFE_END
	mp_task_rearm_all(hdl);
	return(mp_task_sched_result(hdl));
}

#endif
