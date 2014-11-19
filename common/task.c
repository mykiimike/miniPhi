#include <mp.h>

void mp_task_init(mp_task_handler_t *hdl) {
	mp_task_t *task;
	memset(hdl, 0, sizeof(*hdl));

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

	/* just send the message to all tasks */
	mp_task_flush(hdl);
}

void mp_task_flush(mp_task_handler_t *hdl) {
	mp_task_t *next;
	mp_task_t *seek;

	/* assert */
	if(hdl->usedList.first == NULL)
		return;

	/* just send the message to all tasks */
	seek = hdl->usedList.first->user;
	while(seek != NULL) {
		next = seek->item.next != NULL ? seek->item.next->user : NULL;

		/* force to shutdown tasks */
		seek->signal = MP_TASK_SIG_STOP;

		seek = next;
	}

}

mp_task_t *mp_task_create(mp_task_handler_t *hdl, void *name, mp_task_wakeup_t wakeup, void *user, unsigned long delay) {
	mp_task_t *task;

	/* assert */
	if(hdl->freeList.first == NULL)
		return(NULL);

	/* allocate task */
	task = hdl->freeList.last->user;

	/* remove from freeList */
	mp_list_remove(&hdl->freeList, &task->item);

	/* prepare task */
	task->check = mp_clock_ticks();
	task->delay = delay;
	task->name = name;
	task->wakeup = wakeup;
	task->user = user;

	hdl->usedNumber++;

	task->signal = MP_TASK_SIG_OK;

	/* add to usedList */
	mp_list_add_last(&hdl->usedList, &task->item, task);

	return(task);
}

mp_ret_t mp_task_destroy(mp_task_t *task) {
	task->signal = MP_TASK_SIG_STOP;
	return(TRUE);
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
			seek->signal = MP_TASK_SIG_STOP;
			pass = YES;
		}
		else if(now-seek->check >= seek->delay)
			pass = YES;
		else
			pass = NO;

		if(pass == YES) {
			if(seek->signal == MP_TASK_SIG_OK || seek->signal == MP_TASK_SIG_STOP) {
				/* execute wakeup */
				seek->wakeup(seek);
			}

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
