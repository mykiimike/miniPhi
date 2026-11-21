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

/* Host test of the SSD1306 page map. Not linked into the firmware. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mp_gpio_port_s mp_gpio_port_t;

struct mp_gpio_port_s {
	int level;
};

typedef enum {
	MP_GPIO_OUTPUT = 2,
} mp_gpio_direction_t;

#define SUPPORT_DRV_LCD_SSD1306

#include <mp.h>
#include "../ssd1306.h"

static int failed;
static mp_gpio_port_t sda, scl;
static unsigned char last_addr;
static unsigned char payload[160];
static int plen;
static int tx_n;
static unsigned char page_cmd[8][3];
static unsigned char col5[8];
static int last_page = -1;
static int pages;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

char *mp_options_get(mp_options_t *options, char *key) {
	(void)options;
	(void)key;
	return(NULL);
}

mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who) {
	(void)text;
	(void)who;
	return(NULL);
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	(void)port;
	return(TRUE);
}

mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction) {
	(void)port;
	(void)direction;
	return(TRUE);
}

void mp_gpio_set(mp_gpio_port_t *port) {
	if(port)
		port->level = 1;
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port)
		port->level = 0;
}

mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_bus_t *bus, mp_options_t *options, char *who) {
	(void)kernel;
	(void)bus;
	(void)options;
	(void)who;
	return(TRUE);
}

void mp_i2c_close(mp_i2c_bus_t *bus) {
	(void)bus;
}

int mp_i2c_tx(mp_i2c_bus_t *bus, unsigned char address, const unsigned char *data, int len) {
	int page;

	(void)bus;
	last_addr = address;
	tx_n++;
	if(len < 1)
		return(1);
	if(data[0] == 0x00 && len == 4 && (data[1] & 0xF0) == 0xB0) {
		page = data[1] & 0x0F;
		if(page < 8) {
			page_cmd[page][0] = data[1];
			page_cmd[page][1] = data[2];
			page_cmd[page][2] = data[3];
			last_page = page;
			pages++;
		}
	}
	if(data[0] == 0x40 && len == 1 + MP_DRV_SSD1306_WIDTH && last_page >= 0 && last_page < 8)
		col5[last_page] = data[1 + 5];
	plen = len;
	if(len > 1)
		memcpy(payload, data, (size_t)(len < (int)sizeof(payload) ? len : (int)sizeof(payload)));
	return(1);
}

#include "../ssd1306.c"

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "ssd1306: %s\n", msg);
}

int main(void) {
	mp_drv_ssd1306_t lcd;
	unsigned char setup[25] = {
		0xAE,
		0xD5, 0x80,
		0xA8, 0x3F,
		0xD3, 0x00,
		0x40,
		0x8D, 0x14,
		0x20, 0x02,
		0xA1,
		0xC8,
		0xDA, 0x12,
		0x81, 0xCF,
		0xD9, 0xF1,
		0xDB, 0x40,
		0xA4,
		0xA6,
		0x2E
	};

	expect(_ssd1306_address(NULL) == 0x3C, "addr default");
	expect(_ssd1306_address("0x3D") == 0x3D, "addr 3d");
	expect(_ssd1306_address("0x3E") == 0, "addr other");
	expect(_ssd1306_contrast_option(NULL) == 0xCF, "contrast 207");
	expect(_ssd1306_contrast_option("300") == 0xCF, "contrast junk");
	expect(_ssd1306_contrast_option("16") == 16, "contrast 16");

	memset(&lcd, 0, sizeof(lcd));
	lcd.address = 0x3C;
	lcd.contrast = 0xCF;
	lcd.bus.sda = &sda;
	lcd.bus.scl = &scl;
	expect(_ssd1306_setup(&lcd) == TRUE, "setup");
	expect(last_addr == 0x3C, "setup addr");
	/* control 0x00 is a command. mux 0x3F is 64 rows, 0x02 is page mode,
	 * 0x14 turns the charge pump on, 0x12 is the 128x64 COM map. */
	expect(plen == 26 && payload[0] == 0x00, "setup control");
	expect(memcmp(payload + 1, setup, 25) == 0, "setup bytes");

	mp_drv_ssd1306_clear(&lcd);
	lcd.dirty = 0;
	mp_drv_ssd1306_point(&lcd, 5, 0, 1);
	mp_drv_ssd1306_point(&lcd, 5, 9, 1);
	expect(lcd.gram[5] == 0x01, "page0 bit0");
	expect(lcd.gram[MP_DRV_SSD1306_WIDTH + 5] == 0x02, "page1 bit1");
	lcd.on = 1;
	pages = 0;
	memset(page_cmd, 0, sizeof(page_cmd));
	expect(mp_drv_ssd1306_flush(&lcd) == TRUE, "flush");
	expect(pages == 2, "two pages");
	expect(page_cmd[0][0] == 0xB0 && page_cmd[0][1] == 0x00 && page_cmd[0][2] == 0x10, "page 0 addr");
	expect(page_cmd[1][0] == 0xB1 && page_cmd[1][1] == 0x00 && page_cmd[1][2] == 0x10, "page 1 addr");
	expect(col5[0] == 0x01 && col5[1] == 0x02, "data column");

	expect(mp_drv_ssd1306_contrast(&lcd, 300) == TRUE, "contrast clamp");
	expect(lcd.contrast == 255, "contrast 255");
	expect(payload[0] == 0x00 && payload[1] == 0x81 && payload[2] == 0xFF, "contrast cmd");

	expect(mp_drv_ssd1306_power(&lcd, 0) == TRUE, "off");
	expect(lcd.on == 0 && payload[0] == 0x00 && payload[1] == 0xAE, "display off");
	return(failed ? 1 : 0);
}
