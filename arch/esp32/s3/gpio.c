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
@defgroup mpArchESP32S3GPIO GPIO

@ingroup mpArchESP32

Port A, pin number is the GPIO. Function 1 is the GPIO matrix, and the
matrix output select resets to GPIO_OUT, so a pin needs only the mux,
the output enable and GPIO_OUT. Pins 32 and above use the second bank.

@{
*/

/* INT_TYPE: 1 rising, 2 falling, 3 both */
#define _EDGE_RISE  1
#define _EDGE_FALL  2
#define _EDGE_BOTH  3

/* IO_MUX offset from S3_IOMUX. 0 means the pad does not exist. */
static const unsigned short _mux_off[S3_PIN_COUNT] = {
	0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 0x20,
	0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40,
	0x44, 0x48, 0x4c, 0x50, 0x54, 0x58,
	0, 0, 0, 0,
	0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84,
	0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c,
	0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc,
	0xc0, 0xc4,
};

static mp_gpio_port_t _pins[S3_PIN_COUNT];
static mp_bool_t _gpio_irq;

static unsigned long _bit(int pin) {
	return(1UL << (pin & 31));
}

static unsigned long _bank(int pin, unsigned long low, unsigned long high) {
	return(S3_GPIO + (pin < 32 ? low : high));
}

static int _blocked(int pin) {
	if(pin < 0 || pin >= S3_PIN_COUNT || _mux_off[pin] == 0)
		return(1);
	if(pin == 19 || pin == 20)
		return(1);
	if(pin >= 26 && pin <= 37)
		return(1);
	if(pin == 45 || pin == 46)
		return(1);
	return(0);
}

static int _parse(char *text) {
	int pin = 0;
	int digits = 0;

	if(!text)
		return(-1);
	if(text[0] == 'p' || text[0] == 'P')
		text++;
	if(text[0] != 'a' && text[0] != 'A')
		return(-1);
	text++;
	if(text[0] == '.')
		text++;
	while(text[0] >= '0' && text[0] <= '9') {
		pin = pin * 10 + (text[0] - '0');
		text++;
		digits++;
		if(pin >= S3_PIN_COUNT)
			return(-1);
	}
	if(!digits || text[0])
		return(-1);
	return(pin);
}

static unsigned long _pin_reg(int pin) {
	return(S3_GPIO + 0x74UL + (unsigned long)pin * 4UL);
}

/* MCU_SEL = 1, input enabled, drive strength 2. Keeps a pull-up when asked. */
static void _mux(int pin, int pullup) {
	unsigned long value = (1UL << 12) | (1UL << 9) | (2UL << 10);

	if(pullup)
		value |= 1UL << 8;
	s3_write(S3_IOMUX + _mux_off[pin], value);
}

static void _drive(int pin, int high) {
	s3_write(_bank(pin, 0x08UL, 0x14UL), high ? _bit(pin) : 0);
	if(!high)
		s3_write(_bank(pin, 0x0CUL, 0x18UL), _bit(pin));
}

static void _edge(mp_gpio_port_t *port, int type) {
	unsigned long addr = _pin_reg((int)port->pin);
	unsigned long value = s3_read(addr);

	value &= ~(7UL << 7);
	value |= ((unsigned long)type << 7);
	value |= (1UL << 13);
	s3_write(addr, value);
	s3_write(_bank((int)port->pin, 0x4CUL, 0x58UL), _bit((int)port->pin));
}

static void _gpio_isr(void *user) {
	unsigned long low;
	unsigned long high;
	int pin;

	(void)user;
	low = s3_read(S3_GPIO + 0x44UL);
	high = s3_read(S3_GPIO + 0x50UL);
	s3_write(S3_GPIO + 0x4CUL, low);
	s3_write(S3_GPIO + 0x58UL, high);
	for(pin = 0; pin < 32; pin++) {
		if((low & (1UL << pin)) && _pins[pin].callback)
			_pins[pin].callback(_pins[pin].user);
	}
	for(pin = 32; pin < S3_PIN_COUNT; pin++) {
		if((high & (1UL << (pin - 32))) && _pins[pin].callback)
			_pins[pin].callback(_pins[pin].user);
	}
}

void mp_gpio_init(void) {
	int pin;

	_gpio_irq = NO;
	for(pin = 0; pin < S3_PIN_COUNT; pin++) {
		_pins[pin].port = 0;
		_pins[pin].pin = (unsigned int)pin;
		_pins[pin].who = NULL;
		_pins[pin].used = NO;
		_pins[pin].direction = 0;
		_pins[pin].callback = NULL;
		_pins[pin].user = NULL;
		_pins[pin].reverse = NO;
	}
}

void mp_gpio_fini(void) {
	int pin;

	for(pin = 0; pin < S3_PIN_COUNT; pin++) {
		if(_pins[pin].used == YES)
			mp_gpio_release(&_pins[pin]);
	}
}

mp_gpio_port_t *mp_gpio_handle(unsigned int port, unsigned int slot, char *who) {
	if(port != 0 || _blocked((int)slot)) {
		mp_printk("GPIO pa%u is not available", slot);
		return(NULL);
	}
	if(_pins[slot].used == YES) {
		mp_printk("GPIO pa%u is already used by %s", slot, _pins[slot].who ? _pins[slot].who : "?");
		return(NULL);
	}
	_pins[slot].used = YES;
	_pins[slot].who = who;
	_pins[slot].reverse = NO;
	_pins[slot].callback = NULL;
	return(&_pins[slot]);
}

mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who) {
	int pin = _parse(text);

	if(pin < 0) {
		mp_printk("GPIO %s is not a pin", text ? text : "(null)");
		return(NULL);
	}
	return(mp_gpio_handle(0, (unsigned int)pin, who));
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	if(!port || port->used == NO)
		return(FALSE);
	mp_gpio_interrupt_unset(port);
	s3_write(_bank((int)port->pin, 0x28UL, 0x34UL), _bit((int)port->pin));
	port->used = NO;
	port->who = NULL;
	port->callback = NULL;
	port->reverse = NO;
	return(TRUE);
}

mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction) {
	if(!port)
		return(FALSE);
	if(direction == MP_GPIO_OUTPUT) {
		_mux((int)port->pin, 0);
		_drive((int)port->pin, port->reverse == YES ? 1 : 0);
		s3_write(_bank((int)port->pin, 0x24UL, 0x30UL), _bit((int)port->pin));
	}
	else {
		s3_write(_bank((int)port->pin, 0x28UL, 0x34UL), _bit((int)port->pin));
		_mux((int)port->pin, 0);
	}
	port->direction = (char)direction;
	return(TRUE);
}

void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode) {
	unsigned long addr;
	unsigned long value;

	if(!port)
		return;
	if(mode == MP_GPIO_MODE_OUTPUT || mode == MP_GPIO_MODE_OUTPUT_OD)
		mp_gpio_direction(port, MP_GPIO_OUTPUT);
	else {
		mp_gpio_direction(port, MP_GPIO_INPUT);
		if(mode == MP_GPIO_MODE_INPUT_PULL)
			mp_gpio_pull_up(port);
	}
	addr = _pin_reg((int)port->pin);
	value = s3_read(addr);
	if(mode == MP_GPIO_MODE_OUTPUT_OD)
		value |= 1UL << 2;
	else
		value &= ~(1UL << 2);
	s3_write(addr, value);
}

void mp_gpio_af(mp_gpio_port_t *port, unsigned int af) {
	(void)port;
	(void)af;
}

void mp_gpio_pull_up(mp_gpio_port_t *port) {
	unsigned long addr;

	if(!port)
		return;
	addr = S3_IOMUX + _mux_off[port->pin];
	s3_set(addr, 1UL << 8);
	s3_clr(addr, 1UL << 7);
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	if(!port)
		return(OFF);
	return(s3_read(_bank((int)port->pin, 0x3CUL, 0x40UL)) & _bit((int)port->pin) ? ON : OFF);
}

void mp_gpio_set(mp_gpio_port_t *port) {
	if(port)
		_drive((int)port->pin, port->reverse == YES ? 0 : 1);
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port)
		_drive((int)port->pin, port->reverse == YES ? 1 : 0);
}

void mp_gpio_turn(mp_gpio_port_t *port) {
	unsigned long out;

	if(!port)
		return;
	out = s3_read(_bank((int)port->pin, 0x04UL, 0x10UL));
	_drive((int)port->pin, out & _bit((int)port->pin) ? 0 : 1);
}

mp_ret_t mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who) {
	if(!port || !in)
		return(FALSE);
	if(_gpio_irq == NO) {
		if(!mp_interrupt_set(S3_SRC_GPIO, _gpio_isr, NULL, "GPIO"))
			return(FALSE);
		_gpio_irq = YES;
	}
	port->callback = in;
	port->user = user;
	(void)who;
	_edge(port, _EDGE_BOTH);
	return(TRUE);
}

mp_ret_t mp_gpio_interrupt_unset(mp_gpio_port_t *port) {
	unsigned long addr;
	unsigned long value;

	if(!port)
		return(FALSE);
	addr = _pin_reg((int)port->pin);
	value = s3_read(addr);
	value &= ~(7UL << 7);
	value &= ~(0x1FUL << 13);
	s3_write(addr, value);
	s3_write(_bank((int)port->pin, 0x4CUL, 0x58UL), _bit((int)port->pin));
	port->callback = NULL;
	port->user = NULL;
	return(TRUE);
}

mp_bool_t mp_gpio_wake_arm(mp_gpio_port_t *port) {
	(void)port;
	return(FALSE);
}

void mp_gpio_wake_disarm(mp_gpio_port_t *port) {
	(void)port;
}

void mp_gpio_interrupt_enable(mp_gpio_port_t *port) {
	if(port)
		s3_set(_pin_reg((int)port->pin), 1UL << 13);
}

void mp_gpio_interrupt_disable(mp_gpio_port_t *port) {
	if(port)
		s3_clr(_pin_reg((int)port->pin), 0x1FUL << 13);
}

void mp_gpio_interrupt_lo2hi(mp_gpio_port_t *port) {
	if(port)
		_edge(port, _EDGE_RISE);
}

void mp_gpio_interrupt_hi2lo(mp_gpio_port_t *port) {
	if(port)
		_edge(port, _EDGE_FALL);
}

void mp_gpio_interrupt_hilo_switch(mp_gpio_port_t *port) {
	unsigned long value;
	int type;

	if(!port)
		return;
	value = s3_read(_pin_reg((int)port->pin));
	type = (int)((value >> 7) & 7UL);
	if(type == _EDGE_RISE)
		_edge(port, _EDGE_FALL);
	else
		_edge(port, _EDGE_RISE);
}

/**@}*/
