#ifndef _HAVE_TASK_H
	#define _HAVE_TASK_H

	typedef struct mp_task_handler_s mp_task_handler_t;
	typedef struct mp_task_s mp_task_t;

	typedef enum {
		MP_TASK_SIG_OK,
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
