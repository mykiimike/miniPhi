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


#ifndef _HAVE_MP_SERIAL_H
	#define _HAVE_MP_SERIAL_H

	#ifdef SUPPORT_COMMON_SERIAL
		typedef struct mp_serial_s mp_serial_t;
		typedef struct mp_serial_interface_s mp_serial_interface_t;

		typedef void (*mp_serial_on_t)(mp_serial_t *serial);

		typedef void (*mp_serial_onTx_t)(mp_serial_t *serial, unsigned char chr);
		typedef unsigned char (*mp_serial_onRx_t)(mp_serial_t *serial);
		typedef void (*mp_serial_onInt_t)(mp_serial_t *serial);


		struct mp_serial_interface_s {
			mp_serial_onTx_t tx;
			mp_serial_onRx_t rx;
			mp_serial_onInt_t intRxDisable;
			mp_serial_onInt_t intRxEnable;
			void *user;

		};

		struct mp_serial_s {
			mp_kernel_t *kernel;

			mp_uart_t *uart;

			mp_serial_interface_t iface;

			mp_circular_t txCir;
			mp_circular_t rxCir;

			char opened;
		};

		mp_ret_t mp_serial_initUART(mp_kernel_t *kernel, mp_serial_t *serial, mp_uart_t *uart, char *who);
		mp_ret_t mp_serial_fini(mp_serial_t *serial);


		void mp_serial_write(mp_serial_t *serial, unsigned char *input, int size);
	#endif

#endif
