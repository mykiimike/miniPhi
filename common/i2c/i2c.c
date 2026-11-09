/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                   *
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_COMMON_I2C

static void _mp_i2c_half(mp_i2c_bus_t *bus) {
	unsigned long start = mp_clock_us();
	unsigned long half = bus->half_us ? bus->half_us : 5UL;

	while((unsigned long)(mp_clock_us() - start) < half)
		;
}

/* a released line is an input with its pull-up; a low line is open-drain */
static void _mp_i2c_line(mp_gpio_port_t *pin, int high) {
	if(high) {
		mp_gpio_set(pin);
		mp_gpio_mode(pin, MP_GPIO_MODE_INPUT_PULL);
	}
	else {
		mp_gpio_mode(pin, MP_GPIO_MODE_OUTPUT_OD);
		mp_gpio_unset(pin);
	}
}

static int _mp_i2c_scl_high(mp_i2c_bus_t *bus) {
	int spin;

	_mp_i2c_line(bus->scl, 1);
	for(spin = 0; spin < 1000 && mp_gpio_read(bus->scl) != ON; spin++)
		_mp_i2c_half(bus);
	return(mp_gpio_read(bus->scl) == ON ? 1 : 0);
}

static void _mp_i2c_start(mp_i2c_bus_t *bus) {
	_mp_i2c_line(bus->sda, 1);
	_mp_i2c_scl_high(bus);
	_mp_i2c_line(bus->sda, 0);
	_mp_i2c_line(bus->scl, 0);
}

static void _mp_i2c_stop(mp_i2c_bus_t *bus) {
	_mp_i2c_line(bus->sda, 0);
	_mp_i2c_scl_high(bus);
	_mp_i2c_line(bus->sda, 1);
}

static int _mp_i2c_put(mp_i2c_bus_t *bus, unsigned char byte) {
	int bit, ack;

	for(bit = 7; bit >= 0; bit--) {
		_mp_i2c_line(bus->sda, (byte >> bit) & 1);
		if(!_mp_i2c_scl_high(bus))
			return(0);
		_mp_i2c_line(bus->scl, 0);
	}
	_mp_i2c_line(bus->sda, 1);
	if(!_mp_i2c_scl_high(bus))
		return(0);
	ack = mp_gpio_read(bus->sda) == ON ? 0 : 1;
	_mp_i2c_line(bus->scl, 0);
	return(ack);
}

static int _mp_i2c_get(mp_i2c_bus_t *bus, int ack) {
	int bit, byte = 0;

	_mp_i2c_line(bus->sda, 1);
	for(bit = 7; bit >= 0; bit--) {
		if(!_mp_i2c_scl_high(bus))
			return(-1);
		if(mp_gpio_read(bus->sda) == ON)
			byte |= (1 << bit);
		_mp_i2c_line(bus->scl, 0);
	}
	_mp_i2c_line(bus->sda, ack ? 0 : 1);
	if(!_mp_i2c_scl_high(bus))
		return(-1);
	_mp_i2c_line(bus->scl, 0);
	_mp_i2c_line(bus->sda, 1);
	return(byte);
}

mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_bus_t *bus, mp_options_t *options, char *who) {
	char *sda, *clk;

	memset(bus, 0, sizeof(*bus));
	bus->kernel = kernel;
	bus->half_us = 5UL;
	sda = mp_options_get(options, "sda");
	clk = mp_options_get(options, "clk");
	bus->sda = mp_gpio_text_handle(sda, who ? who : "I2C SDA");
	bus->scl = mp_gpio_text_handle(clk, who ? who : "I2C SCL");
	if(!bus->sda || !bus->scl) {
		mp_printk("I2C: need sda and clk");
		return(FALSE);
	}
	_mp_i2c_line(bus->sda, 1);
	_mp_i2c_line(bus->scl, 1);
	(void)who;
	return(TRUE);
}

void mp_i2c_close(mp_i2c_bus_t *bus) {
	if(!bus || !bus->kernel)
		return;
	if(bus->sda) {
		mp_gpio_release(bus->sda);
		bus->sda = NULL;
	}
	if(bus->scl) {
		mp_gpio_release(bus->scl);
		bus->scl = NULL;
	}
	bus->kernel = NULL;
}

void mp_i2c_setup(mp_i2c_bus_t *bus, unsigned long half_us) {
	if(half_us)
		bus->half_us = half_us;
}

int mp_i2c_tx(mp_i2c_bus_t *bus, unsigned char address, const unsigned char *data, int len) {
	int index;

	_mp_i2c_start(bus);
	if(!_mp_i2c_put(bus, (unsigned char)(address << 1))) {
		_mp_i2c_stop(bus);
		return(0);
	}
	for(index = 0; index < len; index++) {
		if(!_mp_i2c_put(bus, data[index])) {
			_mp_i2c_stop(bus);
			return(0);
		}
	}
	_mp_i2c_stop(bus);
	return(1);
}

int mp_i2c_write_reg(mp_i2c_bus_t *bus, unsigned char address, unsigned char reg, const unsigned char *data, int len) {
	_mp_i2c_start(bus);
	if(!_mp_i2c_put(bus, (unsigned char)(address << 1)) || !_mp_i2c_put(bus, reg)) {
		_mp_i2c_stop(bus);
		return(0);
	}
	while(len-- > 0) {
		if(!_mp_i2c_put(bus, *data++)) {
			_mp_i2c_stop(bus);
			return(0);
		}
	}
	_mp_i2c_stop(bus);
	return(1);
}

int mp_i2c_read_reg(mp_i2c_bus_t *bus, unsigned char address, unsigned char reg, unsigned char *buf, int len) {
	int index, byte;

	if(len <= 0)
		return(0);
	_mp_i2c_start(bus);
	if(!_mp_i2c_put(bus, (unsigned char)(address << 1)) || !_mp_i2c_put(bus, reg)) {
		_mp_i2c_stop(bus);
		return(0);
	}
	_mp_i2c_start(bus);
	if(!_mp_i2c_put(bus, (unsigned char)((address << 1) | 1))) {
		_mp_i2c_stop(bus);
			return(0);
	}
	for(index = 0; index < len; index++) {
		byte = _mp_i2c_get(bus, index < len - 1);
		if(byte < 0) {
			_mp_i2c_stop(bus);
			return(0);
		}
		buf[index] = (unsigned char)byte;
	}
	_mp_i2c_stop(bus);
	return(1);
}

#endif
