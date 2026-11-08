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
#include "../include/modem.h"

/**
@defgroup mpArchESP32Modem Modem domain

@ingroup mpArchESP32

One refcount for the clocks shared by the PHY and every MAC.
The series owns the register sequence (mp_arch_modem_on / off).

@{
*/

static unsigned char _held[MP_ARCH_MODEM_COUNT];
static int _users;

mp_ret_t mp_arch_modem_get(mp_arch_modem_user_t user) {
	if((unsigned)user >= MP_ARCH_MODEM_COUNT)
		return(FALSE);
	if(_held[user])
		return(TRUE);
	if(_users == 0)
		mp_arch_modem_on();
	_held[user] = 1;
	_users++;
	return(TRUE);
}

void mp_arch_modem_put(mp_arch_modem_user_t user) {
	if((unsigned)user >= MP_ARCH_MODEM_COUNT || !_held[user])
		return;
	_held[user] = 0;
	_users--;
	if(_users == 0)
		mp_arch_modem_off();
}

int mp_arch_modem_users(void) {
	return(_users);
}

/**@}*/
