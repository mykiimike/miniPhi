#include <mp.h>


void mp_state_init(mp_state_handler_t *hdl) {
	memset(&hdl->states, 0, sizeof(hdl->states));

}

void mp_state_fini(mp_state_handler_t *hdl) {


}

mp_ret_t mp_state_switch(mp_state_handler_t *hdl, char number) {
	if(number >= MP_STATE_MAX)
		return(FALSE);
	hdl->changeState = number;
	return(TRUE);
}

void mp_state_tick(mp_state_handler_t *hdl) {
	char save_state = hdl->changeState;
	void *user = hdl->states[hdl->currentState].user;

	/* check for state change */
	if(hdl->currentState != save_state) {
		/* unset the actual state */
		hdl->states[hdl->currentState].unset(user);

		/* set new state*/
		user = hdl->states[save_state].user;
		hdl->states[save_state].set(user);

		hdl->currentState = save_state;
	}

	/* call the machine state */
	hdl->states[hdl->currentState].tick(user);
}

mp_ret_t mp_state_define(
		mp_state_handler_t *hdl,
		char number, char *name, void *user,
		mp_state_callback_t set,
		mp_state_callback_t unset,
		mp_state_callback_t tick
	) {
	mp_state_t *state;

	/* assert */
	if(number >= MP_STATE_MAX)
		return(FALSE);

	/* get and assert state */
	state = &hdl->states[number];
	if(state->used == YES)
		return(FALSE);

	state->number = number;
	state->name = name;
	state->user = user;
	state->used = YES;

	state->set = set;
	state->unset = unset;
	state->tick = tick;

	return(TRUE);
}
