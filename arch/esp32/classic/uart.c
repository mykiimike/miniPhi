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
@defgroup mpArchESP32ClassicUART UART

@ingroup mpArchESP32

UART0 is the console, GPIO1 TX and GPIO3 RX, 8N1. The baud divider is
APB / baud with a 4-bit fraction, and APB is the 40 MHz crystal. A
direct IO_MUX function is used when the pin is that UART's pad;
any other pin goes through the GPIO matrix. The three UARTs share one
level-1 line. The host-wait used by the radio test returns immediately:
a CH340 has no DTR bit in this block. Options of mp_uart_open(): gate,
txd and rxd (the gate defaults when missing). Options of
mp_uart_setup(): baudRate. The line stays 8N1.

@{
*/

#define _FIFO     0x00UL
#define _INT_ENA  0x0CUL
#define _INT_CLR  0x10UL
#define _CLKDIV   0x14UL
#define _STATUS   0x1CUL
#define _CONF0    0x20UL
#define _CONF1    0x24UL

#define _RX_INT   ((1UL << 0) | (1UL << 8))
#define _TX_INT   (1UL << 1)
#define _ERR_INT  ((1UL << 2) | (1UL << 3) | (1UL << 4))

#define _TX_FULL  120UL

static const unsigned char _clk_bit[3] = { 2, 5, 23 };
static const unsigned char _direct_tx[3] = { 1, 10, 17 };
static const unsigned char _direct_rx[3] = { 3, 9, 16 };
static const unsigned char _direct_fn[3] = { 0, 4, 4 };
static const unsigned short _signal[3] = { 14, 17, 198 };

typedef struct {
	mp_uart_t *uart;
	unsigned long base;
	int index;
	mp_gpio_port_t *tx;
	mp_gpio_port_t *rx;
	/** the baud is too fast for REF_TICK and follows APB */
	int apb;
} _port_t;

static _port_t _port[3];
static mp_uart_t *_sync;
static int _line;
static int _ok;

static _port_t *_of(mp_uart_t *uart) {
	int i;

	for(i = 0; i < 3; i++) {
		if(_port[i].uart == uart)
			return(&_port[i]);
	}
	return(NULL);
}

static unsigned long _mux(int pin) {
	static const unsigned short off[CL_PIN_COUNT] = {
		0x44, 0x88, 0x40, 0x84, 0x48, 0x6c, 0x60, 0x64,
		0x68, 0x54, 0x58, 0x5c, 0x34, 0x38, 0x30, 0x3c,
		0x4c, 0x50, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x8c,
		0x90, 0x24, 0x28, 0x2c, 0, 0, 0, 0,
		0x1c, 0x20, 0x14, 0x18, 0x04, 0x08, 0x0c, 0x10,
	};

	if(pin < 0 || pin >= CL_PIN_COUNT)
		return(0);
	return(off[pin]);
}

/* CONF0 bit 27 clear clocks the divider from the 1 MHz REF_TICK, which
 * does not move with the clock level. 16 MHz over 115200 is 8 and 11/16,
 * 0.08 % off. Above 250 kbaud the fraction is too coarse and APB is used. */
#define _REF_MAX 250000UL

int cl_uart_apb(void) {
	int i;
	int n = 0;

	for(i = 0; i < 3; i++) {
		if(_port[i].uart && _port[i].apb)
			n++;
	}
	return(n);
}

static void _baud(_port_t *port, unsigned long baud) {
	unsigned long clock;
	unsigned long div;

	if(!baud)
		baud = 115200UL;
	port->apb = baud > _REF_MAX;
	if(port->apb) {
		clock = cl_apb_hz();
		cl_set(port->base + _CONF0, 1UL << 27);
	}
	else {
		clock = CL_REF_TICK_HZ;
		cl_clr(port->base + _CONF0, 1UL << 27);
	}
	div = ((clock << 4) + baud / 2) / baud;
	cl_write(port->base + _CLKDIV, (div >> 4) | ((div & 0xfUL) << 20));
}

static void _format(unsigned long base) {
	cl_write(base + _CONF0, (3UL << 2) | (1UL << 4) | (1UL << 27));
	cl_set(base + _CONF0, (1UL << 17) | (1UL << 18));
	cl_clr(base + _CONF0, (1UL << 17) | (1UL << 18));
	cl_write(base + _CONF1, (1UL << 31) | (2UL << 24) | (8UL << 8) | 1UL);
}

static void _pad(int pin, int function, int pullup) {
	unsigned long value = ((unsigned long)function << 12) | (1UL << 9) | (2UL << 10);
	unsigned long off = _mux(pin);

	if(!off)
		return;
	cl_pad_digital(pin);
	if(pullup)
		value |= 1UL << 8;
	cl_write(CL_IOMUX + off, value);
}

static void _tx_pin(int index, int pin) {
	if(pin == _direct_tx[index]) {
		_pad(pin, _direct_fn[index], 0);
		return;
	}
	_pad(pin, 2, 0);
	cl_write(CL_GPIO + 0x530UL + (unsigned long)pin * 4UL, (1UL << 10) | _signal[index]);
	cl_write(CL_GPIO + (pin < 32 ? 0x24UL : 0x30UL), 1UL << (pin & 31));
}

static void _rx_pin(int index, int pin) {
	/* The matrix input is armed even on the UART's own pad. SIG_IN_SEL
	 * left at 0 does not deliver GPIO3 to the receiver, so the console
	 * transmits and never hears the cursor report. */
	if(pin == _direct_rx[index])
		_pad(pin, _direct_fn[index], 1);
	else
		_pad(pin, 2, 1);
	cl_write(CL_GPIO + 0x130UL + (unsigned long)_signal[index] * 4UL, (1UL << 7) | (unsigned long)pin);
}

static void _dummy_read(mp_uart_t *uart) {
	_port_t *port = _of(uart);

	if(port)
		(void)cl_read(port->base + _FIFO);
}

static void _dummy_write(mp_uart_t *uart) {
	mp_uart_disable_tx_int(uart);
}

static void _isr(void *user) {
	int i;

	(void)user;
	for(i = 0; i < 3; i++) {
		mp_uart_t *uart = _port[i].uart;
		unsigned long base;
		unsigned long st;
		int n;

		if(!uart)
			continue;
		base = _port[i].base;
		st = cl_read(base + 0x08UL);
		if(st & _ERR_INT)
			cl_write(base + _INT_CLR, _ERR_INT);
		if(st & _RX_INT) {
			cl_write(base + _INT_CLR, _RX_INT);
			n = 0;
			while(n < 128 && (cl_read(base + _STATUS) & 0xffUL)) {
				if(uart->onRead)
					uart->onRead(uart);
				else
					(void)cl_read(base + _FIFO);
				n++;
			}
		}
		if((st & _TX_INT) && (cl_read(base + _INT_ENA) & _TX_INT) && uart->onWrite) {
			n = 0;
			while(n < 16 && (cl_read(base + _INT_ENA) & _TX_INT) &&
				((cl_read(base + _STATUS) >> 16) & 0xffUL) < _TX_FULL) {
				uart->onWrite(uart);
				n++;
			}
		}
	}
}

mp_ret_t mp_uart_init(void) {
	int i;

	_sync = NULL;
	_line = 0;
	_ok = 0;
	for(i = 0; i < 3; i++) {
		_port[i].uart = NULL;
		_port[i].tx = NULL;
		_port[i].rx = NULL;
	}
	return(TRUE);
}

mp_ret_t mp_uart_fini(void) {
	int i;

	for(i = 0; i < 3; i++) {
		if(_port[i].uart)
			mp_uart_close(_port[i].uart);
	}
	return(TRUE);
}

mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who) {
	char *value;
	_port_t *port;
	int index;
	unsigned long base;
	mp_gpio_port_t *tx;
	mp_gpio_port_t *rx;

	memset(uart, 0, sizeof(*uart));
	uart->kernel = kernel;
	uart->onRead = _dummy_read;
	uart->onWrite = _dummy_write;

	value = mp_options_get(options, "gate");
	uart->gate = mp_gate_handle(value, who);
	if(!uart->gate)
		return(FALSE);

	base = (unsigned long)uart->gate->base;
	if(base == CL_UART1)
		index = 1;
	else if(base == CL_UART2)
		index = 2;
	else if(base == CL_UART0)
		index = 0;
	else {
		mp_printk("UART %s is not a UART gate", uart->gate->portDevice);
		mp_uart_close(uart);
		return(FALSE);
	}
	port = &_port[index];
	if(port->uart) {
		mp_printk("UART %s is already open", uart->gate->portDevice);
		mp_uart_close(uart);
		return(FALSE);
	}

	value = mp_options_get(options, "txd");
	tx = mp_gpio_text_handle(value ? value : (char *)uart->gate->pin0, who);
	if(!tx) {
		mp_printk("UART %s: TX pin %s is not available", uart->gate->portDevice, value ? value : uart->gate->pin0);
		mp_uart_close(uart);
		return(FALSE);
	}
	value = mp_options_get(options, "rxd");
	rx = mp_gpio_text_handle(value ? value : (char *)uart->gate->pin1, who);
	if(!rx) {
		mp_printk("UART %s: RX pin %s is not available", uart->gate->portDevice, value ? value : uart->gate->pin1);
		mp_gpio_release(tx);
		mp_uart_close(uart);
		return(FALSE);
	}

	port->uart = uart;
	port->base = base;
	port->index = index;
	port->tx = tx;
	port->rx = rx;
	if(!_sync)
		_sync = uart;

	cl_periph(1UL << _clk_bit[index], 1);
	_format(base);
	_tx_pin(index, (int)tx->pin);
	_rx_pin(index, (int)rx->pin);
	cl_write(base + _INT_CLR, _RX_INT | _TX_INT | _ERR_INT);
	cl_write(base + _INT_ENA, 0);

	if(!_line) {
		if(!mp_interrupt_set(CL_SRC_UART0 + index, _isr, NULL, who)) {
			mp_uart_close(uart);
			return(FALSE);
		}
		_line = 1;
	}
	else
		cl_write(CL_INTMTX + (unsigned long)(CL_SRC_UART0 + index) * 4UL, CL_CPU_UART);

	return(mp_uart_setup(uart, options));
}

mp_ret_t mp_uart_setup(mp_uart_t *uart, mp_options_t *options) {
	char *value;
	_port_t *port;

	value = mp_options_get(options, "baudRate");
	uart->baudRate = value ? (unsigned long)atol(value) : 115200UL;
	port = _of(uart);
	if(port)
		_baud(port, uart->baudRate);
	return(TRUE);
}

mp_ret_t mp_uart_close(mp_uart_t *uart) {
	_port_t *port = _of(uart);
	int i;
	int left = 0;
	int source = CL_SRC_UART0;

	if(!uart || !uart->gate)
		return(FALSE);
	if(port) {
		source = CL_SRC_UART0 + port->index;
		cl_write(port->base + _INT_ENA, 0);
		cl_periph(1UL << _clk_bit[port->index], 0);
		if(port->tx)
			mp_gpio_release(port->tx);
		if(port->rx)
			mp_gpio_release(port->rx);
		port->uart = NULL;
		port->tx = NULL;
		port->rx = NULL;
	}
	if(_sync == uart)
		_sync = NULL;
	mp_gate_release(uart->gate);
	uart->gate = NULL;
	uart->onRead = NULL;
	uart->onWrite = NULL;
	for(i = 0; i < 3; i++) {
		if(_port[i].uart) {
			left = 1;
			if(!_sync)
				_sync = _port[i].uart;
		}
	}
	if(!left && _line) {
		mp_interrupt_unset(source);
		_line = 0;
	}
	return(TRUE);
}

void mp_uart_retime(void) {
}

mp_bool_t mp_uart_quiet(void) {
	_port_t *port = _of(_sync);

	if(!port)
		return(YES);
	return(cl_read(port->base + _INT_ENA) & _TX_INT ? NO : YES);
}

void mp_uart_wake(mp_bool_t arm) {
	(void)arm;
}

mp_bool_t mp_uart_host_wait(unsigned long ms) {
	(void)ms;
	return(YES);
}

mp_bool_t mp_uart_host_back(void) {
	return(NO);
}

void mp_uart_enable_tx_int(mp_uart_t *uart) {
	_port_t *port = _of(uart);

	if(!port)
		return;
	cl_set(port->base + _INT_ENA, _TX_INT);
	if(uart->onWrite && ((cl_read(port->base + _STATUS) >> 16) & 0xffUL) < _TX_FULL)
		uart->onWrite(uart);
}

void mp_uart_disable_tx_int(mp_uart_t *uart) {
	_port_t *port = _of(uart);

	if(port)
		cl_clr(port->base + _INT_ENA, _TX_INT);
}

void mp_uart_enable_rx_int(mp_uart_t *uart) {
	_port_t *port = _of(uart);

	if(port)
		cl_set(port->base + _INT_ENA, _RX_INT);
}

void mp_uart_disable_rx_int(mp_uart_t *uart) {
	_port_t *port = _of(uart);

	if(port)
		cl_clr(port->base + _INT_ENA, _RX_INT);
}

void mp_uart_tx(mp_uart_t *uart, unsigned char data) {
	_port_t *port = _of(uart);
	unsigned int spin = 100000;

	if(!port)
		return;
	while(((cl_read(port->base + _STATUS) >> 16) & 0xffUL) >= _TX_FULL && spin)
		spin--;
	if(!spin)
		return;
	cl_write(port->base + _FIFO, data);
}

void mp_uart_rx_poll(mp_uart_t *uart) {
	_port_t *port = _of(uart);
	int n = 0;

	if(!port || !uart->onRead)
		return;
	while(n < 128 && (cl_read(port->base + _STATUS) & 0xffUL)) {
		uart->onRead(uart);
		n++;
	}
}

unsigned char mp_uart_rx(mp_uart_t *uart) {
	_port_t *port = _of(uart);

	if(!port)
		return(0);
	return((unsigned char)cl_read(port->base + _FIFO));
}

char mp_uart_isBusy(mp_uart_t *uart) {
	_port_t *port = _of(uart);

	if(!port)
		return(0);
	return((cl_read(port->base + _STATUS) >> 16) & 0xffUL ? 1 : 0);
}

mp_bool_t mp_uart_tx_ok(void) {
	return(_ok ? YES : NO);
}

void mp_uart_write_sync(const unsigned char *data, int size) {
	_port_t *port;
	unsigned long ena;
	int i;

	if(!_sync || size <= 0 || !data)
		return;
	port = _of(_sync);
	if(!port)
		return;
	_ok = 0;
	ena = cl_read(port->base + _INT_ENA);
	cl_clr(port->base + _INT_ENA, _TX_INT);
	for(i = 0; i < size; i++) {
		unsigned long start = mp_clock_us();

		while(((cl_read(port->base + _STATUS) >> 16) & 0xffUL) >= _TX_FULL) {
			if((unsigned long)(mp_clock_us() - start) > 20000UL) {
				if(ena & _TX_INT)
					cl_set(port->base + _INT_ENA, _TX_INT);
				return;
			}
		}
		cl_write(port->base + _FIFO, data[i]);
	}
	if(ena & _TX_INT)
		cl_set(port->base + _INT_ENA, _TX_INT);
	_ok = 1;
}

/**@}*/
