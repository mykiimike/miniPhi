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

static void _mp_serial_printk(void *user, char *fmt, ...);

static mp_serial_t *_mp_serial_printk_line;

static int _mp_serial_is_open(mp_serial_t *serial) {
	return(serial && serial->opened == _MP_SERIAL_IS_OPEN);
}

static int _mp_serial_holds_printk(mp_serial_t *serial) {
	return(mp_printk_call == _mp_serial_printk && mp_printk_user == serial);
}

static void _mp_serial_unprintk(mp_serial_t *serial) {
	if(_mp_serial_holds_printk(serial))
		mp_printk_unset();
	if(_mp_serial_printk_line == serial)
		_mp_serial_printk_line = NULL;
	if(serial->use == MP_SERIAL_PRINTK)
		serial->use = MP_SERIAL_NONE;
}

static mp_ret_t _mp_serial_attach(mp_kernel_t *kernel, mp_serial_t *serial, mp_uart_t *uart, char *who) {
	mp_ret_t ret;

	serial->kernel = kernel;
	serial->uart = uart;
	serial->use = MP_SERIAL_NONE;

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
	mp_serial_UART_txIntDisable(&serial->txCir);
	mp_serial_UART_rxIntDisable(&serial->rxCir);

	/* setup interruption vectors */
	serial->uart->user = serial;
	serial->uart->onRead = mp_serial_UART_rxInt;
	serial->uart->onWrite = mp_serial_UART_txInt;

	mp_circular_start(&serial->rxCir);

	mp_printk("Initialize Serial over UART: %s", who);

	serial->opened = _MP_SERIAL_IS_OPEN;
	return(TRUE);
}

mp_ret_t mp_serial_initUART(mp_kernel_t *kernel, mp_serial_t *serial, mp_uart_t *uart, char *who) {
	memset(serial, 0, sizeof(*serial));
	return(_mp_serial_attach(kernel, serial, uart, who));
}

mp_ret_t mp_serial_open(mp_kernel_t *kernel, mp_serial_t *serial, mp_options_t *options, char *who) {
	memset(serial, 0, sizeof(*serial));

	if(mp_uart_open(kernel, &serial->port, options, who) == FALSE)
		return(FALSE);

	if(_mp_serial_attach(kernel, serial, &serial->port, who) == FALSE) {
		mp_uart_close(&serial->port);
		return(FALSE);
	}

	serial->owned = YES;
	return(TRUE);
}

mp_ret_t mp_serial_fini(mp_serial_t *serial) {
	mp_uart_t *uart;

	if(!_mp_serial_is_open(serial))
		return(FALSE);

	mp_serial_give(serial);

	serial->uart->onRead = NULL;
	serial->uart->onWrite = NULL;
	serial->uart->user = NULL;
	serial->reader = NULL;
	mp_power_release(serial->kernel, &serial->awake);

	mp_circular_fini(&serial->rxCir);
	mp_circular_fini(&serial->txCir);

	uart = serial->uart;
	serial->opened = 0;
	serial->uart = NULL;

	if(serial->owned == YES)
		mp_uart_close(uart);

	return(TRUE);
}

mp_ret_t mp_serial_printk(mp_serial_t *serial) {
	if(!_mp_serial_is_open(serial))
		return(FALSE);
	if(serial->use == MP_SERIAL_TERMINAL)
		return(FALSE);
	if(_mp_serial_printk_line && _mp_serial_printk_line != serial)
		_mp_serial_unprintk(_mp_serial_printk_line);
	mp_printk_set(_mp_serial_printk, serial);
	_mp_serial_printk_line = serial;
	serial->use = MP_SERIAL_PRINTK;
	return(TRUE);
}

mp_ret_t mp_serial_terminal(mp_serial_t *serial) {
	if(!_mp_serial_is_open(serial))
		return(FALSE);
	_mp_serial_unprintk(serial);
	serial->use = MP_SERIAL_TERMINAL;
	return(TRUE);
}

void mp_serial_give(mp_serial_t *serial) {
	if(!_mp_serial_is_open(serial))
		return;
	_mp_serial_unprintk(serial);
	serial->use = MP_SERIAL_NONE;
}

mp_ret_t mp_serial_write(mp_serial_t *serial, unsigned char *input, int size) {

	if(!_mp_serial_is_open(serial))
		return(TRUE);

	if(mp_uart_sync(serial->uart) == YES) {
		mp_uart_write_sync(input, size);
		return(TRUE);
	}
	return(mp_circular_write(&serial->txCir, input, size));

}

int mp_serial_pending(mp_serial_t *serial) {
	if(!_mp_serial_is_open(serial) || mp_uart_sync(serial->uart) == YES)
		return(0);
	return(serial->txCir.totalSize);
}

mp_bool_t mp_serial_sync(mp_serial_t *serial) {
	return(mp_uart_sync(serial ? serial->uart : NULL));
}

mp_bool_t mp_serial_delivered(mp_serial_t *serial) {
	(void)serial;
	return(mp_uart_tx_ok());
}

mp_bool_t mp_serial_host_back(mp_serial_t *serial) {
	(void)serial;
	return(mp_uart_host_back());
}

static void _mp_serial_printk(void *user, char *fmt, ...) {
	mp_serial_t *serial = user;
	unsigned char buffer[MP_SERIAL_PRINTK_MAX];
	va_list args;
	int size;

	if(!_mp_serial_is_open(serial) || serial->use != MP_SERIAL_PRINTK)
		return;

	va_start(args, fmt);
	size = vsnprintf((char *)buffer, sizeof(buffer) - 2, fmt, args);
	va_end(args);

	if(size < 0)
		return;
	if(size > (int)sizeof(buffer) - 3)
		size = sizeof(buffer) - 3;

	buffer[size++] = '\n';
	buffer[size++] = '\r';

	mp_serial_write(serial, buffer, size);
}

/* UART predefined interfacing */
static void mp_serial_UART_rxInt(mp_uart_t *uart) {
	unsigned char chr;

	mp_serial_t *serial = uart->user;

	/* read register */
	chr = mp_uart_rx(uart);

	/* run circular interrupt service */
	mp_circular_rxInterrupt(&serial->rxCir, chr);

	/* someone types: stay out of Stop, which loses the first byte */
	mp_power_hold_for(serial->kernel, &serial->awake, MP_POWER_LOW, MP_SERIAL_AWAKE, "Serial");
	if(serial->reader)
		mp_task_wake(serial->reader);
}

static void mp_serial_UART_txInt(mp_uart_t *uart) {
	mp_serial_t *serial = uart->user;
	unsigned char toSend;
	mp_bool_t done;
	int room;

	/* one packet per call: a byte callback misses the host's next poll.
	 * 0 is a byte port. A bulk port with no room returns negative and
	 * keeps the line, so the next completion can take the packet. */
	room = mp_uart_tx_room(uart);
	if(room < 0)
		return;
	if(room > 0) {
		unsigned char buf[64];
		int n;

		if(room > (int)sizeof(buf))
			room = (int)sizeof(buf);
		n = mp_circular_txTake(&serial->txCir, buf, room, &done);
		if(n)
			mp_uart_write(uart, buf, n);
		if(done == YES)
			mp_circular_txStop(&serial->txCir);
		return;
	}
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
