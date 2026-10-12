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
@defgroup mpArchSTM32UART USART

@ingroup mpArchSTM32

Options of mp_uart_open(): gate (USART1, USART2), txd and rxd (the
remap 0 pins when missing). Options of mp_uart_setup(): baudRate,
parity (none, even, odd), stopBits (1, 2), bits (8, 9).

@{
*/

static void _mp_uart_interrupt(void *user);

/* without a reader or a writer, the flags still have to drop or the line fires forever */
static void _mp_uart_dummy_read(mp_uart_t *uart) {
	(void)uart->regs->DR;
}

static void _mp_uart_dummy_write(mp_uart_t *uart) {
	mp_uart_disable_tx_int(uart);
}

mp_ret_t mp_uart_init() {
	return(TRUE);
}

mp_ret_t mp_uart_fini() {
	return(TRUE);
}

mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who) {
	char *value;

	memset(uart, 0, sizeof(*uart));
	uart->kernel = kernel;
	uart->onRead = _mp_uart_dummy_read;
	uart->onWrite = _mp_uart_dummy_write;

	value = mp_options_get(options, "gate");
	uart->gate = mp_gate_handle(value, who);
	if(!uart->gate)
		return(FALSE);
	uart->regs = uart->gate->base;

	value = mp_options_get(options, "txd");
	uart->txd_port = mp_gpio_text_handle(value ? value : (char *)uart->gate->pin0, who);
	if(!uart->txd_port) {
		mp_printk("UART %s: TX pin %s is not available", uart->gate->portDevice, value ? value : uart->gate->pin0);
		mp_uart_close(uart);
		return(FALSE);
	}

	value = mp_options_get(options, "rxd");
	uart->rxd_port = mp_gpio_text_handle(value ? value : (char *)uart->gate->pin1, who);
	if(!uart->rxd_port) {
		mp_printk("UART %s: RX pin %s is not available", uart->gate->portDevice, value ? value : uart->gate->pin1);
		mp_uart_close(uart);
		return(FALSE);
	}

	mp_gpio_mode(uart->txd_port, MP_GPIO_MODE_AF);
	mp_gpio_mode(uart->rxd_port, MP_GPIO_MODE_INPUT_PULL);
	uart->rxd_port->base->BSRR = 1UL << uart->rxd_port->pin;

	return(mp_uart_setup(uart, options));
}

mp_ret_t mp_uart_setup(mp_uart_t *uart, mp_options_t *options) {
	USART_TypeDef *regs = uart->regs;
	unsigned long pclk = mp_gate_pclk(uart->gate);
	uint32_t cr1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
	uint32_t cr2 = 0;
	char *value;

	value = mp_options_get(options, "baudRate");
	uart->baudRate = value ? strtoul(value, NULL, 10) : 115200;
	if(uart->baudRate == 0 || uart->baudRate > pclk / 16) {
		mp_printk("UART %s: baud rate %lu is out of range", uart->gate->portDevice, uart->baudRate);
		return(FALSE);
	}

	value = mp_options_get(options, "parity");
	if(value && mp_options_cmp(value, "even") == TRUE)
		cr1 |= USART_CR1_PCE | USART_CR1_M;
	else if(value && mp_options_cmp(value, "odd") == TRUE)
		cr1 |= USART_CR1_PCE | USART_CR1_PS | USART_CR1_M;

	value = mp_options_get(options, "bits");
	if(value && mp_options_cmp(value, "9") == TRUE)
		cr1 |= USART_CR1_M;

	value = mp_options_get(options, "stopBits");
	if(value && mp_options_cmp(value, "2") == TRUE)
		cr2 |= USART_CR2_STOP2;

	MP_INTERRUPT_SAFE_BEGIN

	regs->CR1 = 0;
	regs->CR2 = cr2;
	regs->CR3 = 0;
	/* 16x oversampling: mantissa and fraction together are pclk / baud, rounded */
	regs->BRR = (pclk + uart->baudRate / 2) / uart->baudRate;
	regs->CR1 = cr1;

	/* drop what came in before */
	(void)regs->SR;
	(void)regs->DR;

	mp_interrupt_set(uart->gate->irq, _mp_uart_interrupt, uart, uart->gate->portDevice);

	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

mp_ret_t mp_uart_close(mp_uart_t *uart) {
	if(uart->gate) {
		/* let the last byte leave */
		while(uart->regs->CR1 & USART_CR1_TE && mp_uart_isBusy(uart));

		mp_interrupt_unset(uart->gate->irq);
		uart->regs->CR1 = 0;
	}

	if(uart->txd_port) {
		mp_gpio_release(uart->txd_port);
		uart->txd_port = NULL;
	}

	if(uart->rxd_port) {
		mp_gpio_release(uart->rxd_port);
		uart->rxd_port = NULL;
	}

	if(uart->gate) {
		mp_gate_release(uart->gate);
		uart->gate = NULL;
	}

	return(TRUE);
}

static void _mp_uart_interrupt(void *user) {
	mp_uart_t *uart = user;
	USART_TypeDef *regs = uart->regs;
	uint32_t sr = regs->SR;
	uint32_t cr1 = regs->CR1;

	if(cr1 & USART_CR1_RXNEIE) {
		if(sr & USART_SR_RXNE)
			(uart->onRead ? uart->onRead : _mp_uart_dummy_read)(uart);
		else if(sr & USART_SR_ORE) {
			/* SR then DR clears the overrun */
			(void)regs->DR;
		}
	}

	if((sr & USART_SR_TXE) && (cr1 & USART_CR1_TXEIE))
		(uart->onWrite ? uart->onWrite : _mp_uart_dummy_write)(uart);
}

/**@}*/
