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

#define _MP_SERIAL_IS_OPEN 0x45

static void mp_serial_UART_rxInt(mp_uart_t *uart);
static void mp_serial_UART_txInt(mp_uart_t *uart);

static void mp_serial_UART_rxIntDisable(mp_circular_t *cir);
static void mp_serial_UART_rxIntEnable(mp_circular_t *cir);
static void mp_serial_UART_txIntDisable(mp_circular_t *cir);
static void mp_serial_UART_txIntEnable(mp_circular_t *cir);

mp_ret_t mp_serial_initUART(mp_kernel_t *kernel, mp_serial_t *serial, mp_uart_t *uart, char *who) {
	mp_ret_t ret;

	memset(serial, 0, sizeof(*serial));

	serial->kernel = kernel;
	serial->uart = uart;

	/* Circular context for TX */
	ret = mp_circular_init(
		kernel, &serial->txCir,
		mp_serial_UART_txIntEnable, mp_serial_UART_txIntDisable
	);
	if(!ret) {
		mp_printk("Serial UART: Can not create TX circular");
		return(FALSE);
	}

	/* Circular context for RX */
	ret = mp_circular_init(
		kernel, &serial->rxCir,
		mp_serial_UART_rxIntEnable, mp_serial_UART_rxIntDisable
	);
	if(!ret) {
		mp_printk("Serial UART: Can not create RX circular");
		return(FALSE);
	}

	serial->txCir.user = serial;
	serial->rxCir.user = serial;

	/* setup interruption vectors */
	serial->uart->user = serial;
	serial->uart->onRead = mp_serial_UART_rxInt;
	serial->uart->onWrite = mp_serial_UART_txInt;

	mp_printk("Initialize Serial over UART: %s", who);

	serial->opened = _MP_SERIAL_IS_OPEN;
	return(TRUE);
}

mp_ret_t mp_serial_fini(mp_serial_t *serial) {

	serial->uart->onRead = NULL;
	serial->uart->onWrite = NULL;
	serial->uart->user = NULL;

	mp_circular_fini(&serial->rxCir);
	mp_circular_fini(&serial->txCir);

	return(TRUE);
}


void mp_serial_write(mp_serial_t *serial, unsigned char *input, int size) {

	if(serial->opened != _MP_SERIAL_IS_OPEN)
		return;

	mp_circular_write(&serial->txCir, input, size);

}

/* UART predefined interfacing */
static void mp_serial_UART_rxInt(mp_uart_t *uart) {
	unsigned char chr;

	mp_serial_t *serial = uart->user;

	/* read register */
	chr = mp_uart_rx(uart);

	/* run circular interrupt service */
	mp_circular_rxInterrupt(&serial->rxCir, chr);
}

static void mp_serial_UART_txInt(mp_uart_t *uart) {
	mp_serial_t *serial = uart->user;
	unsigned char toSend;
	mp_bool_t done;

	/* run circular interrupt service */
	toSend = mp_circular_txInterrupt(&serial->txCir, &done);
	if(!done)
		mp_uart_tx(uart, toSend);

	/* done could be used to manage CTS */

}

static void mp_serial_UART_rxIntDisable(mp_circular_t *cir) {
	mp_serial_t *serial = cir->user;
	mp_uart_disable_rx_int(serial->uart);
	return;
}

static void mp_serial_UART_rxIntEnable(mp_circular_t *cir) {
	mp_serial_t *serial = cir->user;
	mp_uart_enable_rx_int(serial->uart);
	return;
}


static void mp_serial_UART_txIntDisable(mp_circular_t *cir) {
	mp_serial_t *serial = cir->user;
	mp_uart_disable_tx_int(serial->uart);
	return;
}

static void mp_serial_UART_txIntEnable(mp_circular_t *cir) {
	mp_serial_t *serial = cir->user;
	mp_uart_enable_tx_int(serial->uart);

}

#endif

