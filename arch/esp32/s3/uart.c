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
chip. The SOF counter freezes while the cable is out and moves again
when the host enumerates, which is how a replug is told from a reset.
Bytes accumulate in the IN fifo and one WR_DONE, when the burst
ends, publishes them as a single packet. A full 64-byte packet stays
in the host until a shorter one follows, so a burst stops at 63.
Baud, parity and pins are ignored: the gate is "USB".

@{
*/

#define _EP1        (S3_USB + 0x00UL)
#define _EP1_CONF   (S3_USB + 0x04UL)
#define _INT_RAW    (S3_USB + 0x08UL)
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
	/* bit 9 is the USB bus reset the host raises on enumeration.
	 * Leave it latched: mp_uart_host_back reads it to tell a
	 * replug and replay the splash. Clearing it here eats the
	 * signal before the task sees it. */
	unsigned long other = status & ~(_OUT_INT | _IN_INT | (1UL << 9));

	if(other)
		s3_write(_INT_CLR, other);

	if(status & _OUT_INT) {
		s3_write(_INT_CLR, _OUT_INT);
		/* Drain exactly the bytes the OUT fifo holds. The
		 * serial_out_ep_data_avail bit can clear mid-packet while the
		 * PHY is still filling the FIFO, which loses the tail bytes;
		 * out_ep1_rec_data_cnt (out_ep1_st bits 22:16) is the
		 * authoritative count. A packet that lands while we
		 * drain latches a fresh edge after this clear. */
		unsigned long st = s3_read(S3_USB + 0x3CUL);
		int cnt = (st >> 16) & 0x7F;
		while(cnt-- > 0) {
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
		/* onWrite turns the interrupt off when the buffer runs out, and
		 * that publishes the packet. A WR_DONE while one is in flight
		 * (IN_FREE clear) replaces it with an empty packet. */
		if((s3_read(_INT_ENA) & _IN_INT) && (s3_read(_EP1_CONF) & _IN_FREE) && n > 0) {
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
	/* IN_FREE clear means the previous packet is still with the host. */
	if(s3_read(_EP1_CONF) & _IN_FREE)
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

/* The console is written from the term task, not the TX interrupt. A
 * 64-byte packet is held by the host until a shorter one follows, so a
 * packet stops at 63. WR_DONE only while IN_FREE is set. */
#define _PACKET 63

static int _usb_ok;
static int _usb_skip;

static int _wait_in_free(unsigned long us) {
	unsigned long start;

	if(s3_read(_EP1_CONF) & _IN_FREE)
		return(1);
	start = mp_clock_us();
	while((s3_read(_EP1_CONF) & _IN_FREE) == 0) {
		if((unsigned long)(mp_clock_us() - start) >= us)
			return(0);
	}
	return(1);
}

mp_bool_t mp_uart_tx_ok(void) {
	return(_usb_ok ? YES : NO);
}

/* The host re-enumerated: drop whatever the IN fifo still holds, clear the
 * interrupt latches the bus reset raised, and re-arm RX in case the reset
 * cleared it. The USB reset disable bit stays set. */
static void _usb_replug(void) {
	s3_write(_INT_CLR, 0x7ffffUL);
	if(s3_read(_EP1_CONF) & _IN_FREE)
		s3_write(_EP1_CONF, _WR_DONE);
	s3_set(_INT_ENA, _OUT_INT);
	_usb_skip = 0;
	_usb_ok = 0;
}

/* The host is back when, after the SOF counter froze (cable out), either
 * it moves again or the USB bus reset the ROM enumeration raises latches.
 * FRAM_NUM advances every millisecond while the host sends SOF; BUS_RESET
 * latches in INT_RAW on every enumeration. */
mp_bool_t mp_uart_host_back(void) {
	static unsigned long frame, stamp;
	static int seen, away;
	unsigned long sof = s3_read(S3_USB + 0x24UL) & 0x7ffUL;
	unsigned long rst = s3_read(_INT_RAW) & (1UL << 9);
	unsigned long now = mp_clock_us();
	/* forward distance, wrapping at the 11-bit SOF counter */
	unsigned long d = (sof >= frame) ? (sof - frame) : (sof + 2048UL - frame);

	if(rst)
		s3_write(_INT_CLR, 1UL << 9);
	if(!seen) {
		seen = 1;
		frame = sof;
		stamp = now;
		return(NO);
	}
	/* SOF does not fully freeze without a host: it still creeps (~7/s).
	 * Only a real jump (the host sending 1 ms SOF again) counts as "moved". */
	if(d >= 2UL) {
		frame = sof;
		stamp = now;
		if(away) {
			away = 0;
			_usb_replug();
			return(YES);
		}
		return(NO);
	}
	if(!away && (unsigned long)(now - stamp) > 100000UL)
		away = 1;
	if(rst) {
		away = 0;
		_usb_replug();
		return(YES);
	}
	return(NO);
}

void mp_uart_write_sync(const unsigned char *data, int size) {
	int i;
	int n;
	unsigned long budget;

	if(size <= 0 || !data)
		return;

	_usb_ok = 0;
	s3_clr(_INT_ENA, _IN_INT);
	/* fail fast while the host is gone, but once a packet is taken the line
	 * is alive: use the full wait for the rest so a slow poller (1 ms frame)
	 * does not time out mid-burst and wedge the IN fifo. */
	budget = _usb_skip ? 5000UL : 50000UL;
	if(!_wait_in_free(budget)) {
		_usb_skip = 1;
		return;
	}
	_usb_skip = 0;
	budget = 50000UL;
	n = 0;
	for(i = 0; i < size; i++) {
		if(!_wait_in_free(budget)) {
			_usb_skip = 1;
			return;
		}
		s3_write(_EP1, data[i]);
		n++;
		if(n == _PACKET) {
			s3_write(_EP1_CONF, _WR_DONE);
			if(!_wait_in_free(budget)) {
				_usb_skip = 1;
				return;
			}
			n = 0;
		}
	}
	if(n) {
		s3_write(_EP1_CONF, _WR_DONE);
		if(!_wait_in_free(budget)) {
			_usb_skip = 1;
			return;
		}
	}
	_usb_ok = 1;
}

/**@}*/
