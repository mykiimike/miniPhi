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

/**
@defgroup mpArchESP32S3UART USB Serial/JTAG

@ingroup mpArchESP32

The console is the USB Serial/JTAG CDC the ROM already enumerated.
RTC_CNTL_USB_RESET_DISABLE stops a console open from resetting the
chip. Bytes accumulate in the IN fifo and one WR_DONE, when the burst
ends, publishes them as a single packet. A full 64-byte packet stays
in the host until a shorter one follows, so a burst stops at 63.
Baud, parity and pins are ignored: the gate is "USB".

@{
*/

#define _EP1        (S3_USB + 0x00UL)
#define _EP1_CONF   (S3_USB + 0x04UL)
#define _INT_ST     (S3_USB + 0x0CUL)
#define _INT_ENA    (S3_USB + 0x10UL)
#define _INT_CLR    (S3_USB + 0x14UL)
#define _USB_CONF   (S3_RTC + 0x120UL)

#define _WR_DONE    (1UL << 0)
#define _IN_FREE    (1UL << 1)
#define _OUT_AVAIL  (1UL << 2)
#define _OUT_INT    (1UL << 2)
#define _IN_INT     (1UL << 3)

static void _dummy_read(mp_uart_t *uart) {
	(void)s3_read(_EP1);
	(void)uart;
}

static void _dummy_write(mp_uart_t *uart) {
	mp_uart_disable_tx_int(uart);
}

static void _usb_isr(void *user) {
	mp_uart_t *uart = user;
	unsigned long status = s3_read(_INT_ST);
	unsigned long other = status & ~(_OUT_INT | _IN_INT);

	if(other)
		s3_write(_INT_CLR, other);

	if(status & _OUT_INT) {
		s3_write(_INT_CLR, _OUT_INT);
		while(s3_read(_EP1_CONF) & _OUT_AVAIL) {
			if(uart->onRead)
				uart->onRead(uart);
			else
				(void)s3_read(_EP1);
		}
	}

	if((status & _IN_INT) && (s3_read(_INT_ENA) & _IN_INT) && uart->onWrite) {
		int n = 0;

		/* A 64-byte packet is not delivered until a later short one. Stay under that. */
		while(n < 63 && (s3_read(_INT_ENA) & _IN_INT) && (s3_read(_EP1_CONF) & _IN_FREE)) {
			uart->onWrite(uart);
			n++;
		}
		/* onWrite flushes when the buffer runs out. A second WR_DONE
		 * replaces that packet with an empty one. */
		if(s3_read(_INT_ENA) & _IN_INT) {
			s3_write(_EP1_CONF, _WR_DONE);
			s3_write(_INT_CLR, _IN_INT);
		}
	}
}

mp_ret_t mp_uart_init(void) {
	s3_set(_USB_CONF, 1UL << 17);
	s3_write(_INT_CLR, 0x7ffffUL);
	s3_write(_INT_ENA, 0);
	return(TRUE);
}

mp_ret_t mp_uart_fini(void) {
	return(TRUE);
}

mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who) {
	char *value;

	memset(uart, 0, sizeof(*uart));
	uart->kernel = kernel;
	uart->onRead = _dummy_read;
	uart->onWrite = _dummy_write;

	value = mp_options_get(options, "gate");
	uart->gate = mp_gate_handle(value, who);
	if(!uart->gate)
		return(FALSE);

	if(strcmp(uart->gate->portDevice, "USB") != 0) {
		mp_printk("UART %s is not implemented", uart->gate->portDevice);
		mp_uart_close(uart);
		return(FALSE);
	}

	if(!mp_interrupt_set(S3_SRC_USB, _usb_isr, uart, who)) {
		mp_uart_close(uart);
		return(FALSE);
	}

	return(mp_uart_setup(uart, options));
}

mp_ret_t mp_uart_setup(mp_uart_t *uart, mp_options_t *options) {
	char *value;

	value = mp_options_get(options, "baudRate");
	uart->baudRate = value ? (unsigned long)atol(value) : 115200UL;
	return(TRUE);
}

mp_ret_t mp_uart_close(mp_uart_t *uart) {
	if(!uart || !uart->gate)
		return(FALSE);
	mp_uart_disable_tx_int(uart);
	mp_uart_disable_rx_int(uart);
	mp_interrupt_unset(S3_SRC_USB);
	mp_gate_release(uart->gate);
	uart->gate = NULL;
	uart->onRead = NULL;
	uart->onWrite = NULL;
	return(TRUE);
}

void mp_uart_retime(void) {
}

mp_bool_t mp_uart_quiet(void) {
	return(s3_read(_INT_ENA) & _IN_INT ? NO : YES);
}

void mp_uart_wake(mp_bool_t arm) {
	(void)arm;
}

void mp_uart_enable_tx_int(mp_uart_t *uart) {
	(void)uart;
	/* The empty-fifo interrupt is level. Enabling it runs the handler,
	 * which publishes a short packet. A direct write here would make
	 * that packet 64 bytes, and the host would hold it. */
	s3_set(_INT_ENA, _IN_INT);
}

void mp_uart_disable_tx_int(mp_uart_t *uart) {
	(void)uart;
	s3_clr(_INT_ENA, _IN_INT);
	s3_write(_EP1_CONF, _WR_DONE);
}

void mp_uart_enable_rx_int(mp_uart_t *uart) {
	(void)uart;
	s3_set(_INT_ENA, _OUT_INT);
}

void mp_uart_disable_rx_int(mp_uart_t *uart) {
	(void)uart;
	s3_clr(_INT_ENA, _OUT_INT);
}

void mp_uart_tx(mp_uart_t *uart, unsigned char data) {
	unsigned int spin = 100000;

	(void)uart;
	while((s3_read(_EP1_CONF) & _IN_FREE) == 0 && spin)
		spin--;
	if(!spin)
		return;
	s3_write(_EP1, data);
}

unsigned char mp_uart_rx(mp_uart_t *uart) {
	(void)uart;
	return((unsigned char)s3_read(_EP1));
}

char mp_uart_isBusy(mp_uart_t *uart) {
	(void)uart;
	return(s3_read(_EP1_CONF) & _IN_FREE ? 0 : 1);
}

/**@}*/
