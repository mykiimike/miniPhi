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

#if MP_TASK_SCHEDULER == MP_TASK_SCHED_COOP

void mp_task_sched_boot(mp_task_handler_t *hdl) {
	(void)hdl;
}

/* Walk usedList from the last task to the first. Every due wakeup runs
 * to completion. Nothing is interrupted. */
mp_task_tick_t mp_task_sched_tick(mp_task_handler_t *hdl) {
	unsigned long now;
	mp_task_t *next;
	mp_task_t *seek;

	now = mp_clock_ticks();

	/* the pass puts back the deadline of every task it visits */
	MP_INTERRUPT_SAFE_BEGIN
	hdl->wakeSet = NO;
	MP_INTERRUPT_SAFE_END

	/* just send the message to all tasks */
	seek = hdl->usedList.last->user;
	while(seek != NULL) {
		next = seek->item.prev != NULL ? seek->item.prev->user : NULL;

		/* check the task status : a stop has been sent */
		if(mp_task_ready(hdl, seek, now) == YES)
			mp_task_execute(hdl, seek, now);

		if(seek->signal == MP_TASK_SIG_OK || seek->signal == MP_TASK_SIG_STOP)
			mp_task_rearm(hdl, seek);
		seek = next;
	}

	return(mp_task_sched_result(hdl));
}

#endif
