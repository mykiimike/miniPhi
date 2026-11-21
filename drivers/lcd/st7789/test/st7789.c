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

/* Host test of the ST7789 command bytes. Not linked into the firmware. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct mp_gpio_port_s mp_gpio_port_t;

struct mp_gpio_port_s {
	int level;
};

typedef enum {
	MP_GPIO_INPUT = 1,
	MP_GPIO_OUTPUT = 2,
} mp_gpio_direction_t;

typedef enum {
	MP_GPIO_MODE_INPUT = 1,
	MP_GPIO_MODE_OUTPUT = 3,
} mp_gpio_mode_t;

#define SUPPORT_DRV_LCD_ST7789

#include <mp.h>
#include "../st7789.h"

/* ST7789 MADCTL 36h: MY D7, MX D6, MV D5, ML D4, BGR D3. */
#define MAD_MY 0x80
#define MAD_MX 0x40
#define MAD_MV 0x20
#define MAD_ML 0x10
#define MAD_BGR 0x08

static int failed;
static mp_gpio_port_t pin_dc, pin_cs, pin_reset, pin_bl;
static unsigned char logb[2048];
static unsigned char logdc[2048];
static unsigned char logcs[2048];
static int nlog;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

unsigned long mp_clock_us(void) {
	static unsigned long us;

	us += 1000000UL;
	return(us);
}

unsigned long mp_clock_get_speed(void) {
	return(160000000UL);
}

char *mp_options_get(mp_options_t *options, char *key) {
	(void)options;
	(void)key;
	return(NULL);
}

mp_ret_t mp_options_cmp(char *a, char *b) {
	if(!a || !b)
		return(FALSE);
	return(strcmp(a, b) == 0 ? TRUE : FALSE);
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

void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode) {
	(void)port;
	(void)mode;
}

void mp_gpio_set(mp_gpio_port_t *port) {
	if(port)
		port->level = 1;
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port)
		port->level = 0;
}

mp_bool_t mp_spi_fast(mp_spi_bus_t *bus) {
	(void)bus;
	return(NO);
}

mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_bus_t *bus, mp_options_t *options, char *who) {
	(void)kernel;
	(void)bus;
	(void)options;
	(void)who;
	return(TRUE);
}

void mp_spi_close(mp_spi_bus_t *bus) {
	(void)bus;
}

void mp_spi_setup(mp_spi_bus_t *bus, unsigned long half_us) {
	(void)bus;
	(void)half_us;
}

int mp_spi_tx(mp_spi_bus_t *bus, const unsigned char *data, int len) {
	int i;

	(void)bus;
	for(i = 0; i < len; i++) {
		if(nlog >= (int)sizeof(logb))
			break;
		logb[nlog] = data[i];
		logdc[nlog] = (unsigned char)pin_dc.level;
		logcs[nlog] = (unsigned char)pin_cs.level;
		nlog++;
	}
	return(1);
}

#include "../st7789.c"

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "st7789: %s\n", msg);
}

static void reset_log(void) {
	nlog = 0;
}

static int find_cmd(unsigned char cmd, int from) {
	int i;

	for(i = from; i < nlog; i++) {
		if(logdc[i] == 0 && logb[i] == cmd)
			return(i);
	}
	return(-1);
}

static void expect_data(int at, const unsigned char *data, int n, const char *msg) {
	int i;

	expect(at >= 0, msg);
	if(at < 0)
		return;
	expect(logcs[at] == 0, msg);
	for(i = 0; i < n; i++) {
		expect(at + 1 + i < nlog, msg);
		if(at + 1 + i >= nlog)
			return;
		expect(logdc[at + 1 + i] == 1 && logb[at + 1 + i] == data[i], msg);
	}
}

/* CASET 2Ah and RASET 2Bh are big-endian inclusive ends. */
static void expect_window(int x, int y, int w, int h, int xoff, int yoff) {
	unsigned int x0 = (unsigned int)(x + xoff);
	unsigned int x1 = (unsigned int)(x + w - 1 + xoff);
	unsigned int y0 = (unsigned int)(y + yoff);
	unsigned int y1 = (unsigned int)(y + h - 1 + yoff);
	unsigned char column[4];
	unsigned char row[4];
	int caset, raset;

	column[0] = (unsigned char)(x0 >> 8);
	column[1] = (unsigned char)x0;
	column[2] = (unsigned char)(x1 >> 8);
	column[3] = (unsigned char)x1;
	row[0] = (unsigned char)(y0 >> 8);
	row[1] = (unsigned char)y0;
	row[2] = (unsigned char)(y1 >> 8);
	row[3] = (unsigned char)y1;
	caset = find_cmd(0x2A, 0);
	raset = find_cmd(0x2B, 0);
	expect_data(caset, column, 4, "CASET");
	expect_data(raset, row, 4, "RASET");
	expect(find_cmd(0x2C, raset) == raset + 5, "RAMWR");
}

static void arm(mp_drv_st7789_t *lcd) {
	memset(lcd, 0, sizeof(*lcd));
	lcd->dc = &pin_dc;
	lcd->cs = &pin_cs;
	lcd->reset = &pin_reset;
	lcd->backlight = &pin_bl;
	pin_dc.level = 1;
	pin_cs.level = 1;
	pin_reset.level = 1;
	pin_bl.level = 1;
	reset_log();
}

int main(void) {
	mp_drv_st7789_t lcd;
	int off = (MP_DRV_ST7789_GRAM - MP_DRV_ST7789_SHORT) / 2;
	unsigned char porch[5] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 };
	unsigned char colmod;

	expect(off == 34, "offset 34");

	arm(&lcd);
	expect(_st7789_glass(&lcd, NULL, NULL, NULL) == 1, "default glass");
	expect(lcd.width == MP_DRV_ST7789_LONG && lcd.height == MP_DRV_ST7789_SHORT, "172 landscape size");
	expect(lcd.xoff == 0 && lcd.yoff == off, "172 landscape offset");
	expect(lcd.madctl == (unsigned char)(MAD_MX | MAD_MV | MAD_ML), "172 madctl");
	expect(lcd.colmod == 0x05 && lcd.invert == 1, "172 colmod");
	expect(_st7789_setup(&lcd) == 1, "172 setup");
	expect(pin_bl.level == 0, "backlight off");
	expect(pin_reset.level == 1, "reset released");
	expect(find_cmd(0x01, 0) >= 0, "SWRESET");
	expect(find_cmd(0x11, 0) > find_cmd(0x01, 0), "SLPOUT");
	colmod = 0x05;
	expect_data(find_cmd(0x3A, 0), &colmod, 1, "COLMOD 172");
	expect_data(find_cmd(0x36, 0), &lcd.madctl, 1, "MADCTL 172");
	/* PORCTRL B2h reset value in the ST7789 datasheet. */
	expect_data(find_cmd(0xB2, 0), porch, 5, "PORCTRL");
	expect(find_cmd(0x21, 0) > 0, "INVON");
	expect(find_cmd(0x13, 0) > find_cmd(0x21, 0), "NORON");
	expect(find_cmd(0x29, 0) > find_cmd(0x13, 0), "DISPON");

	reset_log();
	_st7789_window(&lcd, 0, 0, lcd.width, lcd.height);
	expect_window(0, 0, lcd.width, lcd.height, 0, off);
	/* full landscape: columns 0..319, rows 34..205 */
	expect(off == 34 && off + MP_DRV_ST7789_SHORT - 1 == 205, "172 span");

	reset_log();
	_st7789_window(&lcd, 10, 20, 30, 40);
	expect_window(10, 20, 30, 40, 0, off);

	arm(&lcd);
	expect(_st7789_glass(&lcd, "240x320", "landscape", "auto") == 1, "240 glass");
	expect(lcd.width == 320 && lcd.height == 240, "240 landscape size");
	expect(lcd.xoff == 0 && lcd.yoff == 0, "240 offset");
	expect(lcd.madctl == (unsigned char)(MAD_MX | MAD_MV | MAD_ML | MAD_BGR), "240 bgr");
	expect(lcd.colmod == 0x55 && lcd.invert == 0, "240 colmod");
	expect(_st7789_setup(&lcd) == 1, "240 setup");
	colmod = 0x55;
	expect_data(find_cmd(0x3A, 0), &colmod, 1, "COLMOD 240");
	expect(find_cmd(0x21, 0) < 0, "no INVON");

	arm(&lcd);
	expect(_st7789_glass(&lcd, "240x320", "portrait-flipped", "rgb") == 1, "portrait rgb");
	expect(lcd.width == 240 && lcd.height == 320, "portrait size");
	expect(lcd.xoff == 0 && lcd.yoff == 0, "portrait offset");
	expect(lcd.madctl == (unsigned char)(MAD_MY | MAD_MX), "portrait madctl");
	reset_log();
	_st7789_window(&lcd, 0, 0, 240, 320);
	expect_window(0, 0, 240, 320, 0, 0);

	arm(&lcd);
	expect(_st7789_glass(&lcd, "172x320", "portrait", "bgr") == 1, "172 portrait bgr");
	expect(lcd.width == 172 && lcd.height == 320, "172 portrait size");
	expect(lcd.xoff == off && lcd.yoff == 0, "172 portrait offset");
	expect(lcd.madctl == (unsigned char)(MAD_BGR), "portrait bgr bit");

	expect(_st7789_glass(&lcd, "nope", NULL, NULL) == 0, "unknown glass");
	/* The row cursor is only read by the PUI blit. */
	(void)_st7789_stream_y;
	return(failed ? 1 : 0);
}
