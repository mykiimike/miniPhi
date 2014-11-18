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

#include <mp.h>

static void _mp_uart_interrupt(void *user);


/**
 * @brief Global uart initialization
 * @return TRUE or FALSE
 */
mp_ret_t mp_uart_init() {

	return(TRUE);
}

mp_ret_t mp_uart_fini() {

	return(TRUE);
}

mp_ret_t mp_uart_open(mp_uart_t *uart, char *who) {
	float division_factor;
	unsigned int brf;
	unsigned long freq;

	/* get gate Id*/
	uart->_gate = mp_gate_handle(uart->gateId, "UART");
	if(uart->_gate == NULL)
		return(FALSE);

	uart->_gate->gateFlags = uart->gateFlags;

	/* allocate GPIO port for RX data */
	uart->_rxd_port = mp_gpio_handle(uart->rxd.port, uart->rxd.pin, "UART RX");
	if(uart->_rxd_port == NULL) {
		mp_gate_release(uart->_gate);
		mp_uart_close(uart);
		return(FALSE);
	}
	/* allocate GPIO port for TX data */
	uart->_txd_port = mp_gpio_handle(uart->txd.port, uart->txd.pin, "UART TX");
	if(uart->_txd_port == NULL) {
		mp_gate_release(uart->_gate);
		mp_uart_close(uart);
		return(FALSE);
	}

	/* configure ports */
	_GPIO_REG8(uart->_rxd_port, _GPIO_SEL) |= 1<<uart->rxd.pin;
	_GPIO_REG8(uart->_txd_port, _GPIO_SEL) |= 1<<uart->txd.pin;
	mp_gpio_direction(uart->_rxd_port, MP_GPIO_INPUT);
	mp_gpio_direction(uart->_txd_port, MP_GPIO_OUTPUT);

	/* disable interrupts */
	__disable_interrupt();

	_UART_REG8(uart->_gate, _UART_CTL1) |= UCSWRST; //Reset State
	_UART_REG8(uart->_gate, _UART_CTL0) = 0;
	if(uart->gateFlags & MP_UART_PARITY)
		_UART_REG8(uart->_gate, _UART_CTL0) |= UCPEN;
	else
		_UART_REG8(uart->_gate, _UART_CTL0) &= ~UCPEN;

	if(uart->gateFlags & MP_UART_PAR_EVEN)
		_UART_REG8(uart->_gate, _UART_CTL0) |= UCPAR;
	else
		_UART_REG8(uart->_gate, _UART_CTL0) &= ~UCPAR;

	if(uart->gateFlags & MP_UART_7BITS)
		_UART_REG8(uart->_gate, _UART_CTL0) |= UC7BIT;
	else
		_UART_REG8(uart->_gate, _UART_CTL0) &= ~UC7BIT;

	if(uart->gateFlags & MP_UART_TWO_S)
		_UART_REG8(uart->_gate, _UART_CTL0) |= UCSPB;
	else
		_UART_REG8(uart->_gate, _UART_CTL0) &= ~UCSPB;

	if(uart->gateFlags & MP_UART_SYNC)
		_UART_REG8(uart->_gate, _UART_CTL0) |= UCSYNC;
	else
		_UART_REG8(uart->_gate, _UART_CTL0) &= ~UCSYNC;

	/* uart mode */
	_UART_REG8(uart->_gate, _UART_CTL0) |= UCMODE_0;

	/* clear status register */
	_UART_REG8(uart->_gate, _UART_STATW) = 0;

	/* clearn interrupt flags */
	//_UART_REG8(uart->_gate, _UART_IFG) = 0;

	/* place interrupt */
	mp_interrupt_set(uart->_gate->_ISRVector, _mp_uart_interrupt, uart, uart->_gate->portDevice);

	/* Use ACLK for Baud rates less than 9600 to allow us to still */
	/* receive characters while in LPM3. */
	if(uart->baudRate <= 9600) {
		_UART_REG8(uart->_gate, _UART_CTL1) = _UART_UCSSEL_ACLK_mask;
		freq = ACLK_FREQ_HZ;
	}
	else {
		_UART_REG8(uart->_gate, _UART_CTL1) = _UART_UCSSEL_SMCLK_mask;
		freq = mp_clock_get_speed();
	}

	division_factor = FLOAT_DIV(freq, uart->baudRate);

	/* Set up the modulation stages and oversampling mode. */
	_UART_REG8(uart->_gate, _UART_MCTL) = 0;
	if((division_factor >= 16) && (uart->baudRate < 921600L)) {
		/* we will use oversampling mode and formulas as in sect 19.3.10.2*/
		/* of MSP430x5xx Family Users Guide */
		_UART_REG16(uart->_gate, _UART_BRW) = (unsigned int) (division_factor / 16);

		/* forumla for BRF specifies rounding up which is why 0.5 is added*/
		/* before casting to int since C integer casts truncate */
		brf = ((unsigned int)((((FLOAT_DIV(division_factor, 16))-((unsigned int)(FLOAT_DIV(division_factor, 16))))*16)+0.5));

		/* set the correct BRF, then set BRS to 0 (may need this later), */
		/* then enable oversampling mode */
		_UART_REG8(uart->_gate, _UART_MCTL) = (((brf << _UART_MCTL_BRF_bit) & _UART_MCTL_BRF_MASK) | _UART_MCTL_UCOS16_mask);
	}
	else {
		/* we will use oversampling mode and formulas as in sect 19.3.10.1*/
		/* of MSP430x5xx Family Users Guide section 19.3.10.1 specifies */
		/* setting UCBRS and clearing UCOS16 bit. */
		_UART_REG16(uart->_gate, _UART_BRW) = (unsigned int)division_factor;
		brf = ((unsigned int)(((division_factor - ((unsigned int)division_factor))*8) + 0.5));

		/* Set the proper BRS field */
		_UART_REG8(uart->_gate, _UART_MCTL) = ((brf << _UART_MCTL_BRS_bit) & _UART_MCTL_BRS_MASK);
		_UART_REG8(uart->_gate, _UART_MCTL) &= (~(_UART_MCTL_UCOS16_mask));
	}

	/* lock write */
	_UART_REG8(uart->_gate, _UART_CTL1) &= ~(UCDORM);
	_UART_REG8(uart->_gate, _UART_CTL1) &= ~UCSWRST; // lock write

	__enable_interrupt();

	_UART_REG8(uart->_gate, _UART_IE) |= UCRXIE;

	return(TRUE);
}

mp_ret_t mp_uart_close(mp_uart_t *uart) {

	/* remove interrupt */
	mp_interrupt_unset(uart->_gate->_ISRVector);

	/* clean gpio */
	if(uart->_rxd_port != NULL)
		mp_gpio_release(uart->_rxd_port);

	if(uart->_txd_port != NULL)
		mp_gpio_release(uart->_txd_port);

	_GPIO_REG8(uart->_rxd_port, _GPIO_SEL) &= ~(1<<uart->rxd.pin);
	_GPIO_REG8(uart->_txd_port, _GPIO_SEL) &= ~(1<<uart->txd.pin);

	mp_gate_release(uart->_gate);

	return(TRUE);
}





// int used to indicate a request for more new data
/*
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){
	UCA2IE &= ~UCRXIE ; // disable RX interrupts
	rx_buffer_ptr = buffer;
	bytes_to_read = len;
	UCA2IE |= UCRXIE; // enable RX interrupts
	hal_uart_dma_enable_rx(); // enable receive
}

*/

static void _mp_uart_interrupt(void *user) {
	mp_uart_t *uart = user;

	P10OUT ^= 0x40;

}

