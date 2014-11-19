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

/* internal pointers */
static mp_list_t __spi;
static unsigned int __spi_count;

static unsigned char __mp_spi_rx(mp_spi_t *spi);
static void __mp_spi_tx(mp_spi_t *spi, unsigned char data);
static void __mp_spi_interrupt(void *user);
static void __mp_spi_asr(mp_task_t *task);

void mp_spi_init() {
	mp_list_init(&__spi);
	__spi_count = 0;
}

void mp_spi_fini() {
/* should be ok */
}

mp_ret_t mp_spi_open(
		mp_kernel_t *kernel,
		mp_spi_t *spi, char *who,
		mp_spi_phase_t phase,
		mp_spi_polarity_t polarity,
		mp_spi_first_t first,
		mp_spi_role_t role,
		mp_spi_mode_t mode,
		mp_spi_bit_t bit,
		mp_spi_sync_t sync,
		unsigned long frequency
	) {
	unsigned int prescaler;

	spi->frequency = frequency;
	spi->tx_reference = 0;

	/* get gate Id*/
	spi->_gate = mp_gate_handle(spi->gateId, "SPI");
	if(spi->_gate == NULL)
		return(FALSE);

	/* open gpio ports */
	spi->_simo = mp_gpio_handle(spi->simo.port, spi->simo.pin, "SPI SIMO");
	if(spi->_simo == NULL) {
		mp_gate_release(spi->_gate);
		return(FALSE);
	}

	spi->_somi = mp_gpio_handle(spi->somi.port, spi->somi.pin, "SPI SOMI");
	if(spi->_somi == NULL) {
		mp_gpio_release(spi->_simo);
		mp_gate_release(spi->_gate);
		return(FALSE);
	}

	spi->_clock = mp_gpio_handle(spi->clock.port, spi->clock.pin, "SPI CLOCK");
	if(spi->_clock == NULL) {
		mp_gpio_release(spi->_simo);
		mp_gpio_release(spi->_somi);
		mp_gate_release(spi->_gate);
		return(FALSE);
	}

	if(spi->ste.port > 0) {
		spi->_ste = mp_gpio_handle(spi->ste.port, spi->ste.pin, "SPI STE");
		if(spi->_ste == NULL) {
			mp_gpio_release(spi->_clock);
			mp_gpio_release(spi->_somi);
			mp_gpio_release(spi->_simo);
			mp_gate_release(spi->_gate);
			return(FALSE);
		}
	}

	/* create the task */
	spi->task = mp_task_create(&kernel->tasks, who, __mp_spi_asr, spi, 0);
	if(spi->task == NULL) {
		if(spi->_ste != NULL)
			mp_gpio_release(spi->_ste);
		mp_gpio_release(spi->_clock);
		mp_gpio_release(spi->_somi);
		mp_gpio_release(spi->_simo);
		mp_gate_release(spi->_gate);
		return(FALSE);
	}

	/* set port alternate function and port direction */
	_GPIO_REG8(spi->_simo, _GPIO_SEL) |= 1<<spi->_simo->pin;
	_GPIO_REG8(spi->_somi, _GPIO_SEL) |= 1<<spi->_somi->pin;
	_GPIO_REG8(spi->_clock, _GPIO_SEL) |= 1<<spi->_clock->pin;
	if(spi->_ste) {
		_GPIO_REG8(spi->_ste, _GPIO_SEL) |= 1<<spi->_ste->pin;
		/* set output ste */
		_GPIO_REG8(spi->_ste, _GPIO_DIR) |= 1<<spi->_ste->pin;
	}

	_GPIO_REG8(spi->_simo, _GPIO_DIR) |= 1<<spi->_simo->pin;
	_GPIO_REG8(spi->_somi, _GPIO_DIR) &= ~(1<<spi->_somi->pin);
	_GPIO_REG8(spi->_clock, _GPIO_DIR) |= 1<<spi->_clock->pin;

	/* open for write */
	_SPI_REG8(spi->_gate, _SPI_CTL1) |= UCSWRST;

	/* clock source - SMCLK */
	_SPI_REG8(spi->_gate, _SPI_CTL1) |= 0x80;
	UCB2CTL1 = UCB2CTL1 | 0x80;                     //

	prescaler = mp_clock_get_speed() / frequency;
	_SPI_REG8(spi->_gate, _SPI_BR0) = prescaler % 256;
	_SPI_REG8(spi->_gate, _SPI_BR1) = prescaler / 256;

	/* user options */
	switch(phase) {
		case MP_SPI_CLK_PHASE_CHANGE:
			_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~(UCCKPH);
			break;
		case MP_SPI_CLK_PHASE_CAPTURE:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCCKPH;
			break;
	}

	switch(polarity) {
		case MP_SPI_CLK_POLARITY_LOW:
			_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~(UCCKPL);
			break;
		case MP_SPI_CLK_POLARITY_HIGH:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCCKPL;
			break;
	}

	switch(first) {
		case MP_SPI_LSB:
			_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~(UCMSB);
			break;
		case MP_SPI_MSB:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCMSB;
			break;
	}

	switch(role) {
		case MP_SPI_SLAVE:
			_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~(UCMST);
			break;
		case MP_SPI_MASTER:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCMST;
			break;
	}

	switch(mode) {
		case MP_SPI_MODE_3PIN:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCMODE_0;
			break;
		case MP_SPI_MODE_4PIN_LOW:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCMODE_1;
			break;
		case MP_SPI_MODE_4PIN_HIGH:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCMODE_2;
			break;
	}

	switch(bit) {
		case MP_SPI_BIT_8:
			_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~(UC7BIT);
			break;
		case MP_SPI_BIT_7:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UC7BIT;
			break;
	}

	switch(sync) {
		case MP_SPI_ASYNC:
			_SPI_REG8(spi->_gate, _SPI_CTL0) &= ~(UCSYNC);
			break;
		case MP_SPI_SYNC:
			_SPI_REG8(spi->_gate, _SPI_CTL0) |= UCSYNC;
			break;
	}

	/* write finished */
	_SPI_REG8(spi->_gate, _SPI_CTL1) &= ~(UCSWRST);

	/* place interrupt */
	mp_interrupt_set(spi->_gate->_ISRVector, __mp_spi_interrupt, spi, spi->_gate->portDevice);

	/* list */
	mp_list_add_last(&__spi, &spi->item, spi);
	__spi_count++;
	return(TRUE);
}

mp_ret_t mp_spi_close(mp_spi_t *spi) {

	/* kill the task */
	mp_task_destroy(spi->task);

	/* disble interrupts */
	mp_spi_disable_rx(spi);
	mp_spi_disable_tx(spi);
	mp_interrupt_unset(spi->_gate->_ISRVector);

	_GPIO_REG8(spi->_simo, _GPIO_SEL) &= ~(1<<spi->_simo->pin);
	_GPIO_REG8(spi->_somi, _GPIO_SEL) &= ~(1<<spi->_somi->pin);
	_GPIO_REG8(spi->_clock, _GPIO_SEL) &= ~(1<<spi->_clock->pin);

	if(spi->_ste) {
		_GPIO_REG8(spi->_ste, _GPIO_SEL) &= ~(1<<spi->_ste->pin);
		/* set output ste */
		_GPIO_REG8(spi->_ste, _GPIO_DIR) &= ~(1<<spi->_ste->pin);
	}

	mp_list_remove(&__spi, &spi->item);
	__spi_count--;
	return(TRUE);
}

void mp_spi_write(mp_spi_t *spi, unsigned char *input) {
	int size = 2;
	int rest;
	int a;

	/* disable TX interrupt */
	mp_spi_disable_tx(spi);

	rest = sizeof(spi->tx_buffer)-spi->tx_size;

	/* no more space flush output now */
	if(rest < size) {

		/* flush now */
		for(a=spi->tx_pos; a<spi->tx_size; a++)
			__mp_spi_tx(spi, spi->tx_buffer[a]);
		spi->tx_size = 0;
		spi->tx_pos = 0;

		/* always place */
		memcpy(spi->tx_buffer, input, size);
		spi->tx_size += size;
	}
	/* space available */
	else {
		memcpy(spi->tx_buffer+spi->tx_pos, input, size);
		spi->tx_size += size;
	}

	/* enable TX interrupt */
	mp_spi_enable_tx(spi);
}

static unsigned char __mp_spi_rx(mp_spi_t *spi) {
	if (_SPI_REG8(spi->_gate, _SPI_IFG) & UCRXIFG)
		return(_SPI_REG8(spi->_gate, _SPI_RXBUF));
	return 0;
}

static void __mp_spi_tx(mp_spi_t *spi, unsigned char data) {
	while (!(_SPI_REG8(spi->_gate, _SPI_IFG) & UCRXIFG));
	_SPI_REG8(spi->_gate, _SPI_TXBUF) = data;
}

static void __mp_spi_interrupt(void *user) {
	mp_spi_t *spi = user;

	switch(_SPI_REG8(spi->_gate, _SPI_IFG)) {
		case UCRXIFG:
			spi->rx_buffer[spi->rx_size] = __mp_spi_rx(spi);
			spi->rx_size++;
			spi->task->signal = MP_TASK_SIG_OK; /* ASR */
			break;

		case UCTXIFG:
			/* no more data to send */
			if(spi->tx_size == 0 || spi->tx_pos == spi->tx_size) {
				mp_spi_disable_tx(spi);
				spi->task->signal = MP_TASK_SIG_OK; /* ASR */
				spi->tx_reference++;
				spi->tx_pos = 0;
				spi->tx_size = 0;
				return;
			}

			/* send last char */
			__mp_spi_tx(spi, spi->tx_buffer[spi->tx_pos]);
			spi->tx_pos++;

			break;
	}

}


static void __mp_spi_asr(mp_task_t *task) {
	mp_spi_t *spi = task->user;

	/* disable interrupt */
	mp_spi_disable_rx(spi);
	mp_spi_disable_tx(spi);

	/* - - - - - RX - - - - - */

	/* manage callbacks */
	if(spi->rx_size%2 == 0)
		spi->onRead(spi);
	spi->rx_size = 0;

	/* - - - - - TX - - - - - */

	/* write ended */
	if(spi->onWriteEnd && spi->tx_reference > 0) {
		spi->onWriteEnd(spi);
		spi->tx_reference--;
	}

	spi->task->signal = MP_TASK_SIG_SLEEP;

	/* enable interrupt */
	mp_spi_enable_rx(spi);
	mp_spi_enable_tx(spi);
}
