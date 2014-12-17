/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

#ifndef _HAVE_MSP430_UART_H
	#define _HAVE_MSP430_UART_H

	#define MP_UART_PARITY    1    /* default disabled */
	#define MP_UART_PAR_EVEN  1<<1 /* default odd */
	#define MP_UART_MSB       1<<2 /* default lsb */
	#define MP_UART_7BITS     1<<3 /* default 8bits */
	#define MP_UART_TWO_S     1<<4 /* default one */
	#define MP_UART_SYNC      1<<5 /* default async */

	typedef struct mp_uart_regs_s mp_uart_regs_t;

	typedef struct mp_uart_s mp_uart_t;

	typedef void (*mp_uart_on_t)(mp_uart_t *uart);

	struct mp_uart_s {
		/** Kernel handler */
		mp_kernel_t *kernel;

		/** Baud rate */
		unsigned int baudRate;

		mp_uart_on_t onWrite;
		mp_uart_on_t onRead;
		void *user;

		/** internal: UART gate */
		mp_gate_t *gate;

		/** internal: rxd_port */
		mp_gpio_port_t *rxd_port;

		/** internal: txd_port */
		mp_gpio_port_t *txd_port;




	};

	mp_ret_t mp_uart_init();
	mp_ret_t mp_uart_fini();
	mp_ret_t mp_uart_open(mp_kernel_t *kernel, mp_uart_t *uart, mp_options_t *options, char *who);
	mp_ret_t mp_uart_close(mp_uart_t *uart);

	/* machine specs */
	#define _UART_CTLW0   0x00
	#define _UART_CTL0    0x01
	#define _UART_CTL1    0x00
	#define _UART_CTLW1   0x02
	#define _UART_BRW     0x06
	#define _UART_BR0     0x06
	#define _UART_BR1     0x07
	#define _UART_MCTL    0x08
	#define _UART_STATW   0x0a
	#define _UART_RXBUF   0x0c
	#define _UART_TXBUF   0x0e
	#define _UART_ABCTL   0x10
	#define _UART_IE      0x1c /* wrong in documentation */
	#define _UART_IFG     0x1d
	#define _UART_IV      0x1e

	#define _UART_UCSSEL_bit          (6)
	#define _UART_UCSSEL_SMCLK_bits   (0x3)
	#define _UART_UCSSEL_ACLK_bits    (0x1)
	#define _UART_UCSSEL_UCA2CLK_bits (0x0)
	#define _UART_UCSSEL_SMCLK_mask   (_UART_UCSSEL_SMCLK_bits << _UART_UCSSEL_bit)
	#define _UART_UCSSEL_ACLK_mask    (_UART_UCSSEL_ACLK_bits << _UART_UCSSEL_bit)
	#define _UART_UCSSEL_UCA2CLK_mask (_UART_UCSSEL_UCA2CLK_bits << _UART_UCSSEL_bit)

	#define _UART_CTL1_RXIE (BIT5)
	#define _UART_CTL1_BRKIE (BIT4)
	#define _UART_CTL1_DORM (BIT3)
	#define _UART_CTL1_TXADDR (BIT2)
	#define _UART_CTL1_TXBRK (BIT1)
	#define _UART_CTL1_SWRST (BIT0)

	#define _UART_MCTL_BRF_bit (4)
	#define _UART_MCTL_BRF_MASK (0xF << _UART_MCTL_BRF_bit)
	#define _UART_MCTL_BRS_bit (1)
	#define _UART_MCTL_BRS_MASK (0x7 << _UART_MCTL_BRS_bit)
	#define _UART_MCTL_UCOS16_mask (BIT0)

	#define _UART_REG8(_port, _type) \
		*((volatile char *)(_port->_baseAddress+_type))

	#define _UART_REG16(_port, _type) \
		*((volatile short *)(_port->_baseAddress+_type))


	static inline void mp_uart_enable_rx(mp_uart_t *uart) {
		// P1OUT &= ~BIT4; // = 0 - RTS low -> ok
	}

	static inline void mp_uart_disable_rx(mp_uart_t *uart) {
		// P1OUT |= BIT4; // = 1 - RTS high -> stop
	}

	static inline void mp_uart_enable_tx_int(mp_uart_t *uart) {
		/* check CTS */
		//_UART_REG8(uart->gate, _UART_CTL0) |= UCTXIE;
	}

	static inline void mp_uart_disable_tx_int(mp_uart_t *uart) {
		//_UART_REG8(uart->gate, _UART_CTL0) &= ~UCTXIE;
	}

	static inline void mp_uart_tx(mp_uart_t *uart, unsigned char data) {
		while (!(_UART_REG8(uart->gate, _UART_IFG) & UCTXIFG));
		_UART_REG8(uart->gate, _UART_TXBUF) = data;
	}

	static inline unsigned char mp_uart_rx(mp_uart_t *uart) {
		while (!(_UART_REG8(uart->gate, _UART_IFG) & UCRXIFG));
		return(_UART_REG8(uart->gate, _UART_RXBUF));
	}

#endif
