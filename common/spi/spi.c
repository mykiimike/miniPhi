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

#ifdef SUPPORT_COMMON_SPI

static void _mp_spi_half(mp_spi_bus_t *bus) {
	unsigned long start;
	unsigned long half = bus->half_us;

	if(!half)
		return;
	start = mp_clock_us();
	while((unsigned long)(mp_clock_us() - start) < half)
		;
}

static char *_mp_spi_option(mp_options_t *options, char *first, char *second, char *third) {
	char *text;

	text = mp_options_get(options, first);
	if(text && text[0])
		return(text);
	if(second) {
		text = mp_options_get(options, second);
		if(text && text[0])
			return(text);
	}
	if(third) {
		text = mp_options_get(options, third);
		if(text && text[0])
			return(text);
	}
	return(NULL);
}

static void _mp_spi_release(mp_gpio_port_t **pin) {
	if(*pin) {
		mp_gpio_release(*pin);
		*pin = NULL;
	}
}

mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_bus_t *bus, mp_options_t *options, char *who) {
	char *sclk, *mosi, *miso;

	memset(bus, 0, sizeof(*bus));
	bus->kernel = kernel;
	bus->half_us = 5UL;
	if(!who || !who[0])
		who = "SPI";

	sclk = _mp_spi_option(options, "sclk", "sck", "clk");
	mosi = _mp_spi_option(options, "mosi", "simo", NULL);
	bus->sclk = mp_gpio_text_handle(sclk, who);
	bus->mosi = mp_gpio_text_handle(mosi, who);
	if(!bus->sclk || !bus->mosi) {
		mp_printk("SPI: need sclk and mosi");
		_mp_spi_release(&bus->sclk);
		_mp_spi_release(&bus->mosi);
		bus->kernel = NULL;
		return(FALSE);
	}

	miso = mp_options_get(options, "miso");
	if(miso && miso[0]) {
		bus->miso = mp_gpio_text_handle(miso, who);
		if(!bus->miso) {
			mp_printk("SPI: miso is not free");
			_mp_spi_release(&bus->sclk);
			_mp_spi_release(&bus->mosi);
			bus->kernel = NULL;
			return(FALSE);
		}
		mp_gpio_mode(bus->miso, MP_GPIO_MODE_INPUT);
	}

	/* mode 0: clock idles low, MOSI idles low */
	mp_gpio_mode(bus->sclk, MP_GPIO_MODE_OUTPUT);
	mp_gpio_unset(bus->sclk);
	mp_gpio_mode(bus->mosi, MP_GPIO_MODE_OUTPUT);
	mp_gpio_unset(bus->mosi);
	return(TRUE);
}

mp_bool_t __attribute__((weak)) mp_spi_bus_hw_open(mp_spi_bus_t *bus) {
	(void)bus;
	return(NO);
}

int __attribute__((weak)) mp_spi_bus_hw_tx(mp_spi_bus_t *bus, const unsigned char *data, int len) {
	(void)bus;
	(void)data;
	(void)len;
	return(0);
}

void __attribute__((weak)) mp_spi_bus_hw_flush(mp_spi_bus_t *bus) {
	(void)bus;
}

void __attribute__((weak)) mp_spi_bus_hw_close(mp_spi_bus_t *bus) {
	(void)bus;
}

mp_bool_t mp_spi_fast(mp_spi_bus_t *bus) {
	if(!bus || !bus->sclk || !bus->mosi)
		return(NO);
	if(bus->fast == NO && mp_spi_bus_hw_open(bus) == YES)
		bus->fast = YES;
	return(bus->fast);
}

void mp_spi_flush(mp_spi_bus_t *bus) {
	if(bus && bus->fast == YES)
		mp_spi_bus_hw_flush(bus);
}

void mp_spi_close(mp_spi_bus_t *bus) {
	if(!bus || !bus->kernel)
		return;
	if(bus->fast == YES)
		mp_spi_bus_hw_close(bus);
	bus->fast = NO;
	_mp_spi_release(&bus->sclk);
	_mp_spi_release(&bus->mosi);
	_mp_spi_release(&bus->miso);
	bus->kernel = NULL;
}

void mp_spi_setup(mp_spi_bus_t *bus, unsigned long half_us) {
	if(bus)
		bus->half_us = half_us;
}

unsigned char mp_spi_xfer(mp_spi_bus_t *bus, unsigned char byte) {
	int bit;
	unsigned char in = 0;

	if(!bus || !bus->sclk || !bus->mosi)
		return(0);
	if(bus->fast == YES && mp_spi_bus_hw_tx(bus, &byte, 1))
		return(0);
	for(bit = 7; bit >= 0; bit--) {
		if((byte >> bit) & 1)
			mp_gpio_set(bus->mosi);
		else
			mp_gpio_unset(bus->mosi);
		_mp_spi_half(bus);
		mp_gpio_set(bus->sclk);
		/* mode 0: MISO is stable while SCLK is high and changes when it falls */
		_mp_spi_half(bus);
		if(bus->miso && mp_gpio_read(bus->miso) == YES)
			in |= (unsigned char)(1U << bit);
		mp_gpio_unset(bus->sclk);
	}
	return(in);
}

void mp_spi_write_byte(mp_spi_bus_t *bus, unsigned char byte) {
	(void)mp_spi_xfer(bus, byte);
}

int mp_spi_tx(mp_spi_bus_t *bus, const unsigned char *data, int len) {
	int index;

	if(!bus || !bus->sclk || !bus->mosi || len < 0 || (len > 0 && !data))
		return(0);
	if(bus->fast == YES && mp_spi_bus_hw_tx(bus, data, len))
		return(1);
	for(index = 0; index < len; index++)
		mp_spi_write_byte(bus, data[index]);
	return(1);
}

#ifdef SUPPORT_COMMON_DMA
/* The arch replaces these when the pins belong to a hardware master. */
mp_dma_engine_t *__attribute__((weak)) mp_spi_bus_engine(mp_spi_bus_t *bus, mp_dma_dir_t dir) {
	(void)bus;
	(void)dir;
	return(NULL);
}

void __attribute__((weak)) mp_spi_bus_engine_drop(mp_spi_bus_t *bus) {
	(void)bus;
}
#endif

#endif
