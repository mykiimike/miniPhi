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
@defgroup mpArchESP32ClassicGPIO GPIO

@ingroup mpArchESP32

Port A, pin number is the GPIO. The IO_MUX address of each pad is not
the pin number. Function 2 is the GPIO matrix. GPIO34 to GPIO39 are
input only and have no pulls. Mux bit 1 is the RTC function; clearing
it selects the digital path. Pins 32 and above use the second bank.
The PRO CPU interrupt enable is bit 2 of the pin int_ena field.

@{
*/

#define _EDGE_RISE  1
#define _EDGE_FALL  2
#define _EDGE_BOTH  3
#define _GPIO_FUNC  2UL
#define _SIG_GPIO   256UL
#define _PRO_INT    (1UL << 15)

/* IO_MUX offset from CL_IOMUX. 0 means the pad is not bonded. */
static const unsigned short _mux_off[CL_PIN_COUNT] = {
	0x44, 0x88, 0x40, 0x84, 0x48, 0x6c, 0x60, 0x64,
	0x68, 0x54, 0x58, 0x5c, 0x34, 0x38, 0x30, 0x3c,
	0x4c, 0x50, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x8c,
	0x90, 0x24, 0x28, 0x2c, 0, 0, 0, 0,
	0x1c, 0x20, 0x14, 0x18, 0x04, 0x08, 0x0c, 0x10,
};

static mp_gpio_port_t _pins[CL_PIN_COUNT];
static mp_bool_t _gpio_irq;

static unsigned long _bit(int pin) {
	return(1UL << (pin & 31));
}

static unsigned long _bank(int pin, unsigned long low, unsigned long high) {
	return(CL_GPIO + (pin < 32 ? low : high));
}

static int _input_only(int pin) {
	return(pin >= 34);
}

static int _blocked(int pin) {
	if(pin < 0 || pin >= CL_PIN_COUNT || _mux_off[pin] == 0)
		return(1);
	if(pin == 20 || pin == 24)
		return(1);
	if(pin >= 6 && pin <= 11)
		return(1);
	return(0);
}

void cl_pad_digital(int pin) {
	unsigned long addr = 0;
	unsigned long bit = 0;

	switch(pin) {
		case 0: addr = CL_RTCIO + 0x98UL; bit = 1UL << 19; break;
		case 2: addr = CL_RTCIO + 0x9CUL; bit = 1UL << 19; break;
		case 4: addr = CL_RTCIO + 0x94UL; bit = 1UL << 19; break;
		case 12: addr = CL_RTCIO + 0xA8UL; bit = 1UL << 19; break;
		case 13: addr = CL_RTCIO + 0xA4UL; bit = 1UL << 19; break;
		case 14: addr = CL_RTCIO + 0xACUL; bit = 1UL << 19; break;
		case 15: addr = CL_RTCIO + 0xA0UL; bit = 1UL << 19; break;
		case 25: addr = CL_RTCIO + 0x84UL; bit = 1UL << 17; break;
		case 26: addr = CL_RTCIO + 0x88UL; bit = 1UL << 17; break;
		case 27: addr = CL_RTCIO + 0xB0UL; bit = 1UL << 19; break;
		case 32: addr = CL_RTCIO + 0x8CUL; bit = 1UL << 17; break;
		case 33: addr = CL_RTCIO + 0x8CUL; bit = 1UL << 18; break;
		case 36: addr = CL_RTCIO + 0x7CUL; bit = 1UL << 27; break;
		case 37: addr = CL_RTCIO + 0x7CUL; bit = 1UL << 26; break;
		case 38: addr = CL_RTCIO + 0x7CUL; bit = 1UL << 25; break;
		case 39: addr = CL_RTCIO + 0x7CUL; bit = 1UL << 24; break;
		default: return;
	}
	cl_clr(addr, bit);
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
		if(pin >= CL_PIN_COUNT)
			return(-1);
	}
	if(!digits || text[0])
		return(-1);
	return(pin);
}

static unsigned long _pin_reg(int pin) {
	return(CL_GPIO + 0x88UL + (unsigned long)pin * 4UL);
}

/* Function 2, input enabled, drive strength 2. The matrix output is GPIO_OUT. */
static void _mux(int pin, int pullup) {
	unsigned long value = (_GPIO_FUNC << 12) | (1UL << 9) | (2UL << 10);

	cl_pad_digital(pin);
	if(pullup && !_input_only(pin))
		value |= 1UL << 8;
	cl_write(CL_IOMUX + _mux_off[pin], value);
	if(!_input_only(pin))
		cl_write(CL_GPIO + 0x530UL + (unsigned long)pin * 4UL, (1UL << 10) | _SIG_GPIO);
}

static void _drive(int pin, int high) {
	if(_input_only(pin))
		return;
	cl_write(_bank(pin, 0x08UL, 0x14UL), high ? _bit(pin) : 0);
	if(!high)
		cl_write(_bank(pin, 0x0CUL, 0x18UL), _bit(pin));
}

static void _edge(mp_gpio_port_t *port, int type) {
	unsigned long addr = _pin_reg((int)port->pin);
	unsigned long value = cl_read(addr);

	value &= ~(7UL << 7);
	value |= ((unsigned long)type << 7);
	value |= _PRO_INT;
	cl_write(addr, value);
	cl_write(_bank((int)port->pin, 0x4CUL, 0x58UL), _bit((int)port->pin));
}

static void _gpio_isr(void *user) {
	unsigned long low;
	unsigned long high;
	int pin;

	(void)user;
	low = cl_read(CL_GPIO + 0x68UL);
	high = cl_read(CL_GPIO + 0x7CUL);
	cl_write(CL_GPIO + 0x4CUL, low);
	cl_write(CL_GPIO + 0x58UL, high);
	for(pin = 0; pin < 32; pin++) {
		if((low & (1UL << pin)) && _pins[pin].callback)
			_pins[pin].callback(_pins[pin].user);
	}
	for(pin = 32; pin < CL_PIN_COUNT; pin++) {
		if((high & (1UL << (pin - 32))) && _pins[pin].callback)
			_pins[pin].callback(_pins[pin].user);
	}
}

void mp_gpio_init(void) {
	int pin;

	_gpio_irq = NO;
	for(pin = 0; pin < CL_PIN_COUNT; pin++) {
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

	for(pin = 0; pin < CL_PIN_COUNT; pin++) {
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
	if(!_input_only((int)port->pin))
		cl_write(_bank((int)port->pin, 0x28UL, 0x34UL), _bit((int)port->pin));
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
		if(_input_only((int)port->pin)) {
			mp_printk("GPIO pa%u is input only", port->pin);
			return(FALSE);
		}
		_mux((int)port->pin, 0);
		_drive((int)port->pin, port->reverse == YES ? 1 : 0);
		cl_write(_bank((int)port->pin, 0x24UL, 0x30UL), _bit((int)port->pin));
	}
	else {
		if(!_input_only((int)port->pin))
			cl_write(_bank((int)port->pin, 0x28UL, 0x34UL), _bit((int)port->pin));
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
	if(mode == MP_GPIO_MODE_OUTPUT || mode == MP_GPIO_MODE_OUTPUT_OD) {
		if(_input_only((int)port->pin))
			return;
		mp_gpio_direction(port, MP_GPIO_OUTPUT);
	}
	else {
		mp_gpio_direction(port, MP_GPIO_INPUT);
		if(mode == MP_GPIO_MODE_INPUT_PULL)
			mp_gpio_pull_up(port);
	}
	if(_input_only((int)port->pin))
		return;
	addr = _pin_reg((int)port->pin);
	value = cl_read(addr);
	if(mode == MP_GPIO_MODE_OUTPUT_OD)
		value |= 1UL << 2;
	else
		value &= ~(1UL << 2);
	cl_write(addr, value);
}

void mp_gpio_af(mp_gpio_port_t *port, unsigned int af) {
	(void)port;
	(void)af;
}

void mp_gpio_pull_up(mp_gpio_port_t *port) {
	unsigned long addr;

	if(!port || _input_only((int)port->pin))
		return;
	addr = CL_IOMUX + _mux_off[port->pin];
	cl_set(addr, 1UL << 8);
	cl_clr(addr, 1UL << 7);
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	if(!port)
		return(OFF);
	return(cl_read(_bank((int)port->pin, 0x3CUL, 0x40UL)) & _bit((int)port->pin) ? ON : OFF);
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

	if(!port || _input_only((int)port->pin))
		return;
	out = cl_read(_bank((int)port->pin, 0x04UL, 0x10UL));
	_drive((int)port->pin, out & _bit((int)port->pin) ? 0 : 1);
}

mp_ret_t mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who) {
	if(!port || !in)
		return(FALSE);
	if(_gpio_irq == NO) {
		if(!mp_interrupt_set(CL_SRC_GPIO, _gpio_isr, NULL, "GPIO"))
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
	value = cl_read(addr);
	value &= ~(7UL << 7);
	value &= ~(0x1FUL << 13);
	cl_write(addr, value);
	cl_write(_bank((int)port->pin, 0x4CUL, 0x58UL), _bit((int)port->pin));
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
		cl_set(_pin_reg((int)port->pin), _PRO_INT);
}

void mp_gpio_interrupt_disable(mp_gpio_port_t *port) {
	if(port)
		cl_clr(_pin_reg((int)port->pin), 0x1FUL << 13);
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
	value = cl_read(_pin_reg((int)port->pin));
	type = (int)((value >> 7) & 7UL);
	if(type == _EDGE_RISE)
		_edge(port, _EDGE_FALL);
	else
		_edge(port, _EDGE_RISE);
}

/**@}*/
