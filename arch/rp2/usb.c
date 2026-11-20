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

#include <stddef.h>
#include <string.h>

/* The firmware includes these through mp.h. This file names them itself so
 * `arm-none-eabi-gcc -Iarch/rp2/include -c arch/rp2/usb.c` can type-check it. */
typedef signed char mp_ret_t;
typedef signed char mp_bool_t;
typedef struct mp_kernel_s mp_kernel_t;
typedef struct mp_options_s mp_options_t;
typedef struct mp_list_item_s mp_list_item_t;

struct mp_list_item_s {
	mp_list_item_t *next;
	mp_list_item_t *prev;
	void *user;
};

#define TRUE 1
#define FALSE 0
#define YES 1
#define NO 0

/* MP_INTERRUPT_SAFE is the kernel lock only when task.h sees MP_CPUS > 1:
 * without it the driver races the other core. task.h sets MP_CPUS from
 * MP_ARCH_CPUS and the scheduler. -DMP_CPUS overrides it. A type-check of
 * this file alone has neither header and assumes one core. */
#if __has_include(<config.h>)
	#include <config.h>
#endif
#if __has_include(<task/cpus.h>)
	#include <task/cpus.h>
#endif
#ifndef MP_CPUS
	#define MP_CPUS 1
#endif

#include "include/rp2350.h"
#include "include/interrupt.h"
#include "include/gpio.h"
#include "include/gate.h"
#include "include/uart.h"

void mp_clock_usb_floor(void);

/**
@defgroup mpArchRP2350USB USB console

@ingroup mpArchRP2350

CDC ACM on the USB device controller (DPRAM 0x50100000, registers
0x50110000). clk_usb is the 48 MHz PLL_USB output. Gate "USB" has no
GPIO: the connector is the dedicated USB PHY. Baud is remembered for
GET_LINE_CODING and otherwise ignored.

The block does not enumerate on its own. This is the device function:
EP0 control, EP1 interrupt IN (serial state, left NAK'd), EP2 bulk OUT
and EP2 bulk IN for the console byte stream. A byte waits in a 64-byte
packet until the packet is full or the TX interrupt is masked, which is
the flush at the end of a serial burst. The IN packet is one DPRAM
buffer; the next one is copied in the completion so the host's retry
still finds it armed.

@{
*/

typedef void (*mp_printk_call_t)(void *user, char *fmt, ...);
extern mp_printk_call_t mp_printk_call;
extern void *mp_printk_user;

char *mp_options_get(mp_options_t *options, char *key);
mp_ret_t mp_options_cmp(char *a, char *b);
mp_bool_t mp_clock_usb48(void);
void mp_flash_bootsel(void);

typedef char _usb_dpram_epx_ok[(offsetof(usb_dpram_t, epx_data) == 0x180) ? 1 : -1];

enum {
	_EP0_IDLE = 0,
	_EP0_IN_DATA,
	_EP0_IN_STATUS,
	_EP0_OUT_DATA,
	_EP0_OUT_STATUS
};

static const uint8_t _dev_desc[] = {
	18, 1,
	0x00, 0x02,
	0x02, 0x00, 0x00,
	64,
	0x8A, 0x2E,
	0x0A, 0x00,
	0x00, 0x01,
	1, 2, 3,
	1
};

/* 67 bytes: config, CDC comm + call/ACM/union, EP1 IN, data iface, EP2 OUT/IN.
 * VID 0x2E8A / PID 0x000A is the Pico SDK CDC id, so a host that already
 * knows a Pico serial port binds this one too. */
static const uint8_t _cfg_desc[] = {
	9, 2, 67, 0, 2, 1, 0, 0x80, 50,
	9, 4, 0, 0, 1, 0x02, 0x02, 0x01, 0,
	5, 0x24, 0x00, 0x10, 0x01,
	5, 0x24, 0x01, 0x00, 0x01,
	4, 0x24, 0x02, 0x02,
	5, 0x24, 0x06, 0x00, 0x01,
	7, 5, 0x81, 0x03, 8, 0, 16,
	9, 4, 1, 0, 2, 0x0A, 0x00, 0x00, 0,
	7, 5, 0x02, 0x02, 64, 0, 0,
	7, 5, 0x82, 0x02, 64, 0, 0
};

static const uint8_t _lang_desc[] = { 4, 3, 0x09, 0x04 };
static const uint8_t _status_desc[2] = { 0, 0 };

typedef char _usb_dev_desc_ok[(sizeof(_dev_desc) == 18) ? 1 : -1];
typedef char _usb_cfg_desc_ok[(sizeof(_cfg_desc) == 67) ? 1 : -1];
typedef char _usb_ep_layout_ok[(USB_EP2_IN_OFF + 128 <= USB_EP1_IN_OFF) ? 1 : -1];

static mp_uart_t *_usb_uart;
static volatile unsigned char _configured;
/* bits 0 and 1: that EP2 IN buffer is armed. Both can be on the wire. */
static volatile unsigned char _tx_mask;
static volatile unsigned char _tx_int;
static volatile unsigned char _rx_int;
/* bytes waiting for a free IN buffer. 256 so a completion can refill both. */
static unsigned char _tx_ring[256];
static unsigned short _tx_head;
static unsigned short _tx_count;
static unsigned char _rxq[128];
static volatile unsigned char _rx_head;
static volatile unsigned char _rx_tail;
static unsigned char _ep2_in_pid;
static unsigned char _ep2_out_pid;
static unsigned char _ep0_pid;
static unsigned char _ep0_stage;
static unsigned char _ep0_set_addr;
static unsigned char _ep0_addr;
static unsigned char _ep0_out_line;
static unsigned char _line[7];
static const uint8_t *_ep0_src;
static uint16_t _ep0_left;
static unsigned char _ep0_tmp[64];
static unsigned char _cfg_byte;
static unsigned char _irq_on;
static unsigned char _setup_now;

static void _kick(mp_uart_t *uart);
static void _pump(void);
static void _arm_out(void);
static void _endpoints(void);
static void _ep0_in_chunk(void);
static void _ep0_status_in(void);
static void _give(volatile uint32_t *ctrl, volatile uint8_t *dst, const uint8_t *src, uint16_t len, int in, int pid);

static void _say(char *text) {
	if(mp_printk_call)
		mp_printk_call(mp_printk_user, "%s", text);
}

static unsigned long _num(char *text) {
	unsigned long n = 0;

	if(!text)
		return(0);
	while(*text >= '0' && *text <= '9') {
		n = n * 10UL + (unsigned long)(*text - '0');
		text++;
	}
	return(n);
}

static void _dmb(void) {
	__asm volatile ("dmb" ::: "memory");
}

static void _give(volatile uint32_t *ctrl, volatile uint8_t *dst, const uint8_t *src, uint16_t len, int in, int pid) {
	uint32_t val = (uint32_t)len | USB_BUF_CTRL_AVAIL;
	uint16_t i;

	if(in) {
		if(src && dst) {
			for(i = 0; i < len; i++)
				dst[i] = src[i];
		}
		val |= USB_BUF_CTRL_FULL;
	}
	if(pid)
		val |= USB_BUF_CTRL_DATA1_PID;
	_dmb();
	*ctrl = val;
	(void)*ctrl;
}

static void _endpoints(void) {
	USB_DPRAM->ep_ctrl[0].in = USB_EP_CTRL_ENABLE | USB_EP_CTRL_INT_1BUF
		| (USB_EP_TYPE_INT << USB_EP_CTRL_TYPE_LSB) | USB_EP1_IN_OFF;
	USB_DPRAM->ep_ctrl[1].out = USB_EP_CTRL_ENABLE | USB_EP_CTRL_INT_1BUF
		| (USB_EP_TYPE_BULK << USB_EP_CTRL_TYPE_LSB) | USB_EP2_OUT_OFF;
	/* DOUBLE plus an interrupt per buffer: one buffer refills while the other is on the wire */
	USB_DPRAM->ep_ctrl[1].in = USB_EP_CTRL_ENABLE | USB_EP_CTRL_DOUBLE | USB_EP_CTRL_INT_1BUF
		| (USB_EP_TYPE_BULK << USB_EP_CTRL_TYPE_LSB) | USB_EP2_IN_OFF;
}

/* bytes the ring can still take */
static int _tx_room(void) {
	if(!_configured || _tx_count >= 256)
		return(0);
	return(256 - (int)_tx_count);
}

/* buffer 0 first whenever it is idle, so a selector reset always lands on it */
static int _tx_slot_free(void) {
	if(_tx_mask == 0 || (_tx_mask & 1u) == 0)
		return(0);
	if((_tx_mask & 2u) == 0)
		return(1);
	return(-1);
}

static int _tx_can(void) {
	return(_tx_room() > 0);
}

/* clk_sys is above 48 MHz: the controller must see the control word before AVAILABLE. */
static void _buf_settle(void) {
	__asm volatile (
		"nop\nnop\nnop\nnop\nnop\nnop\n"
		"nop\nnop\nnop\nnop\nnop\nnop\n"
		::: "memory");
}

/* STRH touches one buffer. A 32-bit store rewrites the other half and can
 * raise AVAILABLE on a buffer the host still owns. 0 if that buffer is still armed. */
static int _arm_in(unsigned slot, const unsigned char *src, unsigned n) {
	volatile uint8_t *dst = &USB_DPRAM->epx_data[(USB_EP2_IN_OFF - 0x180) + (slot << 6)];
	volatile uint16_t *half = (volatile uint16_t *)&USB_DPRAM->ep_buf_ctrl[2].in;
	uint16_t val;
	unsigned i;

	if(slot)
		half++;
	if(*half & USB_BUF_CTRL_AVAIL)
		return(0);
	for(i = 0; i < n; i++)
		dst[i] = src[i];
	val = (uint16_t)n | USB_BUF_CTRL_FULL;
	if(_ep2_in_pid)
		val |= USB_BUF_CTRL_DATA1_PID;
	/* only buffer 0 can reset the selector, and only while the other is idle */
	if(slot == 0 && _tx_mask == 0)
		val |= USB_BUF_CTRL_SEL;
	_dmb();
	__asm volatile ("strh %0, [%1]" :: "r"(val), "r"(half) : "memory");
	_buf_settle();
	val = (uint16_t)(val | USB_BUF_CTRL_AVAIL);
	__asm volatile ("strh %0, [%1]" :: "r"(val), "r"(half) : "memory");
	_ep2_in_pid ^= 1u;
	_tx_mask |= (unsigned char)(1u << slot);
	return(1);
}

static void _arm_out(void) {
	uint32_t val = 64 | USB_BUF_CTRL_AVAIL;

	if(_ep2_out_pid)
		val |= USB_BUF_CTRL_DATA1_PID;
	_ep2_out_pid ^= 1u;
	_dmb();
	USB_DPRAM->ep_buf_ctrl[2].out = val;
}

static void _pump(void) {
	unsigned char pkt[64];
	unsigned n;
	unsigned i;

	if(!_configured)
		return;
	for(;;) {
		int slot = _tx_slot_free();

		if(slot < 0)
			return;
		/* a short packet is the end of a write; a full one can follow it */
		if(_tx_count < 64 && (_tx_int || _tx_count == 0))
			return;
		n = _tx_count > 64 ? 64 : (unsigned)_tx_count;
		for(i = 0; i < n; i++)
			pkt[i] = _tx_ring[(_tx_head + i) & 255];
		if(!_arm_in((unsigned)slot, pkt, n))
			return;
		_tx_head = (unsigned short)((_tx_head + n) & 255);
		_tx_count = (unsigned short)(_tx_count - n);
	}
}

static void _kick(mp_uart_t *uart) {
	unsigned n = 0;

	if(!uart || !_tx_int || !_configured || !uart->onWrite)
		return;
	while(n < 8 && _tx_int && _tx_room() > 0) {
		uart->onWrite(uart);
		n++;
	}
}

static void _rx_push(unsigned char data) {
	unsigned char next = (unsigned char)((_rx_head + 1) & 127);

	if(next == _rx_tail)
		return;
	_rxq[_rx_head] = data;
	_rx_head = next;
}

static void _rx_drain(mp_uart_t *uart) {
	if(!uart || !uart->onRead)
		return;
	while(_rx_int && _rx_head != _rx_tail) {
		unsigned char tail = _rx_tail;

		uart->onRead(uart);
		if(_rx_tail == tail)
			break;
	}
}

static void _ep0_in_chunk(void) {
	uint16_t n = _ep0_left > 64 ? 64 : _ep0_left;

	_give(&USB_DPRAM->ep_buf_ctrl[0].in, USB_DPRAM->ep0_buf_a, _ep0_src, n, 1, _ep0_pid);
	_ep0_pid ^= 1u;
	_ep0_src += n;
	_ep0_left = (uint16_t)(_ep0_left - n);
}

static void _ep0_in_data(const uint8_t *data, uint16_t len, uint16_t wlen) {
	if(len > wlen)
		len = wlen;
	_ep0_src = data;
	_ep0_left = len;
	_ep0_pid = 1;
	if(!len) {
		_ep0_stage = _EP0_OUT_STATUS;
		_give(&USB_DPRAM->ep_buf_ctrl[0].out, 0, 0, 0, 0, 1);
		return;
	}
	_ep0_stage = _EP0_IN_DATA;
	_ep0_in_chunk();
}

static void _ep0_status_in(void) {
	_ep0_stage = _EP0_IN_STATUS;
	_give(&USB_DPRAM->ep_buf_ctrl[0].in, USB_DPRAM->ep0_buf_a, 0, 0, 1, 1);
}

static void _stall(void) {
	USB_EP_STALL_ARM = USB_EP_STALL_ARM_EP0_IN | USB_EP_STALL_ARM_EP0_OUT;
	USB_DPRAM->ep_buf_ctrl[0].in = USB_BUF_CTRL_STALL;
	USB_DPRAM->ep_buf_ctrl[0].out = USB_BUF_CTRL_STALL;
	_ep0_stage = _EP0_IDLE;
}

static uint16_t _string(const char *text) {
	unsigned n = 0;
	unsigned i;

	while(text[n])
		n++;
	if(n > 30)
		n = 30;
	_ep0_tmp[0] = (unsigned char)(2 + n * 2);
	_ep0_tmp[1] = 3;
	for(i = 0; i < n; i++) {
		_ep0_tmp[2 + i * 2] = (unsigned char)text[i];
		_ep0_tmp[3 + i * 2] = 0;
	}
	return(_ep0_tmp[0]);
}

static void _apply_config(void) {
	_endpoints();
	USB_DPRAM->ep_buf_ctrl[2].in = 0;
	_ep2_in_pid = 0;
	_ep2_out_pid = 0;
	_tx_mask = 0;
	_tx_head = 0;
	_tx_count = 0;
	_configured = 1;
	_arm_out();
	_kick(_usb_uart);
}

static void _bus_reset(void) {
	USB_ADDR_ENDP = 0;
	_configured = 0;
	_tx_mask = 0;
	_tx_head = 0;
	_tx_count = 0;
	_ep0_stage = _EP0_IDLE;
	_ep0_set_addr = 0;
	_ep2_in_pid = 0;
	_ep2_out_pid = 0;
	_rx_head = 0;
	_rx_tail = 0;
}

static void _ep0_in_done(void) {
	if(_ep0_stage == _EP0_IN_DATA) {
		if(_ep0_left)
			_ep0_in_chunk();
		else {
			_ep0_stage = _EP0_OUT_STATUS;
			_give(&USB_DPRAM->ep_buf_ctrl[0].out, 0, 0, 0, 0, 1);
		}
		return;
	}
	if(_ep0_stage == _EP0_IN_STATUS) {
		if(_ep0_set_addr) {
			USB_ADDR_ENDP = _ep0_addr & 0x7f;
			_ep0_set_addr = 0;
		}
		_ep0_stage = _EP0_IDLE;
	}
}

static void _ep0_out_done(void) {
	uint32_t ctrl;
	uint16_t len;
	uint16_t i;

	ctrl = USB_DPRAM->ep_buf_ctrl[0].out;
	len = (uint16_t)(ctrl & USB_BUF_CTRL_LEN_MASK);
	if(_ep0_stage == _EP0_OUT_DATA) {
		if(_ep0_out_line) {
			if(len > 7)
				len = 7;
			for(i = 0; i < len; i++)
				_line[i] = USB_DPRAM->ep0_buf_a[i];
			_ep0_out_line = 0;
		}
		_ep0_status_in();
		return;
	}
	if(_ep0_stage == _EP0_OUT_STATUS)
		_ep0_stage = _EP0_IDLE;
}

static void _ep2_out_done(mp_uart_t *uart) {
	uint32_t ctrl = USB_DPRAM->ep_buf_ctrl[2].out;
	uint16_t len = (uint16_t)(ctrl & USB_BUF_CTRL_LEN_MASK);
	uint16_t i;
	volatile uint8_t *src = &USB_DPRAM->epx_data[0];

	if(len > 64)
		len = 64;
	for(i = 0; i < len; i++)
		_rx_push(src[i]);
	_arm_out();
	_rx_drain(uart);
}

static void _ep2_in_done(unsigned buf) {
	if((_tx_mask & (1u << buf)) == 0)
		return;
	_tx_mask = (unsigned char)(_tx_mask & ~(1u << buf));
}

static void _setup(void) {
	uint8_t pkt[8];
	uint16_t wValue;
	uint16_t wLength;
	uint8_t bm;
	uint8_t req;
	unsigned i;

	for(i = 0; i < 8; i++)
		pkt[i] = USB_DPRAM->setup_packet[i];
	USB_SIE_STATUS = USB_SIE_STATUS_SETUP_REC;
	USB_EP_STALL_ARM = 0;
	_ep0_set_addr = 0;
	_ep0_out_line = 0;

	bm = pkt[0];
	req = pkt[1];
	wValue = (uint16_t)pkt[2] | ((uint16_t)pkt[3] << 8);
	wLength = (uint16_t)pkt[6] | ((uint16_t)pkt[7] << 8);

	if((bm & 0x60) == 0x20) {
		if(req == 0x20 && (bm & 0x80) == 0) {
			_ep0_out_line = 1;
			_ep0_stage = _EP0_OUT_DATA;
			_give(&USB_DPRAM->ep_buf_ctrl[0].out, 0, 0, 7, 0, 1);
			return;
		}
		if(req == 0x21 && (bm & 0x80)) {
			_ep0_in_data(_line, 7, wLength);
			return;
		}
		if(req == 0x22 || req == 0x23) {
			_ep0_status_in();
			/* the 1200 baud touch: the port closes (DTR down) at 1200 */
			if(req == 0x22 && !(wValue & 1) &&
					(_line[0] | (uint32_t)_line[1] << 8 | (uint32_t)_line[2] << 16 | (uint32_t)_line[3] << 24) == 1200UL)
				mp_flash_bootsel();
			return;
		}
		_stall();
		return;
	}

	if((bm & 0x60) != 0) {
		_stall();
		return;
	}

	if(bm & 0x80) {
		if(req == 6) {
			switch(wValue >> 8) {
				case 1:
					_ep0_in_data(_dev_desc, sizeof(_dev_desc), wLength);
					return;
				case 2:
					_ep0_in_data(_cfg_desc, sizeof(_cfg_desc), wLength);
					return;
				case 3:
					if((wValue & 0xff) == 0) {
						_ep0_in_data(_lang_desc, sizeof(_lang_desc), wLength);
						return;
					}
					if((wValue & 0xff) == 1) {
						_ep0_in_data(_ep0_tmp, _string("miniPhi"), wLength);
						return;
					}
					if((wValue & 0xff) == 2) {
						_ep0_in_data(_ep0_tmp, _string("RP2350 CDC"), wLength);
						return;
					}
					if((wValue & 0xff) == 3) {
						_ep0_in_data(_ep0_tmp, _string("rp2350"), wLength);
						return;
					}
					break;
				default:
					break;
			}
		}
		else if(req == 0) {
			_ep0_in_data(_status_desc, 2, wLength);
			return;
		}
		else if(req == 8) {
			_cfg_byte = _configured ? 1 : 0;
			_ep0_in_data(&_cfg_byte, 1, wLength);
			return;
		}
		else if(req == 10) {
			_cfg_byte = 0;
			_ep0_in_data(&_cfg_byte, 1, wLength);
			return;
		}
		_stall();
		return;
	}

	if(req == 5) {
		_ep0_addr = (unsigned char)(wValue & 0x7f);
		_ep0_set_addr = 1;
		_ep0_status_in();
		return;
	}
	if(req == 9) {
		if((wValue & 0xff) == 0)
			_configured = 0;
		else
			_apply_config();
		_ep0_status_in();
		return;
	}
	if(req == 1 || req == 3 || req == 11) {
		_ep0_status_in();
		return;
	}
	_stall();
}

static void _buff(mp_uart_t *uart) {
	uint32_t bits = USB_BUFF_STATUS;
	uint32_t bit;

	for(bit = 1; bit && bits; bit <<= 1) {
		if((bits & bit) == 0)
			continue;
		bits &= ~bit;
		if(bit == (1u << 4)) {
			unsigned n = 0;

			/* both buffers can complete together: the bit comes back after the clear.
			 * refill only after both are released, or the new arm is counted as the other completion. */
			while((USB_BUFF_STATUS & bit) && n < 2) {
				unsigned buf = (USB_BUFF_CPU_SHOULD_HANDLE & bit) ? 1u : 0u;

				USB_BUFF_STATUS = bit;
				_ep2_in_done(buf);
				n++;
			}
			_pump();
			_kick(uart);
			_pump();
			continue;
		}
		USB_BUFF_STATUS = bit;
		if(_setup_now && (bit & 3u))
			continue;
		if(bit == 1u)
			_ep0_in_done();
		else if(bit == 2u)
			_ep0_out_done();
		else if(bit == (1u << 5))
			_ep2_out_done(uart);
	}
}

static void _usb_isr(void *user) {
	mp_uart_t *uart = user;
	uint32_t status;

	MP_INTERRUPT_SAFE_BEGIN
	mp_clock_usb_floor();
	status = USB_INTS;
	_setup_now = 0;
	if(status & USB_INTS_BUS_RESET) {
		USB_SIE_STATUS = USB_SIE_STATUS_BUS_RESET;
		_bus_reset();
	}
	if(status & USB_INTS_SETUP_REQ) {
		_setup_now = 1;
		_setup();
	}
	if(status & USB_INTS_BUFF_STATUS)
		_buff(uart);
	_setup_now = 0;
	MP_INTERRUPT_SAFE_END
}

static int _hw_init(void) {
	unsigned spins = 1000000;

	RESETS_RESET |= RESETS_USBCTRL;
	RESETS_RESET &= ~RESETS_USBCTRL;
	while(spins-- && !(RESETS_RESET_DONE & RESETS_USBCTRL));
	if(!(RESETS_RESET_DONE & RESETS_USBCTRL))
		return(0);

	memset((void *)RP2350_USBCTRL_DPRAM_BASE, 0, 4096);
	_configured = 0;
	_tx_mask = 0;
	_tx_head = 0;
	_tx_count = 0;
	_tx_int = 0;
	_ep2_in_pid = 0;
	_rx_int = 0;
	_rx_head = 0;
	_rx_tail = 0;
	_ep0_stage = _EP0_IDLE;
	USB_MUXING = USB_MUXING_TO_PHY | USB_MUXING_SOFTCON;
	USB_PWR = USB_PWR_VBUS_DETECT | USB_PWR_VBUS_DETECT_OVERRIDE_EN;
	USB_MAIN_CTRL = USB_MAIN_CTRL_CONTROLLER_EN;
	USB_SIE_CTRL = USB_SIE_CTRL_EP0_INT_1BUF;
	USB_ADDR_ENDP = 0;
	USB_INTE = USB_INTS_BUFF_STATUS | USB_INTS_BUS_RESET | USB_INTS_SETUP_REQ;
	_endpoints();
	return(1);
}

static void _dummy_read(mp_uart_t *uart) {
	(void)mp_usb_rx(uart);
}

static void _dummy_write(mp_uart_t *uart) {
	mp_usb_disable_tx_int(uart);
}

mp_ret_t mp_usb_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who) {
	char *value;

	memset(uart, 0, sizeof(*uart));
	uart->kernel = kernel;
	uart->onRead = _dummy_read;
	uart->onWrite = _dummy_write;
	uart->usb = YES;

	if(mp_clock_usb48() == NO) {
		_say("USB: 48 MHz clock is off");
		return(FALSE);
	}

	value = mp_options_get(options, "gate");
	uart->gate = mp_gate_handle(value, who);
	if(!uart->gate)
		return(FALSE);
	if(mp_options_cmp(uart->gate->portDevice, "USB") == FALSE) {
		_say("UART gate is not USB");
		mp_usb_uart_close(uart);
		return(FALSE);
	}

	_usb_uart = uart;
	if(!_hw_init()) {
		_say("USB: controller did not leave reset");
		mp_usb_uart_close(uart);
		return(FALSE);
	}
	if(!mp_interrupt_set((int)uart->gate->irq, _usb_isr, uart, who)) {
		mp_usb_uart_close(uart);
		return(FALSE);
	}
	_irq_on = 1;
	USB_SIE_CTRL = USB_SIE_CTRL_EP0_INT_1BUF | USB_SIE_CTRL_PULLUP_EN;
	return(mp_usb_uart_setup(uart, options));
}

mp_ret_t mp_usb_uart_setup(mp_uart_t *uart, mp_options_t *options) {
	char *value;
	unsigned long baud;

	value = mp_options_get(options, "baudRate");
	baud = _num(value);
	if(!baud)
		baud = 115200;
	uart->baudRate = baud;
	_line[0] = (unsigned char)(baud & 0xff);
	_line[1] = (unsigned char)((baud >> 8) & 0xff);
	_line[2] = (unsigned char)((baud >> 16) & 0xff);
	_line[3] = (unsigned char)((baud >> 24) & 0xff);
	_line[4] = 0;
	_line[5] = 0;
	_line[6] = 8;
	return(TRUE);
}

mp_ret_t mp_usb_uart_close(mp_uart_t *uart) {
	if(!uart)
		return(FALSE);

	USB_SIE_CTRL = USB_SIE_CTRL_EP0_INT_1BUF;
	USB_INTE = 0;
	_configured = 0;
	_tx_int = 0;
	_rx_int = 0;
	_tx_mask = 0;
	_tx_head = 0;
	_tx_count = 0;
	if(_irq_on) {
		mp_interrupt_unset(USBCTRL_IRQ);
		_irq_on = 0;
	}
	if(_usb_uart == uart)
		_usb_uart = NULL;
	if(uart->gate) {
		mp_gate_release(uart->gate);
		uart->gate = NULL;
	}
	uart->usb = NO;
	uart->onRead = NULL;
	uart->onWrite = NULL;
	return(TRUE);
}

mp_bool_t mp_usb_linked(void) {
	return(_configured ? YES : NO);
}

mp_bool_t mp_usb_quiet(void) {
	if(!_configured)
		return(YES);
	if(_tx_mask || _tx_count || _tx_int)
		return(NO);
	return(YES);
}

void mp_usb_enable_tx_int(mp_uart_t *uart) {
	MP_INTERRUPT_SAFE_BEGIN
	_tx_int = 1;
	_kick(uart);
	MP_INTERRUPT_SAFE_END
}

void mp_usb_disable_tx_int(mp_uart_t *uart) {
	(void)uart;
	MP_INTERRUPT_SAFE_BEGIN
	_tx_int = 0;
	_pump();
	MP_INTERRUPT_SAFE_END
}

void mp_usb_enable_rx_int(mp_uart_t *uart) {
	MP_INTERRUPT_SAFE_BEGIN
	_rx_int = 1;
	_rx_drain(uart);
	MP_INTERRUPT_SAFE_END
}

void mp_usb_disable_rx_int(mp_uart_t *uart) {
	(void)uart;
	_rx_int = 0;
}

int mp_usb_tx_room(mp_uart_t *uart) {
	int room;

	(void)uart;
	MP_INTERRUPT_SAFE_BEGIN
	room = _tx_room();
	MP_INTERRUPT_SAFE_END
	return(room);
}

int mp_usb_write(mp_uart_t *uart, const unsigned char *data, int n) {
	int put = 0;

	(void)uart;
	if(!data || n <= 0)
		return(0);
	MP_INTERRUPT_SAFE_BEGIN
	while(put < n && _tx_count < 256) {
		_tx_ring[(_tx_head + _tx_count) & 255] = data[put];
		_tx_count++;
		put++;
	}
	_pump();
	MP_INTERRUPT_SAFE_END
	return(put);
}

void mp_usb_tx(mp_uart_t *uart, unsigned char data) {
	unsigned spin = 10000;

	(void)uart;
	while(!_tx_can() && spin)
		spin--;
	if(!_tx_can())
		return;

	MP_INTERRUPT_SAFE_BEGIN
	if(_tx_can()) {
		_tx_ring[(_tx_head + _tx_count) & 255] = data;
		_tx_count++;
		_pump();
	}
	MP_INTERRUPT_SAFE_END
}

void mp_usb_flush(mp_uart_t *uart) {
	unsigned char was;

	(void)uart;
	MP_INTERRUPT_SAFE_BEGIN
	was = _tx_int;
	_tx_int = 0;
	_pump();
	_tx_int = was;
	MP_INTERRUPT_SAFE_END
}

unsigned char mp_usb_rx(mp_uart_t *uart) {
	unsigned char data = 0;

	(void)uart;
	MP_INTERRUPT_SAFE_BEGIN
	if(_rx_head != _rx_tail) {
		data = _rxq[_rx_tail];
		_rx_tail = (unsigned char)((_rx_tail + 1) & 127);
	}
	MP_INTERRUPT_SAFE_END
	return(data);
}

char mp_usb_is_busy(mp_uart_t *uart) {
	(void)uart;
	return(_tx_mask || _tx_count ? 1 : 0);
}

/**@}*/
