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

void mp_task_init(mp_kernel_t *kernel, mp_task_handler_t *hdl) {
	mp_task_t *task;
	memset(hdl, 0, sizeof(*hdl));

	hdl->kernel = kernel;

	int a;
	for(a=0; a<MP_TASK_MAX; a++) {
		task = &hdl->tasks[a];

		/* add task into the free list */
		mp_list_add_last(&hdl->freeList, &task->item, task);
	}

	/* set global start */
	hdl->signal = MP_TASK_SIG_OK;

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

	/* assert */
	if(hdl->freeList.first == NULL)
		return(NULL);

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

	hdl->usedNumber++;

	/* add to usedList */
	mp_list_add_last(&hdl->usedList, &task->item, task);

	task->signal = MP_TASK_SIG_OK;
	mp_task_signal(task, MP_TASK_SIG_OK);

	return(task);
}

mp_ret_t mp_task_destroy(mp_task_t *task) {
	mp_task_signal(task, MP_TASK_SIG_STOP);
	return(TRUE);
}

void mp_task_signal(mp_task_t *task, mp_task_signal_t signal) {
	if(task->signal != MP_TASK_SIG_SLEEP && signal == MP_TASK_SIG_SLEEP)
		task->handler->sleepNumber++;
	else if(task->signal == MP_TASK_SIG_SLEEP && signal != MP_TASK_SIG_SLEEP)
		task->handler->sleepNumber--;

	if(task->signal != MP_TASK_SIG_PENDING && signal == MP_TASK_SIG_PENDING)
		task->handler->pendingNumber++;
	else if(task->signal == MP_TASK_SIG_PENDING && signal != MP_TASK_SIG_PENDING)
		task->handler->pendingNumber--;

	task->signal = signal;
	mp_clock_task_change(task);
}

mp_task_tick_t mp_task_tick(mp_task_handler_t *hdl) {
	unsigned long now;
	mp_bool_t pass = NO;
	mp_task_t *next;
	mp_task_t *seek;

	/* assert */
	if(hdl->usedNumber == 0) {
		if(hdl->signal == MP_TASK_SIG_STOP)
			return(MP_TASK_STOPPED);
		return(MP_TASK_WORKING);
	}

	/* get clock */
	now = mp_clock_ticks();

	/* just send the message to all tasks */
	seek = hdl->usedList.last->user;
	while(seek != NULL) {
		next = seek->item.prev != NULL ? seek->item.prev->user : NULL;

		/* check the task status : a stop has been sent */
		if(hdl->signal == MP_TASK_SIG_STOP) {
			mp_task_signal(seek, MP_TASK_SIG_STOP);
			pass = YES;
		}
		else if(seek->signal == MP_TASK_SIG_PENDING)
			pass = YES;
		else if(seek->signal == MP_TASK_SIG_SLEEP)
			pass = NO;
		else if(now-seek->check >= seek->delay)
			pass = YES;
		else
			pass = NO;

		if(pass == YES) {
			/* execute wakeup */
			seek->wakeup(seek);

			/* signal was pending restore OK state */
			//if(seek->signal == MP_TASK_SIG_PENDING)
				//mp_task_signal(seek, MP_TASK_SIG_OK);

			/* acknowledge stop dead message */
			if(seek->signal == MP_TASK_SIG_DEAD) {
				/* remove from usedList */
				mp_list_remove(&hdl->usedList, &seek->item);

				/* add to freelist */
				mp_list_add_last(&hdl->freeList, &seek->item, seek);

				/* less task */
				hdl->usedNumber--;
			}
			/* recycle check delay */
			else
				seek->check = now;
		}
		seek = next;
	}

	if(hdl->signal == MP_TASK_SIG_STOP && hdl->usedNumber == 0)
		return(MP_TASK_STOPPED);
	else if(hdl->signal == MP_TASK_SIG_STOP && hdl->usedNumber != 0)
		return(MP_TASK_DESTROYING);

	return(MP_TASK_WORKING);
}

/**@}*/
