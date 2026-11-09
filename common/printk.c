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

static void _dummy_printk(void *user, char *fmt, ...) { }

mp_printk_call_t mp_printk_call;
void *mp_printk_user;

/**
@defgroup mpCommonPrintk printk() feature

@ingroup mpCommon

@brief Common kernel print

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2014
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

This module reference functions used to setup mp_printk() output.

A serial is the usual sink: mp_serial_printk() after the port is open.
A terminal that takes the line unwires printk; mp_serial_printk() then
fails until mp_serial_give().

@code
mp_serial_open(kernel, &serial, options, "Console");
mp_serial_printk(&serial);
mp_printk("up");
@endcode

@{
*/

/**
 * @brief Set output callback for mp_printk()
 *
 * This function sets the callback and the user pointer
 * used at each call of mp_printk().
 *
 * @param[in] call Printk callback
 * @param[in] user Embedded user pointer
 */
mp_ret_t mp_printk_set(mp_printk_call_t call, void *user) {
	mp_printk_call = call;
	mp_printk_user = user;
	return(TRUE);
}

/**
 * @brief Unset output callback
 *
 * This function clear the mp_printk() callback
 */
mp_ret_t mp_printk_unset() {
	mp_printk_call = _dummy_printk;
	mp_printk_user = NULL;
	return(TRUE);
}


/**@}*/
