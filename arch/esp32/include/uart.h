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

#ifndef _HAVE_ESP32_UART_H
	#define _HAVE_ESP32_UART_H

	typedef struct mp_uart_s mp_uart_t;

	typedef void (*mp_uart_on_t)(mp_uart_t *uart);

	struct mp_uart_s {
		mp_kernel_t *kernel;

		unsigned long baudRate;

		mp_uart_on_t onWrite;
		mp_uart_on_t onRead;

		void *user;

		mp_gate_t *gate;
	};

	mp_ret_t mp_uart_init(void);
	mp_ret_t mp_uart_fini(void);

	mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who);
	mp_ret_t mp_uart_setup(mp_uart_t *uart, mp_options_t *options);
	mp_ret_t mp_uart_close(mp_uart_t *uart);
	void mp_uart_retime(void);
	mp_bool_t mp_uart_quiet(void);
	void mp_uart_wake(mp_bool_t arm);

	void mp_uart_enable_tx_int(mp_uart_t *uart);
	void mp_uart_disable_tx_int(mp_uart_t *uart);
	void mp_uart_enable_rx_int(mp_uart_t *uart);
	void mp_uart_disable_rx_int(mp_uart_t *uart);
	void mp_uart_tx(mp_uart_t *uart, unsigned char data);
	unsigned char mp_uart_rx(mp_uart_t *uart);

	static inline char mp_uart_isBusy(mp_uart_t *uart) {
		(void)uart;
		return(c6_read(C6_USB + 0x4UL) & (1UL << 1) ? 0 : 1);
	}

	static inline char mp_uart_hasOverrun(mp_uart_t *uart) {
		(void)uart;
		return(0);
	}

	static inline void mp_uart_enable_rx(mp_uart_t *uart) {
		(void)uart;
	}

	static inline void mp_uart_disable_rx(mp_uart_t *uart) {
		(void)uart;
	}

#endif
