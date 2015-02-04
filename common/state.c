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
@defgroup mpCommonState Common state manager

@brief States machine

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2014
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

Common states machine is a library used to manage differents states of a machine.

Each state has 3 callbacks which have to defined.

@li @ref mp_state_callback_t set : When the state is initialized
@li @ref mp_state_callback_t unset : When the state is terminated
@li @ref mp_state_callback_t tick : A state tick (corresponding to the loop)

miniPhi machine states is context free then you can use everywhere in your code.

A state machine context has a limited states defined by @ref MP_STATE_MAX (set to 5 by default).

Example 1 shows how to initialize a machine state context
@code
// initialize logical machine state
mp_state_init(&kernel->states);

// define KPANIC machine state
mp_state_define(
	&kernel->states,
	MP_KERNEL_KPANIC, "KPANIC", kernel,
	_mp_kernel_state_kpanic_set,
	_mp_kernel_state_kpanic_unset,
	_mp_kernel_state_kpanic_tick
);

// define BOOT machine state
mp_state_define(
	&kernel->states,
	MP_KERNEL_BOOT, "BOOT", kernel,
	_mp_kernel_state_boot_set,
	_mp_kernel_state_boot_unset,
	_mp_kernel_state_boot_tick
);
@endcode

Example 2 shows how to switch machine state n1 :
@code
mp_state_switch(&kernel->states, 1);
@endcode


@{
*/

/**
  * @brief Initialise state machine context
  * @param[in] hdl Context
  */
void mp_state_init(mp_state_handler_t *hdl) {
	memset(&hdl->states, 0, sizeof(hdl->states));

}


/**
  * @brief Terminate state machine context
  * @param[in] hdl Context
  */
void mp_state_fini(mp_state_handler_t *hdl) {


}


/**
  * @brief Change machine state
  * @param[in] hdl Context
  * @param[in] number Machine state number
  * @return TRUE or FALSE
  */
mp_ret_t mp_state_switch(mp_state_handler_t *hdl, char number) {
	if(number >= MP_STATE_MAX)
		return(FALSE);
	hdl->changeState = number;
	return(TRUE);
}

/**
  * @brief General machine state tick interrupt
  * @param[in] hdl Context
  */
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

/**
  * @brief Define a machine state
  * @param[in] hdl Context
  * @param[in] number Machine state number
  * @param[in] name Name of the state
  * @param[in] user Embedded user pointer
  * @param[in] set Setup callback
  * @param[in] unset Unset callback
  * @param[in] tick Tick callback
  *
  */
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

/**@}*/
