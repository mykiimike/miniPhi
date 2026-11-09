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
		#ifndef MP_SERIAL_AWAKE
			#define MP_SERIAL_AWAKE 30000
		#endif
		#ifndef MP_SERIAL_PRINTK_MAX
			#define MP_SERIAL_PRINTK_MAX 128
		#endif

		typedef struct mp_serial_s mp_serial_t;

		/**
		 * Who has the line. Printk and a terminal cannot share it.
		 * mp_serial_write() is the byte pipe either way.
		 */
		typedef enum {
			MP_SERIAL_NONE = 0,
			MP_SERIAL_PRINTK,
			MP_SERIAL_TERMINAL,
		} mp_serial_use_t;

		struct mp_serial_s {
			mp_kernel_t *kernel;

			mp_uart_t *uart;

			mp_circular_t txCir;
			mp_circular_t rxCir;

			/* port opened by mp_serial_open, closed by mp_serial_fini */
			mp_uart_t port;
			mp_bool_t owned;

			char opened;
			mp_serial_use_t use;

			/* woken on each byte received, NULL for none */
			mp_task_t *reader;

			/* held at LOW for MP_SERIAL_AWAKE ms after each byte received */
			mp_power_hold_t awake;
		};

		mp_ret_t mp_serial_initUART(mp_kernel_t *kernel, mp_serial_t *serial, mp_uart_t *uart, char *who);
		mp_ret_t mp_serial_open(mp_kernel_t *kernel, mp_serial_t *serial, mp_options_t *options, char *who);
		mp_ret_t mp_serial_fini(mp_serial_t *serial);

		/**
		 * Wire mp_printk() to this serial. Each call is one line, at most
		 * MP_SERIAL_PRINTK_MAX bytes, then CR LF. FALSE when the serial is
		 * not open or a terminal has the line. Another serial that was
		 * the sink is given back.
		 */
		mp_ret_t mp_serial_printk(mp_serial_t *serial);

		/**
		 * A terminal takes the line. Printk is unwired if it was this
		 * serial. Further mp_serial_printk() fails until mp_serial_give().
		 * FALSE when the serial is not open.
		 */
		mp_ret_t mp_serial_terminal(mp_serial_t *serial);

		/**
		 * Give the line back. The UART stays open. Printk is unwired
		 * if it was this serial.
		 */
		void mp_serial_give(mp_serial_t *serial);

		void mp_serial_write(mp_serial_t *serial, unsigned char *input, int size);
	#endif

#endif
