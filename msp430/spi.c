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

static void mp_spi_interruptDispatch(void *user);

void mp_spi_init() {
	mp_list_init(&__spi);
	__spi_count = 0;
}

void mp_spi_fini() {
/* should be ok */
}

mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_t *spi, mp_options_t *options, char *who) {
	char *value;

	/* get gate Id*/
	value = mp_options_get(options, "gate");
	if(!value)
		return(FALSE);
	spi->gate = mp_gate_handle(value, "SPI");
	if(spi->gate == NULL) {
		mp_printk("SPI - No gate specify (USCI_B) for %s", who);
		return(FALSE);
	}

	/* somi */
	value = mp_options_get(options, "somi");
	if(!value) {
		mp_printk("SPI - No SOMI port for %s", who);
		mp_spi_close(spi);
		return(FALSE);
	}
	spi->somi = mp_gpio_text_handle(value, "SPI SOMI");
    if(!spi->somi) {
    	mp_printk("SPI - Can not handle GPIO SOMI for %s using %s", who, value);
		mp_spi_close(spi);
		return(FALSE);
	}

	/* simo */
	value = mp_options_get(options, "simo");
	if(!value) {
		mp_printk("SPI - No SIMO port for %s", who);
		mp_spi_close(spi);
		return(FALSE);
	}
	spi->simo = mp_gpio_text_handle(value, "SPI SIMO");
    if(!spi->simo) {
    	mp_printk("SPI - Can not handle GPIO SIMO for %s using %s", who, value);
		mp_spi_close(spi);
		return(FALSE);
	}

	/* clk */
	value = mp_options_get(options, "clk");
	if(!value) {
		mp_printk("SPI - No CLK port for %s", who);
		mp_spi_close(spi);
		return(FALSE);
	}
	spi->clk = mp_gpio_text_handle(value, "SPI CLK");
    if(!spi->clk) {
    	mp_printk("SPI - Can not handle GPIO CLK for %s using %s", who, value);
		mp_spi_close(spi);
		return(FALSE);
	}

	return(TRUE);
}

mp_ret_t mp_spi_setup(mp_spi_t *spi, mp_options_t *options) {
	unsigned long frequency;
	unsigned int prescaler;
	char *value;

	/* frequency */
	value = mp_options_get(options, "frequency");
	if(!value) {
		mp_printk("SPI - No frequency specify");
		mp_spi_close(spi);
		return(FALSE);
	}
	frequency = atol(value);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_BEGIN

	/* set port alternate function and port direction */
	_GPIO_REG8(spi->simo, _GPIO_SEL) |= 1<<spi->simo->pin;
	_GPIO_REG8(spi->somi, _GPIO_SEL) |= 1<<spi->somi->pin;
	_GPIO_REG8(spi->clk, _GPIO_SEL) |= 1<<spi->clk->pin;

	_GPIO_REG8(spi->simo, _GPIO_DIR) |= 1<<spi->simo->pin;
	_GPIO_REG8(spi->somi, _GPIO_DIR) &= ~(1<<spi->somi->pin);
	_GPIO_REG8(spi->clk, _GPIO_DIR) |= 1<<spi->clk->pin;

	/* open for write */
	_SPI_REG8(spi->gate, _SPI_CTL1) |= UCSWRST;

	/* clock source - SMCLK */
	_SPI_REG8(spi->gate, _SPI_CTL1) |= UCSSEL_2;

	prescaler = mp_clock_get_speed() / frequency;
	_SPI_REG8(spi->gate, _SPI_BR0) = prescaler % 256;
	_SPI_REG8(spi->gate, _SPI_BR1) = prescaler / 256;

	/* user options */
	value = mp_options_get(options, "phase");
	if(value && mp_options_cmp(value, "change") == TRUE)
		_SPI_REG8(spi->gate, _SPI_CTL0) &= ~(UCCKPH);
	else
		_SPI_REG8(spi->gate, _SPI_CTL0) |= UCCKPH;

	value = mp_options_get(options, "polarity");
	if(value && mp_options_cmp(value, "low") == TRUE)
		_SPI_REG8(spi->gate, _SPI_CTL0) &= ~(UCCKPL);
	else
		_SPI_REG8(spi->gate, _SPI_CTL0) |= UCCKPL;

	value = mp_options_get(options, "first");
	if(value && mp_options_cmp(value, "lsb") == TRUE)
		_SPI_REG8(spi->gate, _SPI_CTL0) &= ~(UCMSB);
	else
		_SPI_REG8(spi->gate, _SPI_CTL0) |= UCMSB;

	value = mp_options_get(options, "role");
	if(value && mp_options_cmp(value, "slave") == TRUE)
		_SPI_REG8(spi->gate, _SPI_CTL0) &= ~(UCMST);
	else
		_SPI_REG8(spi->gate, _SPI_CTL0) |= UCMST;

	value = mp_options_get(options, "bit");
	if(value && mp_options_cmp(value, "8") == TRUE)
		_SPI_REG8(spi->gate, _SPI_CTL0) &= ~(UC7BIT);
	else
		_SPI_REG8(spi->gate, _SPI_CTL0) |= UC7BIT;

	_SPI_REG8(spi->gate, _SPI_CTL0) |= UCSYNC;

	/* write finished */
	_SPI_REG8(spi->gate, _SPI_CTL1) &= ~(UCSWRST);

	/* disable interrupts */
	mp_spi_disable_tx(spi);
	mp_spi_disable_rx(spi);

	/* place interrupt */
	mp_interrupt_set(spi->gate->_ISRVector, mp_spi_interruptDispatch, spi, spi->gate->portDevice);
	spi->ie = _SPI_REG8(spi->gate, _SPI_IE);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_END

	/* initialize SPI */
	mp_clock_delay(100);

	/* list */
	mp_list_add_last(&__spi, &spi->item, spi);
	__spi_count++;
	return(TRUE);

}

mp_ret_t mp_spi_close(mp_spi_t *spi) {

	/* kill the task */
	if(spi->task)
		mp_task_destroy(spi->task);

	/* disble interrupts */
	if(spi->gate) {
		mp_spi_disable_rx(spi);
		mp_spi_disable_tx(spi);
		mp_interrupt_unset(spi->gate->_ISRVector);
	}

	if(spi->simo) {
		_GPIO_REG8(spi->simo, _GPIO_SEL) &= ~(1<<spi->simo->pin);
		mp_gpio_release(spi->simo);
	}

	if(spi->somi) {
		_GPIO_REG8(spi->somi, _GPIO_SEL) &= ~(1<<spi->somi->pin);
		mp_gpio_release(spi->somi);
	}

	if(spi->clk) {
		_GPIO_REG8(spi->clk, _GPIO_SEL) &= ~(1<<spi->clk->pin);
		mp_gpio_release(spi->clk);
	}

	if(spi->gate)
		mp_gate_release(spi->gate);

	mp_list_remove(&__spi, &spi->item);
	__spi_count--;
	return(TRUE);
}

unsigned char mp_spi_rx(mp_spi_t *spi) {
	return(_SPI_REG8(spi->gate, _SPI_RXBUF));
}

void mp_spi_tx(mp_spi_t *spi, unsigned char data) {
	_SPI_REG8(spi->gate, _SPI_TXBUF) = data;
}

mp_spi_flag_t mp_spi_flags_get(mp_spi_t *spi) {
	return((mp_spi_flag_t)_SPI_REG8(spi->gate, _SPI_IFG));
}

void mp_spi_flags_set(mp_spi_t *spi, unsigned short data) {
	_SPI_REG8(spi->gate, _SPI_IFG) = data;
}

static void mp_spi_interruptDispatch(void *user) {
	mp_spi_t *spi = user;
	mp_spi_iv_t iv = (mp_spi_iv_t)_SPI_REG16(spi->gate, _SPI_IV);
	if(spi->intDispatch)
		spi->intDispatch(spi, iv);
}

#ifdef __UNUSED_FOR_THE_MOMENT
static void __mp_spi_interrupt(void *user) {
	mp_spi_t *spi = user;

	switch(_SPI_REG8(spi->gate, _SPI_IFG)) {
		case UCRXIFG:
			/* interrupt hook */
			if(spi->onReadInterrupt) {
				spi->onReadInterrupt(spi);
				break;
			}

			spi->rx_buffer[spi->rx_size] = mp_spi_rx(spi);
			spi->rx_size++;
			spi->task->signal = MP_TASK_SIG_OK; /* ASR */
			break;

		case UCTXIFG:
			/* interrupt hook */
			if(spi->onWriteInterrupt) {
				spi->onWriteInterrupt(spi);
				break;
			}

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
			mp_spi_tx(spi, spi->tx_buffer[spi->tx_pos]);
			spi->tx_pos++;

			break;
	}

}


static void __mp_spi_asr(mp_task_t *task) {
	mp_spi_t *spi = task->user;
	unsigned char save;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* disable interrupt */
	mp_spi_disable_rx(spi);
	mp_spi_disable_tx(spi);

	spi->task->signal = MP_TASK_SIG_SLEEP;

	/* - - - - - RX - - - - - */

	/* manage callbacks */
	if(spi->rx_size%2 == 0) {
		spi->onRead(spi);
		spi->rx_size = 0;
	}
	else {
		save = spi->rx_buffer[spi->rx_size]; /* save rest */
		spi->rx_size--; /* uncomplet SPI frame */
		spi->onRead(spi);
		spi->rx_buffer[0] = save;
		spi->rx_size = 1;
	}

	/* - - - - - TX - - - - - */

	/* write ended */
	if(spi->onWriteEnd && spi->tx_reference > 0) {
		spi->onWriteEnd(spi);
		spi->tx_reference = 0;
	}

	/* enable interrupt */
	mp_spi_enable_rx(spi);
	mp_spi_enable_tx(spi);
}

#endif
