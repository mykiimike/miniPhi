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

#ifndef _HAVE_STM32_UART_H
	#define _HAVE_STM32_UART_H

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

		USART_TypeDef *regs;
	};

	mp_ret_t mp_uart_init();
	mp_ret_t mp_uart_fini();

	mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who);
	mp_ret_t mp_uart_setup(mp_uart_t *uart, mp_options_t *options);
	mp_ret_t mp_uart_close(mp_uart_t *uart);

	static inline char mp_uart_isBusy(mp_uart_t *uart) {
		return(uart->regs->SR & USART_SR_TC ? 0 : 1);
	}

	static inline char mp_uart_hasOverrun(mp_uart_t *uart) {
		return(uart->regs->SR & (USART_SR_ORE | USART_SR_FE | USART_SR_PE) ? 1 : 0);
	}

	/* no flow control lines */
	static inline void mp_uart_enable_rx(mp_uart_t *uart) {
	}

	static inline void mp_uart_disable_rx(mp_uart_t *uart) {
	}

	static inline void mp_uart_enable_tx_int(mp_uart_t *uart) {
		uart->regs->CR1 |= USART_CR1_TXEIE;
	}

	static inline void mp_uart_disable_tx_int(mp_uart_t *uart) {
		uart->regs->CR1 &= ~USART_CR1_TXEIE;
	}

	static inline void mp_uart_enable_rx_int(mp_uart_t *uart) {
		uart->regs->CR1 |= USART_CR1_RXNEIE;
	}

	static inline void mp_uart_disable_rx_int(mp_uart_t *uart) {
		uart->regs->CR1 &= ~USART_CR1_RXNEIE;
	}

	/* waits for room, then queues the byte: never blocks inside the TXE interrupt */
	static inline void mp_uart_tx(mp_uart_t *uart, unsigned char data) {
		while (!(uart->regs->SR & USART_SR_TXE));
		uart->regs->DR = data;
	}

	static inline unsigned char mp_uart_rx(mp_uart_t *uart) {
		while (!(uart->regs->SR & USART_SR_RXNE));
		return(uart->regs->DR);
	}

#endif
