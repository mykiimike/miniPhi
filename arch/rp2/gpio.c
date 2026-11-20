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
@defgroup mpArchRP2350GPIO GPIO

@ingroup mpArchRP2350

One bank, pa0 to pa29. SIO owns the pin in GPIO mode. Function select is
1 SPI, 2 UART, 3 I2C, 4 PWM, 5 SIO. Every pin shares IO_IRQ_BANK0.

@{
*/

static mp_gpio_port_t __ports[RP2350_GPIO_COUNT];
static int __irqUsers = 0;

static void _mp_gpio_dispatch(void *user);

static void _mp_gpio_pads(mp_gpio_port_t *port, uint32_t bits) {
	PADS_BANK0_GPIO(port->pin) = (PADS_SCHMITT | PADS_DRIVE_4 | bits) & ~PADS_ISO;
}

static void _mp_gpio_func(mp_gpio_port_t *port, uint32_t func) {
	IO_BANK0_GPIO_CTRL(port->pin) = func & IO_GPIO_FUNCSEL_MASK;
}

static int _mp_gpio_irq_on(mp_gpio_port_t *port) {
	unsigned int shift = (port->pin & 7) * 4;

	return((IO_BANK0_PROC0_INTE(port->pin >> 3) & (0xFUL << shift)) != 0);
}

void mp_gpio_init() {
	unsigned int pin;
	unsigned int spins = 1000000;

	RESETS_RESET &= ~(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
	while(spins-- && (RESETS_RESET_DONE & (RESETS_IO_BANK0 | RESETS_PADS_BANK0)) != (RESETS_IO_BANK0 | RESETS_PADS_BANK0));

	memset(__ports, 0, sizeof(__ports));
	__irqUsers = 0;

	for(pin = 0; pin < RP2350_GPIO_COUNT; pin++) {
		__ports[pin].port = 0;
		__ports[pin].pin = pin;
		/* A pin already driving (the smoke panel, lit before the kernel)
		 * keeps its level. Cutting it here turns the glass off, and a
		 * fault later in boot never gets the chance to light it again. */
		if(SIO_GPIO_OE & (1UL << pin))
			continue;
		mp_gpio_mode(&__ports[pin], MP_GPIO_MODE_ANALOG);
	}
}

void mp_gpio_fini() {
	unsigned int pin;

	for(pin = 0; pin < RP2350_GPIO_COUNT; pin++)
		if(__ports[pin].used == YES)
			mp_gpio_release(&__ports[pin]);
}

mp_gpio_port_t *mp_gpio_handle(unsigned int port, unsigned int slot, char *who) {
	mp_gpio_port_t *gpio;

	if(port != 0 || slot >= RP2350_GPIO_COUNT)
		return(NULL);

	gpio = &__ports[slot];
	if(gpio->used == YES) {
		mp_printk("GPIO pa%u is used by %s, %s can not have it", slot, gpio->who, who);
		return(NULL);
	}

	gpio->used = YES;
	gpio->who = who;
	gpio->reverse = NO;
	gpio->callback = NULL;
	gpio->user = NULL;
	gpio->af = 0;
	mp_gpio_mode(gpio, MP_GPIO_MODE_INPUT);
	return(gpio);
}

mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who) {
	unsigned int port;
	unsigned int pin;
	char *p = text;

	if(!p || (*p != 'p' && *p != 'P'))
		return(NULL);
	p++;

	if(*p >= 'a' && *p <= 'z')
		port = *p - 'a';
	else if(*p >= 'A' && *p <= 'Z')
		port = *p - 'A';
	else
		return(NULL);
	p++;

	if(*p == '.')
		p++;

	if(*p < '0' || *p > '9')
		return(NULL);

	pin = 0;
	while(*p >= '0' && *p <= '9')
		pin = pin * 10 + (*p++ - '0');

	if(*p != '\0')
		return(NULL);

	return(mp_gpio_handle(port, pin, who));
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	if(!port || port->used == NO)
		return(FALSE);

	if(_mp_gpio_irq_on(port))
		mp_gpio_interrupt_unset(port);

	mp_gpio_mode(port, MP_GPIO_MODE_ANALOG);
	port->used = NO;
	port->who = NULL;
	port->reverse = NO;
	port->direction = 0;
	port->af = 0;
	return(TRUE);
}

void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode) {
	uint32_t bit;

	if(!port)
		return;

	bit = 1UL << port->pin;
	switch(mode) {
		case MP_GPIO_MODE_ANALOG:
			SIO_GPIO_OE_CLR = bit;
			_mp_gpio_func(port, IO_GPIO_FUNCSEL_NULL);
			_mp_gpio_pads(port, 0);
			port->direction = 0;
			break;
		case MP_GPIO_MODE_INPUT:
			SIO_GPIO_OE_CLR = bit;
			_mp_gpio_func(port, IO_GPIO_FUNCSEL_SIO);
			_mp_gpio_pads(port, PADS_IE);
			port->direction = MP_GPIO_INPUT;
			break;
		case MP_GPIO_MODE_INPUT_PULL:
			SIO_GPIO_OE_CLR = bit;
			_mp_gpio_func(port, IO_GPIO_FUNCSEL_SIO);
			_mp_gpio_pads(port, PADS_IE | PADS_PUE);
			port->direction = MP_GPIO_INPUT;
			break;
		case MP_GPIO_MODE_OUTPUT:
			_mp_gpio_func(port, IO_GPIO_FUNCSEL_SIO);
			_mp_gpio_pads(port, 0);
			SIO_GPIO_OE_SET = bit;
			port->direction = MP_GPIO_OUTPUT;
			break;
		case MP_GPIO_MODE_OUTPUT_OD:
			_mp_gpio_func(port, IO_GPIO_FUNCSEL_SIO);
			_mp_gpio_pads(port, PADS_IE | PADS_PUE | PADS_OD);
			SIO_GPIO_OE_SET = bit;
			port->direction = MP_GPIO_OUTPUT;
			break;
		case MP_GPIO_MODE_AF:
			SIO_GPIO_OE_CLR = bit;
			_mp_gpio_func(port, port->af ? port->af : IO_GPIO_FUNCSEL_SIO);
			_mp_gpio_pads(port, PADS_IE);
			port->direction = 0;
			break;
		case MP_GPIO_MODE_AF_OD:
			SIO_GPIO_OE_CLR = bit;
			_mp_gpio_func(port, port->af ? port->af : IO_GPIO_FUNCSEL_SIO);
			_mp_gpio_pads(port, PADS_IE | PADS_PUE | PADS_OD);
			port->direction = 0;
			break;
	}
}

void mp_gpio_af(mp_gpio_port_t *port, unsigned int af) {
	port->af = af;
	IO_BANK0_GPIO_CTRL(port->pin) = (IO_BANK0_GPIO_CTRL(port->pin) & ~IO_GPIO_FUNCSEL_MASK) | (af & IO_GPIO_FUNCSEL_MASK);
}

void mp_gpio_pull_up(mp_gpio_port_t *port) {
	PADS_BANK0_GPIO(port->pin) |= PADS_PUE;
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	mp_bool_t high = (SIO_GPIO_IN & (1UL << port->pin)) ? YES : NO;

	if(port->reverse == YES)
		high = high == YES ? NO : YES;
	return(high);
}

void mp_gpio_set(mp_gpio_port_t *port) {
	if(port->reverse == YES)
		SIO_GPIO_OUT_CLR = 1UL << port->pin;
	else
		SIO_GPIO_OUT_SET = 1UL << port->pin;
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port->reverse == YES)
		SIO_GPIO_OUT_SET = 1UL << port->pin;
	else
		SIO_GPIO_OUT_CLR = 1UL << port->pin;
}

void mp_gpio_turn(mp_gpio_port_t *port) {
	SIO_GPIO_OUT_XOR = 1UL << port->pin;
}

mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction) {
	if(direction == MP_GPIO_OUTPUT)
		mp_gpio_mode(port, MP_GPIO_MODE_OUTPUT);
	else
		mp_gpio_mode(port, MP_GPIO_MODE_INPUT);
	return(TRUE);
}

static void _mp_gpio_dispatch(void *user) {
	unsigned int reg;

	(void)user;
	for(reg = 0; reg < 4; reg++) {
		uint32_t pending = IO_BANK0_PROC0_INTS(reg);
		unsigned int base = reg * 8;

		IO_BANK0_INTR(reg) = pending;
		while(pending) {
			unsigned int shift = (unsigned int)__builtin_ctz(pending) & ~3U;
			unsigned int pin = base + shift / 4;

			pending &= ~(0xFUL << shift);
			if(pin < RP2350_GPIO_COUNT && __ports[pin].callback)
				__ports[pin].callback(__ports[pin].user);
		}
	}
}

mp_ret_t mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who) {
	unsigned int shift;
	unsigned int reg;

	if(!port || port->pin >= RP2350_GPIO_COUNT)
		return(FALSE);

	shift = (port->pin & 7) * 4;
	reg = port->pin >> 3;

	if(!_mp_gpio_irq_on(port)) {
		if(__irqUsers == 0 && !mp_interrupt_set(IO_IRQ_BANK0, _mp_gpio_dispatch, NULL, "GPIO"))
			return(FALSE);
		__irqUsers++;
	}

	port->callback = in;
	port->user = user;
	(void)who;
	IO_BANK0_INTR(reg) = 0xFUL << shift;
	mp_gpio_interrupt_enable(port);
	return(TRUE);
}

mp_ret_t mp_gpio_interrupt_unset(mp_gpio_port_t *port) {
	unsigned int shift;
	unsigned int reg;

	if(!port || !_mp_gpio_irq_on(port))
		return(FALSE);

	shift = (port->pin & 7) * 4;
	reg = port->pin >> 3;
	IO_BANK0_PROC0_INTE(reg) &= ~(0xFUL << shift);
	IO_BANK0_INTR(reg) = 0xFUL << shift;
	port->callback = NULL;
	port->user = NULL;

	if(__irqUsers > 0)
		__irqUsers--;
	if(__irqUsers == 0)
		mp_interrupt_unset(IO_IRQ_BANK0);
	return(TRUE);
}

mp_bool_t mp_gpio_wake_arm(mp_gpio_port_t *port) {
	if(port->callback)
		return(NO);
	if(mp_gpio_interrupt_set(port, NULL, NULL, "Wake") == FALSE)
		return(NO);
	mp_gpio_interrupt_hi2lo(port);
	return(YES);
}

void mp_gpio_wake_disarm(mp_gpio_port_t *port) {
	mp_gpio_interrupt_unset(port);
}

/**@}*/
