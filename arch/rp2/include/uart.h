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

#ifndef _HAVE_RP2350_UART_H
	#define _HAVE_RP2350_UART_H

	typedef struct mp_uart_s mp_uart_t;

	typedef void (*mp_uart_on_t)(mp_uart_t *uart);

	struct mp_uart_s {
		mp_kernel_t *kernel;

		unsigned long baudRate;

		mp_uart_on_t onWrite;
		mp_uart_on_t onRead;

		void *user;

		mp_gate_t *gate;
		mp_gpio_port_t *rxd_port;
		mp_gpio_port_t *txd_port;

		mp_gpio_port_t *rts_port;
		mp_gpio_port_t *cts_port;

		volatile mp_bool_t ctsWait;

		UART_TypeDef *regs;

		mp_list_item_t item;
		mp_bool_t listed;
		mp_bool_t wakeArmed;

		/** YES when this port is the USB CDC gate. The helpers below call usb.c. */
		mp_bool_t usb;
	};

	mp_ret_t mp_uart_init();
	mp_ret_t mp_uart_fini();

	mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who);
	mp_ret_t mp_uart_setup(mp_uart_t *uart, mp_options_t *options);
	mp_ret_t mp_uart_close(mp_uart_t *uart);
	void mp_uart_retime(void);
	mp_bool_t mp_uart_quiet(void);
	void mp_uart_wake(mp_bool_t arm);

	#include "usb.h"

	static inline char mp_uart_isBusy(mp_uart_t *uart) {
		if(uart->usb == YES)
			return(mp_usb_is_busy(uart));
		return(uart->regs->FR & UART_FR_BUSY ? 1 : 0);
	}

	static inline char mp_uart_hasOverrun(mp_uart_t *uart) {
		if(uart->usb == YES)
			return(0);
		return(uart->regs->RSR & UART_RSR_OE ? 1 : 0);
	}

	static inline void mp_uart_enable_rx(mp_uart_t *uart) {
		if(uart->rts_port)
			mp_gpio_unset(uart->rts_port);
	}

	static inline void mp_uart_disable_rx(mp_uart_t *uart) {
		if(uart->rts_port)
			mp_gpio_set(uart->rts_port);
	}

	static inline void mp_uart_enable_tx_int(mp_uart_t *uart) {
		if(uart->usb == YES) {
			mp_usb_enable_tx_int(uart);
			return;
		}
		uart->regs->IMSC |= UART_IMSC_TXIM;
	}

	static inline void mp_uart_disable_tx_int(mp_uart_t *uart) {
		if(uart->usb == YES) {
			mp_usb_disable_tx_int(uart);
			return;
		}
		uart->regs->IMSC &= ~UART_IMSC_TXIM;
	}

	static inline void mp_uart_enable_rx_int(mp_uart_t *uart) {
		if(uart->usb == YES) {
			mp_usb_enable_rx_int(uart);
			return;
		}
		uart->regs->IMSC |= UART_IMSC_RXIM | UART_IMSC_RTIM;
	}

	static inline void mp_uart_disable_rx_int(mp_uart_t *uart) {
		if(uart->usb == YES) {
			mp_usb_disable_rx_int(uart);
			return;
		}
		uart->regs->IMSC &= ~(UART_IMSC_RXIM | UART_IMSC_RTIM);
	}

	/* waits for a free FIFO slot, then queues the byte */
	static inline void mp_uart_tx(mp_uart_t *uart, unsigned char data) {
		if(uart->usb == YES) {
			mp_usb_tx(uart, data);
			return;
		}
		while(uart->regs->FR & UART_FR_TXFF);
		uart->regs->DR = data;
	}

	static inline void mp_uart_rx_poll(mp_uart_t *uart) {
		(void)uart;
	}

	static inline unsigned char mp_uart_rx(mp_uart_t *uart) {
		if(uart->usb == YES)
			return(mp_usb_rx(uart));
		while(uart->regs->FR & UART_FR_RXFE);
		return((unsigned char)uart->regs->DR);
	}

	static inline mp_bool_t mp_uart_sync(mp_uart_t *uart) {
		(void)uart;
		return(NO);
	}

	static inline void mp_uart_write_sync(const unsigned char *data, int size) {
		(void)data;
		(void)size;
	}

	static inline mp_bool_t mp_uart_tx_ok(void) {
		return(YES);
	}

	static inline mp_bool_t mp_uart_host_back(void) {
		return(NO);
	}

	/* 0: this port writes one byte at a time. Negative: a bulk port with no room. */
	static inline int mp_uart_tx_room(mp_uart_t *uart) {
		int room;

		if(uart->usb != YES)
			return(0);
		room = mp_usb_tx_room(uart);
		if(room > 0)
			return(room);
		return(-1);
	}

	static inline void mp_uart_write(mp_uart_t *uart, const unsigned char *data, int n) {
		if(uart->usb == YES)
			mp_usb_write(uart, data, n);
	}

#endif
