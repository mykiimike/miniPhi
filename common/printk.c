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

@brief Common kernel print

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2014
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

This module reference functions used to setup mp_printk() output.

Example 1 : Defined printk callback and use circular buffering
@code
static void _olimex_printk(void *user, char *fmt, ...) {
	olimex_msp430_t *olimex = user;
	unsigned char *buffer = malloc(256);
	va_list args;
	int size;

	va_start(args, fmt);
	size = vsnprintf((char *)buffer, 256-3, fmt, args);
	va_end(args);

	buffer[size++] = '\n';
	buffer[size++] = '\r';

	mp_serial_write(&olimex->serial, buffer, size);

	free(buffer);

	return;
}
@endcode

Example 2 : initialize serial UART and set printk
@code
// setup serial interface
{
	ret = mp_serial_initUART(&olimex->kernel, &olimex->serial, &olimex->proxyUARTDst, "Serial DST UART");
	if(ret == FALSE)
		return;
}

// set printk
mp_printk_set(_olimex_printk, olimex);
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
	return(TRUE);
}


/**@}*/
