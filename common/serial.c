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

#ifdef SUPPORT_COMMON_SERIAL

static void _mp_serial_on_read(mp_uart_t *uart);
static void _mp_serial_on_write(mp_uart_t *uart);

mp_ret_t mp_serial_init(mp_kernel_t *kernel, mp_serial_t *serial, mp_options_t *options, char *who) {
	mp_ret_t ret;

	/* initialize UART */
	ret = mp_uart_open(kernel, &serial->uart, options, who);
	if(ret == FALSE)
		return(FALSE);

	serial->uart.onRead = _mp_serial_on_read;
	serial->uart.onWrite = _mp_serial_on_write;
	serial->uart.user = serial;

	serial->rx_size = 0;
	serial->rx_pos = 0;
	serial->tx_size = 0;
	serial->tx_pos = 0;

	return(TRUE);
}

mp_ret_t mp_serial_fini(mp_serial_t *serial) {

	return(TRUE);
}

void mp_serial_println(mp_serial_t *serial, char *text) {
	int len = strlen(text);
	mp_serial_write(serial, text, len);
	mp_serial_write(serial, "\n\r", 2);

}

void mp_serial_write(mp_serial_t *serial, char *input, int size) {
	int rest = sizeof(serial->tx_buffer)-serial->tx_size;
	int a;

	for(a=0; a<size; a++)
		mp_uart_tx(&serial->uart, input[a]);

	return;

	/* enable RX */

	/* no more space flush output now */
	if(rest < size) {

		/* disable TX interrupt */
		mp_uart_disable_tx_int(&serial->uart);

		for(a=serial->tx_pos; a<serial->tx_size; a++)
			mp_uart_tx(&serial->uart, serial->tx_buffer[a]);
		serial->tx_size = 0;
		serial->tx_pos = 0;

		/* input buffer biggest than tx buffer flushing now */
		if(size > sizeof(serial->tx_buffer)) {
			for(a=0; a<size; a++)
				mp_uart_tx(&serial->uart, input[a]);

			/* end of buffer disable RX */
		}
		else {
			memcpy(serial->tx_buffer, input, size);
			serial->tx_size += size;

			/* enable TX interrupt */
			mp_uart_enable_tx_int(&serial->uart);
		}
	}
	/* space available */
	else {
		/* disable TX interrupt */
		mp_uart_disable_tx_int(&serial->uart);

		memcpy(serial->tx_buffer+serial->tx_pos, input, size);
		serial->tx_size += size;

		/* enable TX interrupt */
		mp_uart_enable_tx_int(&serial->uart);

	}

}


static void _mp_serial_on_read(mp_uart_t *uart) {
	mp_serial_t *serial = uart->user;

}

static void _mp_serial_on_write(mp_uart_t *uart) {
	mp_serial_t *serial = uart->user;

}

#endif

