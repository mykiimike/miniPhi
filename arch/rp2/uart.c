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
@defgroup mpArchRP2350UART UART

@ingroup mpArchRP2350

PL011. Options of mp_uart_open(): gate (UART0, UART1), txd and rxd (the
gate defaults when missing), rts and cts (any pins). Gate USB is the
CDC console in usb.c: no pins, baud is only reported to the host.
Options of mp_uart_setup(): baudRate, parity (none, even, odd),
stopBits (1, 2). The line is 8 bits. clk_peri is the baud clock.

RTS and CTS are GPIO, active low. CTS high stops the TX interrupt.

@{
*/

static void _mp_uart_interrupt(void *user);
static void _mp_uart_cts(void *user);
static void _mp_uart_divisors(mp_uart_t *uart);

static mp_list_t __uarts;

static void _mp_uart_dummy_read(mp_uart_t *uart) {
	(void)uart->regs->DR;
}

static void _mp_uart_dummy_write(mp_uart_t *uart) {
	mp_uart_disable_tx_int(uart);
}

mp_ret_t mp_uart_init() {
	mp_list_init(&__uarts);
	return(TRUE);
}

mp_ret_t mp_uart_fini() {
	return(TRUE);
}

mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who) {
	char *value;

	value = mp_options_get(options, "gate");
	if(value && mp_options_cmp(value, "USB") == TRUE)
		return(mp_usb_uart_open(kernel, uart, options, who));

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

	mp_gpio_af(uart->txd_port, IO_GPIO_FUNCSEL_UART);
	mp_gpio_af(uart->rxd_port, IO_GPIO_FUNCSEL_UART);
	mp_gpio_mode(uart->txd_port, MP_GPIO_MODE_AF);
	mp_gpio_mode(uart->rxd_port, MP_GPIO_MODE_AF);
	mp_gpio_pull_up(uart->rxd_port);

	value = mp_options_get(options, "rts");
	if(value) {
		uart->rts_port = mp_gpio_text_handle(value, who);
		if(!uart->rts_port) {
			mp_printk("UART %s: RTS pin %s is not available", uart->gate->portDevice, value);
			mp_uart_close(uart);
			return(FALSE);
		}
		mp_gpio_mode(uart->rts_port, MP_GPIO_MODE_OUTPUT);
		mp_uart_enable_rx(uart);
	}

	value = mp_options_get(options, "cts");
	if(value) {
		uart->cts_port = mp_gpio_text_handle(value, who);
		if(!uart->cts_port) {
			mp_printk("UART %s: CTS pin %s is not available", uart->gate->portDevice, value);
			mp_uart_close(uart);
			return(FALSE);
		}
		mp_gpio_mode(uart->cts_port, MP_GPIO_MODE_INPUT_PULL);
		mp_gpio_pull_up(uart->cts_port);
		if(mp_gpio_interrupt_set(uart->cts_port, _mp_uart_cts, uart, who) == FALSE) {
			mp_uart_close(uart);
			return(FALSE);
		}
		mp_gpio_interrupt_hi2lo(uart->cts_port);
	}

	if(mp_uart_setup(uart, options) == FALSE) {
		mp_uart_close(uart);
		return(FALSE);
	}
	return(TRUE);
}

static void _mp_uart_divisors(mp_uart_t *uart) {
	unsigned long clk = mp_gate_pclk(uart->gate);
	unsigned long baud = uart->baudRate;
	uint32_t div;
	uint32_t ibrd;
	uint32_t fbrd;

	div = (uint32_t)((8UL * clk + baud / 2) / baud);
	ibrd = div >> 7;
	fbrd = ((div & 0x7f) + 1) / 2;
	if(fbrd >= 64) {
		ibrd++;
		fbrd = 0;
	}
	uart->regs->IBRD = ibrd;
	uart->regs->FBRD = fbrd;
}

mp_ret_t mp_uart_setup(mp_uart_t *uart, mp_options_t *options) {
	UART_TypeDef *regs = uart->regs;

	if(uart->usb == YES)
		return(mp_usb_uart_setup(uart, options));
	unsigned long pclk = mp_gate_pclk(uart->gate);
	uint32_t lcr = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;
	char *value;

	value = mp_options_get(options, "baudRate");
	uart->baudRate = value ? strtoul(value, NULL, 10) : 115200;
	if(uart->baudRate == 0 || uart->baudRate > pclk / 16) {
		mp_printk("UART %s: baud rate %lu is out of range", uart->gate->portDevice, uart->baudRate);
		return(FALSE);
	}

	value = mp_options_get(options, "parity");
	if(value && mp_options_cmp(value, "even") == TRUE)
		lcr |= UART_LCR_H_PEN | UART_LCR_H_EPS;
	else if(value && mp_options_cmp(value, "odd") == TRUE)
		lcr |= UART_LCR_H_PEN;

	value = mp_options_get(options, "stopBits");
	if(value && mp_options_cmp(value, "2") == TRUE)
		lcr |= UART_LCR_H_STP2;

	MP_INTERRUPT_SAFE_BEGIN

	if(uart->listed == NO) {
		uart->item.next = uart->item.prev = NULL;
		mp_list_add_last(&__uarts, &uart->item, uart);
		uart->listed = YES;
	}

	regs->CR = 0;
	while(regs->FR & UART_FR_BUSY);
	regs->IFLS = 0;
	_mp_uart_divisors(uart);
	regs->LCR_H = lcr;
	regs->ICR = UART_ICR_RXIC | UART_ICR_TXIC | UART_ICR_RTIC | UART_ICR_OEIC;
	regs->CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;

	mp_interrupt_set(uart->gate->irq, _mp_uart_interrupt, uart, uart->gate->portDevice);

	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

mp_ret_t mp_uart_close(mp_uart_t *uart) {
	if(uart->usb == YES)
		return(mp_usb_uart_close(uart));

	if(uart->gate) {
		while(uart->regs->CR & UART_CR_TXE && mp_uart_isBusy(uart));
		mp_interrupt_unset(uart->gate->irq);
		uart->regs->CR = 0;
	}

	if(uart->listed == YES) {
		MP_INTERRUPT_SAFE_BEGIN
		mp_list_remove(&__uarts, &uart->item);
		uart->listed = NO;
		MP_INTERRUPT_SAFE_END
	}

	if(uart->txd_port) {
		mp_gpio_release(uart->txd_port);
		uart->txd_port = NULL;
	}
	if(uart->rxd_port) {
		mp_gpio_release(uart->rxd_port);
		uart->rxd_port = NULL;
	}
	if(uart->rts_port) {
		mp_gpio_release(uart->rts_port);
		uart->rts_port = NULL;
	}
	if(uart->cts_port) {
		mp_gpio_interrupt_unset(uart->cts_port);
		mp_gpio_release(uart->cts_port);
		uart->cts_port = NULL;
	}

	if(uart->gate) {
		mp_gate_release(uart->gate);
		uart->gate = NULL;
	}
	return(TRUE);
}

void mp_uart_retime(void) {
	mp_list_item_t *item;

	for(item = __uarts.first; item; item = item->next) {
		mp_uart_t *uart = item->user;
		UART_TypeDef *regs = uart->regs;
		uint32_t cr = regs->CR;
		uint32_t lcr = regs->LCR_H;

		regs->CR = 0;
		while(regs->FR & UART_FR_BUSY);
		_mp_uart_divisors(uart);
		regs->LCR_H = lcr;
		regs->CR = cr;
	}
}

mp_bool_t mp_uart_quiet(void) {
	mp_list_item_t *item;

	if(mp_usb_quiet() == NO)
		return(NO);

	for(item = __uarts.first; item; item = item->next) {
		mp_uart_t *uart = item->user;

		if(!(uart->regs->CR & UART_CR_TXE))
			continue;
		if((uart->regs->IMSC & UART_IMSC_TXIM) || !(uart->regs->FR & UART_FR_TXFE) || (uart->regs->FR & UART_FR_BUSY))
			return(NO);
	}
	return(YES);
}

void mp_uart_wake(mp_bool_t arm) {
	mp_list_item_t *item;

	for(item = __uarts.first; item; item = item->next) {
		mp_uart_t *uart = item->user;

		if(!uart->rxd_port || !(uart->regs->CR & UART_CR_RXE))
			continue;
		if(arm == YES)
			uart->wakeArmed = mp_gpio_wake_arm(uart->rxd_port);
		else if(uart->wakeArmed == YES) {
			mp_gpio_wake_disarm(uart->rxd_port);
			uart->wakeArmed = NO;
		}
	}
}

static void _mp_uart_interrupt(void *user) {
	mp_uart_t *uart = user;
	UART_TypeDef *regs = uart->regs;
	uint32_t mis = regs->MIS;

	if((mis & (UART_IMSC_RXIM | UART_IMSC_RTIM)) && !(regs->FR & UART_FR_RXFE))
		(uart->onRead ? uart->onRead : _mp_uart_dummy_read)(uart);
	regs->ICR = UART_ICR_RXIC | UART_ICR_RTIC | UART_ICR_OEIC;

	if(mis & UART_IMSC_TXIM) {
		if(uart->cts_port && mp_gpio_read(uart->cts_port) == YES) {
			mp_uart_disable_tx_int(uart);
			uart->ctsWait = YES;
			mp_gpio_interrupt_enable(uart->cts_port);
			if(mp_gpio_read(uart->cts_port) == NO)
				_mp_uart_cts(uart);
		}
		else
			(uart->onWrite ? uart->onWrite : _mp_uart_dummy_write)(uart);
		regs->ICR = UART_ICR_TXIC;
	}
}

static void _mp_uart_cts(void *user) {
	mp_uart_t *uart = user;

	if(uart->ctsWait == NO)
		return;
	uart->ctsWait = NO;
	mp_gpio_interrupt_disable(uart->cts_port);
	mp_uart_enable_tx_int(uart);
}

/**@}*/
