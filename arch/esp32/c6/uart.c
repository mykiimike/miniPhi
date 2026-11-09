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
@defgroup mpArchESP32UART USB Serial/JTAG

@ingroup mpArchESP32

The console is the USB Serial/JTAG CDC the ROM already enumerated.
Opening it does not reset the PHY. Bytes accumulate in the IN fifo and
one WR_DONE, when the burst ends, publishes them as a single packet.
Baud, parity and pins are ignored: the gate is "USB".

@{
*/

#define _CHIP_RST   (C6_USB + 0x4cUL)
#define _EP1        (C6_USB + 0x00UL)
#define _EP1_CONF   (C6_USB + 0x04UL)
#define _INT_ST     (C6_USB + 0x0CUL)
#define _INT_ENA    (C6_USB + 0x10UL)
#define _INT_CLR    (C6_USB + 0x14UL)

#define _CONF0      (C6_USB + 0x18UL)
#define _PULL_OVR   (1UL << 8)
#define _DP_UP      (1UL << 9)
#define _DP_DN      (1UL << 10)

#define _WR_DONE    (1UL << 0)
#define _IN_FREE    (1UL << 1)
#define _OUT_AVAIL  (1UL << 2)
#define _OUT_INT    (1UL << 2)
#define _IN_INT     (1UL << 3)

static void _dummy_read(mp_uart_t *uart) {
	(void)c6_read(_EP1);
	(void)uart;
}

static void _dummy_write(mp_uart_t *uart) {
	mp_uart_disable_tx_int(uart);
}

static void _usb_isr(void *user) {
	mp_uart_t *uart = user;
	unsigned long status = c6_read(_INT_ST);
	unsigned long other = status & ~(_OUT_INT | _IN_INT);

	/* The ROM leaves the bus-reset source enabled. It is level-pending
	 * after the host enumerates, and a handler that ignores it never
	 * returns to the tick. */
	if(other)
		c6_write(_INT_CLR, other);

	if(status & _OUT_INT) {
		c6_write(_INT_CLR, _OUT_INT);
		/* Drain exactly the bytes the OUT fifo holds. The
		 * serial_out_ep_data_avail bit can clear mid-packet while the
		 * PHY is still filling the FIFO, which loses the tail bytes;
		 * out_ep1_rec_data_cnt (out_ep1_st bits 22:16) is the
		 * authoritative count of bytes ready. A packet that lands
		 * while we drain latches a fresh edge after this clear. */
		unsigned long st = c6_read(C6_USB + 0x3CUL);
		int cnt = (st >> 16) & 0x7F;
		while(cnt-- > 0) {
			if(uart->onRead)
				uart->onRead(uart);
			else
				(void)c6_read(_EP1);
		}
	}

	if((status & _IN_INT) && (c6_read(_INT_ENA) & _IN_INT) && uart->onWrite) {
		int n = 0;

		while(n < 63 && (c6_read(_INT_ENA) & _IN_INT) && (c6_read(_EP1_CONF) & _IN_FREE)) {
			uart->onWrite(uart);
			n++;
		}
		/* onWrite flushes when the buffer runs out. A second WR_DONE
		 * replaces that packet with an empty one. */
		if(c6_read(_INT_ENA) & _IN_INT) {
			c6_write(_EP1_CONF, _WR_DONE);
			c6_write(_INT_CLR, _IN_INT);
		}
	}
}

static int _usb_ok = 1;

/* A JTAG reset leaves the IN endpoint with a packet the host will never
 * ack. Dropping D+ is the cable pull: the host re-enumerates and the
 * endpoint comes back. A live host that is still acking is left alone. */
static void _usb_reenum(void) {
	unsigned long c;
	unsigned long start;

	c = c6_read(_CONF0);
	c6_write(_CONF0, (c | _PULL_OVR | _DP_DN) & ~_DP_UP);
	start = mp_clock_us();
	while((unsigned long)(mp_clock_us() - start) < 50000UL)
		;
	c = c6_read(_CONF0);
	c6_write(_CONF0, (c | _DP_UP) & ~(_PULL_OVR | _DP_DN));
	start = mp_clock_us();
	while((unsigned long)(mp_clock_us() - start) < 20000UL)
		;
}

static void _usb_recover(void) {
	unsigned long start;

	if(c6_read(_EP1_CONF) & _IN_FREE)
		return;
	start = mp_clock_us();
	while((c6_read(_EP1_CONF) & _IN_FREE) == 0) {
		if((unsigned long)(mp_clock_us() - start) >= 20000UL) {
			_usb_reenum();
			return;
		}
	}
}

mp_ret_t mp_uart_init(void) {
	/* Bit 2 only. Bits 0 and 1 are sticky reset flags; writing them straps
	 * the ROM into download mode. */
	c6_write(_CHIP_RST, 1UL << 2);
	c6_write(_INT_CLR, 0x7ffffUL);
	c6_write(_INT_ENA, 0);
	_usb_recover();
	_usb_ok = 1;
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

	if(!mp_interrupt_set(C6_SRC_USB, _usb_isr, uart, who)) {
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
	mp_interrupt_unset(C6_SRC_USB);
	mp_gate_release(uart->gate);
	uart->gate = NULL;
	uart->onRead = NULL;
	uart->onWrite = NULL;
	return(TRUE);
}

void mp_uart_retime(void) {
}

mp_bool_t mp_uart_quiet(void) {
	return(c6_read(_INT_ENA) & _IN_INT ? NO : YES);
}

void mp_uart_wake(mp_bool_t arm) {
	(void)arm;
}

void mp_uart_enable_tx_int(mp_uart_t *uart) {
	c6_set(_INT_ENA, _IN_INT);
	if((c6_read(_EP1_CONF) & _IN_FREE) && uart->onWrite)
		uart->onWrite(uart);
}

void mp_uart_disable_tx_int(mp_uart_t *uart) {
	(void)uart;
	c6_clr(_INT_ENA, _IN_INT);
	c6_write(_EP1_CONF, _WR_DONE);
}

void mp_uart_enable_rx_int(mp_uart_t *uart) {
	(void)uart;
	c6_set(_INT_ENA, _OUT_INT);
}

void mp_uart_disable_rx_int(mp_uart_t *uart) {
	(void)uart;
	c6_clr(_INT_ENA, _OUT_INT);
}

void mp_uart_tx(mp_uart_t *uart, unsigned char data) {
	unsigned int spin = 100000;

	(void)uart;
	while((c6_read(_EP1_CONF) & _IN_FREE) == 0 && spin)
		spin--;
	if(!spin)
		return;
	c6_write(_EP1, data);
}

static int _usb_skip;

static int _wait_in_free(unsigned long us) {
	unsigned long start;

	if(c6_read(_EP1_CONF) & _IN_FREE)
		return(1);
	start = mp_clock_us();
	while((c6_read(_EP1_CONF) & _IN_FREE) == 0) {
		if((unsigned long)(mp_clock_us() - start) >= us)
			return(0);
	}
	return(1);
}

mp_bool_t mp_uart_host_wait(unsigned long ms) {
	unsigned long start;

	start = mp_clock_us();
	while((c6_read(C6_USB + 0x4CUL) & 2UL) == 0) {
		if((unsigned long)(mp_clock_us() - start) >= ms * 1000UL)
			return(NO);
	}
	start = mp_clock_us();
	while((unsigned long)(mp_clock_us() - start) < 100000UL)
		;
	_usb_skip = 0;
	return(YES);
}

mp_bool_t mp_uart_tx_ok(void) {
	return(_usb_ok ? YES : NO);
}

void mp_uart_write_sync(const unsigned char *data, int size) {
	int i;
	int n;
	unsigned long budget;

	if(size <= 0 || !data)
		return;

	/* IN_FREE stays clear while a packet waits for the host. A second
	 * WR_DONE in that window wedges the endpoint until the cable moves.
	 * Wait, and if the host is not there, give up without touching it. */
	_usb_ok = 0;
	c6_clr(_INT_ENA, _IN_INT);
	budget = _usb_skip ? 1000UL : 50000UL;
	if(!_wait_in_free(budget)) {
		_usb_skip = 1;
		return;
	}
	_usb_skip = 0;
	n = 0;
	for(i = 0; i < size; i++) {
		if(!_wait_in_free(budget)) {
			_usb_skip = 1;
			return;
		}
		c6_write(_EP1, data[i]);
		n++;
		if(n == 64) {
			c6_write(_EP1_CONF, _WR_DONE);
			if(!_wait_in_free(budget)) {
				_usb_skip = 1;
				return;
			}
			n = 0;
		}
	}
	if(n) {
		c6_write(_EP1_CONF, _WR_DONE);
		if(!_wait_in_free(budget)) {
			_usb_skip = 1;
			return;
		}
	}
	_usb_ok = 1;
}

unsigned char mp_uart_rx(mp_uart_t *uart) {
	(void)uart;
	return((unsigned char)c6_read(_EP1));
}

char mp_uart_isBusy(mp_uart_t *uart) {
	(void)uart;
	return(c6_read(_EP1_CONF) & _IN_FREE ? 0 : 1);
}

/**@}*/
