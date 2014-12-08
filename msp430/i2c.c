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

#include "mp.h"

static void __mp_i2c_intMRX(void *user);
static void __mp_i2c_intMTX(void *user);
static void __mp_i2c_intSRX(void *user);
static void __mp_i2c_intSTX(void *user);


/* internal pointers */
static mp_list_t __i2c;
static unsigned int __i2c_count;

/**
 * @brief Global I2C initialization
 * @return TRUE or FALSE
 */
mp_ret_t mp_i2c_init() {

	return(TRUE);
}

mp_ret_t mp_i2c_fini() {





}


mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_t *i2c, mp_options_t *options, char *who) {
	char *value;

	/* get gate Id*/
	value = mp_options_get(options, "gate");
	if(!value)
		return(FALSE);
	i2c->gate = mp_gate_handle(value, "I2C");
	if(i2c->gate == NULL) {
		mp_printk("I2C - No gate specify (USCI_B) for %s", who);
		return(FALSE);
	}

	/* sda */
	value = mp_options_get(options, "sda");
	if(!value) {
		mp_printk("I2C - No SDA port for %s", who);
		mp_i2c_close(i2c);
		return(FALSE);
	}
	i2c->sda = mp_gpio_text_handle(value, "I2C SDA");
    if(!i2c->sda) {
    	mp_printk("I2C - Can not handle GPIO SDA for %s using %s", who, value);
		mp_i2c_close(i2c);
		return(FALSE);
	}

	/* clk */
	value = mp_options_get(options, "clk");
	if(!value) {
		mp_printk("I2C - No CLK port for %s", who);
		mp_i2c_close(i2c);
		return(FALSE);
	}
	i2c->clk = mp_gpio_text_handle(value, "I2C CLK");
    if(!i2c->clk) {
    	mp_printk("I2C - Can not handle GPIO CLK for %s using %s", who, value);
		mp_i2c_close(i2c);
		return(FALSE);
	}
}

mp_ret_t mp_i2c_setup(mp_i2c_t *i2c, mp_options_t *options) {
	unsigned long frequency;
	unsigned int prescaler;
	char *value;

	/* frequency */
	value = mp_options_get(options, "frequency");
	if(!value) {
		mp_printk("I2C - No frequency specify");
		mp_i2c_close(i2c);
		return(FALSE);
	}
	frequency = atol(value);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_BEGIN

	/* set port alternate function and port direction */
	_GPIO_REG8(i2c->sda, _GPIO_SEL) |= 1<<i2c->sda->pin;
	_GPIO_REG8(i2c->clk, _GPIO_SEL) |= 1<<i2c->clk->pin;

	/* open for write */
	_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCSWRST;

	/* clock source - SMCLK */
	_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCSSEL_2;

	prescaler = mp_clock_get_speed() / frequency;
	_I2C_REG8(i2c->gate, _I2C_BR0) = prescaler % 256;
	_I2C_REG8(i2c->gate, _I2C_BR1) = prescaler / 256;

	/* user options */
	value = mp_options_get(options, "role");
	if(value && mp_options_cmp(value, "slave") == TRUE)
		_I2C_REG8(i2c->gate, _I2C_CTL0) &= ~(UCMST);
	else
		_I2C_REG8(i2c->gate, _I2C_CTL0) |= UCMST;


	_I2C_REG8(i2c->gate, _I2C_CTL0) |= UCSYNC + UCMODE_3;

	/* write finished */
	_I2C_REG8(i2c->gate, _I2C_CTL1) &= ~(UCSWRST);

	/* disable interrupts */
	mp_i2c_disable_tx(i2c);
	mp_i2c_disable_rx(i2c);

	/* place interrupt */
	//i2c->inte = mp_interrupt_set(i2c->gate->_ISRVector, __mp_i2c_intMRX, i2c, i2c->gate->portDevice);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_END

	/* initialize I2C */
	mp_clock_delay(100);

	/* list */
	mp_list_add_last(&__i2c, &i2c->item, i2c);
	__i2c_count++;
	return(TRUE);

}

mp_ret_t mp_i2c_close(mp_i2c_t *i2c) {
	/* kill the task */
	//if(i2c->task)
		//mp_task_destroy(i2c->task);

	/* disable interrupts */
	/*
	if(i2c->inte) {
		mp_i2c_disable_rx(i2c);
		mp_i2c_disable_tx(i2c);

		mp_interrupt_unset(i2c->gate->_ISRVector);
	}
	*/

	if(i2c->sda) {
		_GPIO_REG8(i2c->sda, _GPIO_SEL) &= ~(1<<i2c->sda->pin);
		mp_gpio_release(i2c->sda);
	}

	if(i2c->clk) {
		_GPIO_REG8(i2c->clk, _GPIO_SEL) &= ~(1<<i2c->clk->pin);
		mp_gpio_release(i2c->clk);
	}

	if(i2c->gate)
		mp_gate_release(i2c->gate);

	mp_list_remove(&__i2c, &i2c->item);
	__i2c_count--;

	return(TRUE);
}


mp_i2c_setAddress(mp_i2c_t *i2c, char address) {
	_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCSWRST;

	if(_I2C_REG8(i2c->gate, _I2C_CTL0) & UCMST)
		_I2C_REG8(i2c->gate, _I2C_OA) = address;
	else
		_I2C_REG8(i2c->gate, _I2C_SA) = address;

	_I2C_REG8(i2c->gate, _I2C_CTL1) &= ~(UCSWRST);
}

mp_i2_read(mp_i2c_t *i2c, unsigned char address, int len) {

	/* receiver mode */
	_I2C_REG8(i2c->gate, _I2C_CTL1) &= ~(UCTR);

	/* start receiver */
	_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCTXSTT;

	// enable interrupt
	if(_I2C_REG8(i2c->gate, _I2C_CTL0) & UCMST) {
		_I2C_REG8(i2c->gate, _I2C_SA) = address;
		i2c->inte->callback = __mp_i2c_intMRX;
	}
	else {
		_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCSWRST;

		_I2C_REG8(i2c->gate, _I2C_OA) = address;
		i2c->inte->callback= __mp_i2c_intSRX;

		_I2C_REG8(i2c->gate, _I2C_CTL1) &= ~(UCSWRST);
	}

	i2c->rxBufferIndex = 0;
	i2c->rxBufferLength = len;

	/* operation terminated ??? */


}

mp_i2_write(mp_i2c_t *i2c, char address, char *data, int len) {
	/* TX mode */
	_I2C_REG8(i2c->gate, _I2C_CTL1) &= UCTR;

	/* start receiver */
	_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCTXSTT;

	// enable interrupt
	if(_I2C_REG8(i2c->gate, _I2C_CTL0) & UCMST) {
		_I2C_REG8(i2c->gate, _I2C_SA) = address;
		i2c->inte->callback = __mp_i2c_intMTX;
	}
	else if(_I2C_REG8(i2c->gate, _I2C_OA) != address) {
		_I2C_REG8(i2c->gate, _I2C_CTL1) |= UCSWRST;

		_I2C_REG8(i2c->gate, _I2C_OA) = address;
		i2c->inte->callback = __mp_i2c_intSTX;

		_I2C_REG8(i2c->gate, _I2C_CTL1) &= ~(UCSWRST);
	}

	i2c->txBufferIndex = 0;
	i2c->txBufferLength = len;
}


static void __mp_i2c_intMRX(void *user) {
	mp_spi_t *i2c = user;

	switch(_SPI_REG8(i2c->gate, _SPI_IFG)) {
		case UCSTPIFG:
			break;

		case UCRXIFG:
			break;
	}

}

static void __mp_i2c_intMTX(void *user) {
	mp_spi_t *i2c = user;

	switch(_SPI_REG8(i2c->gate, _SPI_IFG)) {
		case UCNACKIFG:
			break;

		case UCSTPIFG:
			break;

		case UCSTTIFG:
			break;

		case UCTXIFG:
			break;

		case UCRXIFG:
			break;
	}

}

static void __mp_i2c_intSRX(void *user) {
	mp_spi_t *i2c = user;

	switch(_SPI_REG8(i2c->gate, _SPI_IFG)) {
		case UCNACKIFG:
			break;

		case UCSTPIFG:
			break;

		case UCSTTIFG:
			break;

		case UCTXIFG:
			break;

		case UCRXIFG:
			break;
	}

}

static void __mp_i2c_intSTX(void *user) {
	mp_spi_t *i2c = user;

	switch(_SPI_REG8(i2c->gate, _SPI_IFG)) {
		case UCNACKIFG:
			break;

		case UCSTPIFG:
			break;

		case UCSTTIFG:
			break;

		case UCTXIFG:
			break;

		case UCRXIFG:
			break;
	}

}


