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

	typedef struct mp_task_handler_s mp_task_handler_t;
	typedef struct mp_task_s mp_task_t;

	typedef enum {
		MP_TASK_SIG_OK,
		MP_TASK_SIG_SLEEP,
		MP_TASK_SIG_STOP,
		MP_TASK_SIG_DEAD,
	} mp_task_signal_t;

	typedef enum {
		MP_TASK_STOPPED,
		MP_TASK_WORKING,
		MP_TASK_DESTROYING,
	} mp_task_tick_t;

	typedef void (*mp_task_wakeup_t)(mp_task_t *task);

	struct mp_task_s {
		/** task name */
		unsigned char *name;

		/** user pointer */
		void *user;

		/** task scheduled delay */
		unsigned long delay;

		/** last checking moment */
		unsigned long check;

		/** task wake up callback */
		mp_task_wakeup_t wakeup;

		/** actual signal */
		mp_task_signal_t signal;

		/** list input */
		mp_list_item_t item;
	};

	struct mp_task_handler_s {
		/** inline tasks */
		mp_task_t tasks[MP_TASK_MAX];

		/** used organized list */
		mp_list_t usedList;

		/** number of items into used list */
		unsigned int usedNumber;

		/** free organized list */
		mp_list_t freeList;

		/** global signal */
		mp_task_signal_t signal;
	};

	void mp_task_init(mp_task_handler_t *hdl);
	void mp_task_fini(mp_task_handler_t *hdl);
	void mp_task_flush(mp_task_handler_t *hdl);
	mp_task_t *mp_task_create(mp_task_handler_t *hdl, void *name, mp_task_wakeup_t wakeup, void *user, unsigned long delay);
	mp_ret_t mp_task_destroy(mp_task_t *task);
	mp_task_tick_t mp_task_tick(mp_task_handler_t *hdl);

	#define MP_TASK(name) void name(mp_task_t *task)

#endif
