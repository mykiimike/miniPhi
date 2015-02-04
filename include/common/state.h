#ifndef _HAVE_MP_STATE_H
	#define _HAVE_MP_STATE_H

	/**
	 * @defgroup mpCommonState
	 * @{
	 */

	typedef struct mp_state_handler_s mp_state_handler_t;
	typedef struct mp_state_s mp_state_t;

	typedef void (*mp_state_callback_t)(void *user);

	struct mp_state_s {
		char number;
		char *name;
		char used;
		void *user;
		mp_state_callback_t set;
		mp_state_callback_t unset;
		mp_state_callback_t tick;
	};

	struct mp_state_handler_s {
		mp_state_t states[MP_STATE_MAX];

		char currentState;
		char changeState;

	};

	/** @} */

	void mp_state_init(mp_state_handler_t *hdl);
	void mp_state_fini(mp_state_handler_t *hdl);
	mp_ret_t mp_state_switch(mp_state_handler_t *hdl, char number);
	void mp_state_tick(mp_state_handler_t *hdl);
	mp_ret_t mp_state_define(
		mp_state_handler_t *hdl,
		char number, char *name, void *user,
		mp_state_callback_t set,
		mp_state_callback_t unset,
		mp_state_callback_t tick
	);
#endif
