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

#ifdef SUPPORT_DRV_LCD_SSD1306

/* Co = 0, so every following byte is the same kind. */
#define _SSD1306_CMD 0x00
#define _SSD1306_DATA 0x40

/* "miniPhi" in 5x7 columns, a 0 between letters. Bit 0 is the top pixel. */
static const unsigned char _ssd1306_name[] = {
	0x7C, 0x04, 0x18, 0x04, 0x78, 0,
	0x00, 0x44, 0x7D, 0x40, 0x00, 0,
	0x7C, 0x08, 0x04, 0x04, 0x78, 0,
	0x00, 0x44, 0x7D, 0x40, 0x00, 0,
	0x7F, 0x09, 0x09, 0x09, 0x06, 0,
	0x7F, 0x08, 0x04, 0x04, 0x78, 0,
	0x00, 0x44, 0x7D, 0x40, 0x00,
};

/* The control byte (0x00 command, 0x40 data) leads the payload, then the
 * shared bus sends address + body in one transaction. */
static int _ssd1306_write(mp_drv_ssd1306_t *lcd, unsigned char control, const unsigned char *data, int count) {
	unsigned char body[1 + MP_DRV_SSD1306_WIDTH];
	int index;

	if(count < 0 || count > MP_DRV_SSD1306_WIDTH)
		return(0);
	body[0] = control;
	for(index = 0; index < count; index++)
		body[1 + index] = data[index];
	return(mp_i2c_tx(&lcd->bus, lcd->address, body, count + 1));
}

static int _ssd1306_command(mp_drv_ssd1306_t *lcd, const unsigned char *command, int count) {
	return(_ssd1306_write(lcd, _SSD1306_CMD, command, count));
}

static mp_ret_t _ssd1306_page(mp_drv_ssd1306_t *lcd, int page) {
	unsigned char command[3];

	command[0] = (unsigned char)(0xB0 | page);
	command[1] = 0x00;
	command[2] = 0x10;
	if(!_ssd1306_command(lcd, command, 3))
		return(FALSE);
	if(!_ssd1306_write(lcd, _SSD1306_DATA, lcd->gram + page * MP_DRV_SSD1306_WIDTH, MP_DRV_SSD1306_WIDTH))
		return(FALSE);
	return(TRUE);
}

mp_ret_t mp_drv_ssd1306_flush(mp_drv_ssd1306_t *lcd) {
	int page;

	if(!lcd)
		return(FALSE);
	if(lcd->on == 0)
		return(TRUE);
	for(page = 0; page < MP_DRV_SSD1306_PAGES; page++) {
		if((lcd->dirty & (1u << page)) == 0)
			continue;
		if(_ssd1306_page(lcd, page) == FALSE)
			return(FALSE);
	}
	lcd->dirty = 0;
	return(TRUE);
}

void mp_drv_ssd1306_clear(mp_drv_ssd1306_t *lcd) {
	if(!lcd)
		return;
	memset(lcd->gram, 0, sizeof(lcd->gram));
	lcd->dirty = (1u << MP_DRV_SSD1306_PAGES) - 1;
}

void mp_drv_ssd1306_point(mp_drv_ssd1306_t *lcd, int x, int y, int on) {
	int page;
	unsigned char bit;
	unsigned char *cell;

	if(!lcd || x < 0 || y < 0 || x >= MP_DRV_SSD1306_WIDTH || y >= MP_DRV_SSD1306_HEIGHT)
		return;
	page = y >> 3;
	bit = (unsigned char)(1u << (y & 7));
	cell = &lcd->gram[page * MP_DRV_SSD1306_WIDTH + x];
	if(on)
		*cell |= bit;
	else
		*cell &= (unsigned char)~bit;
	lcd->dirty |= (unsigned char)(1u << page);
}

mp_ret_t mp_drv_ssd1306_contrast(mp_drv_ssd1306_t *lcd, int contrast) {
	unsigned char command[2];

	if(!lcd)
		return(FALSE);
	if(contrast < 0)
		contrast = 0;
	if(contrast > 255)
		contrast = 255;
	lcd->contrast = (unsigned char)contrast;
	command[0] = 0x81;
	command[1] = lcd->contrast;
	if(!_ssd1306_command(lcd, command, 2))
		return(FALSE);
	return(TRUE);
}

mp_ret_t mp_drv_ssd1306_power(mp_drv_ssd1306_t *lcd, int on) {
	unsigned char command;

	if(!lcd || !lcd->bus.sda)
		return(FALSE);
	if(on) {
		command = 0xAF;
		if(!_ssd1306_command(lcd, &command, 1))
			return(FALSE);
		lcd->on = 1;
		lcd->dirty = (1u << MP_DRV_SSD1306_PAGES) - 1;
		return(mp_drv_ssd1306_flush(lcd));
	}
	command = 0xAE;
	if(!_ssd1306_command(lcd, &command, 1))
		return(FALSE);
	lcd->on = 0;
	return(TRUE);
}

static mp_ret_t _ssd1306_setup(mp_drv_ssd1306_t *lcd) {
	unsigned char command[25];
	int count;

	count = 0;
	command[count++] = 0xAE;
	command[count++] = 0xD5;
	command[count++] = 0x80;
	command[count++] = 0xA8;
	command[count++] = 0x3F;
	command[count++] = 0xD3;
	command[count++] = 0x00;
	command[count++] = 0x40;
	/* charge pump on: a 4-wire module has no external panel supply */
	command[count++] = 0x8D;
	command[count++] = 0x14;
	/* page addressing, so one flush sends one page */
	command[count++] = 0x20;
	command[count++] = 0x02;
	command[count++] = 0xA1;
	command[count++] = 0xC8;
	command[count++] = 0xDA;
	command[count++] = 0x12;
	command[count++] = 0x81;
	command[count++] = lcd->contrast;
	command[count++] = 0xD9;
	command[count++] = 0xF1;
	command[count++] = 0xDB;
	command[count++] = 0x40;
	command[count++] = 0xA4;
	command[count++] = 0xA6;
	command[count++] = 0x2E;
	return(_ssd1306_command(lcd, command, count) ? TRUE : FALSE);
}

static void _ssd1306_picture(mp_drv_ssd1306_t *lcd) {
	int x, y, column, bit;

	mp_drv_ssd1306_clear(lcd);
	for(x = 0; x < MP_DRV_SSD1306_WIDTH; x++) {
		mp_drv_ssd1306_point(lcd, x, 0, 1);
		mp_drv_ssd1306_point(lcd, x, MP_DRV_SSD1306_HEIGHT - 1, 1);
	}
	for(y = 0; y < MP_DRV_SSD1306_HEIGHT; y++) {
		mp_drv_ssd1306_point(lcd, 0, y, 1);
		mp_drv_ssd1306_point(lcd, MP_DRV_SSD1306_WIDTH - 1, y, 1);
	}
	x = (MP_DRV_SSD1306_WIDTH - (int)sizeof(_ssd1306_name)) / 2;
	y = (MP_DRV_SSD1306_HEIGHT - 7) / 2;
	for(column = 0; column < (int)sizeof(_ssd1306_name); column++) {
		for(bit = 0; bit < 7; bit++) {
			if(_ssd1306_name[column] & (1u << bit))
				mp_drv_ssd1306_point(lcd, x + column, y + bit, 1);
		}
	}
}

static unsigned char _ssd1306_address(char *text) {
	unsigned long value;

	if(!text || !text[0])
		return(0x3C);
	value = strtoul(text, NULL, 0);
	if(value == 0x3CUL || value == 0x3DUL)
		return((unsigned char)value);
	return(0);
}

static unsigned char _ssd1306_contrast_option(char *text) {
	unsigned long value;

	if(!text || !text[0])
		return(0xCF);
	value = strtoul(text, NULL, 10);
	if(value > 255UL)
		return(0xCF);
	return((unsigned char)value);
}

static void _ssd1306_release(mp_gpio_port_t **pin) {
	if(*pin) {
		mp_gpio_release(*pin);
		*pin = NULL;
	}
}

mp_ret_t mp_drv_ssd1306_fini(mp_drv_ssd1306_t *lcd) {
	if(!lcd)
		return(TRUE);
	if(lcd->on && lcd->bus.sda && lcd->bus.scl)
		mp_drv_ssd1306_power(lcd, 0);
	lcd->on = 0;
	_ssd1306_release(&lcd->reset);
	mp_i2c_close(&lcd->bus);
	return(TRUE);
}

mp_ret_t mp_drv_ssd1306_init(mp_kernel_t *kernel, mp_drv_ssd1306_t *lcd, mp_options_t *options, char *who) {
	char *reset;
	unsigned char command;

	memset(lcd, 0, sizeof(*lcd));
	lcd->kernel = kernel;
	if(!who || !who[0])
		who = "ssd1306";

	lcd->address = _ssd1306_address(mp_options_get(options, "address"));
	if(lcd->address == 0) {
		mp_printk("SSD1306: address is 0x3C or 0x3D");
		return(FALSE);
	}
	lcd->contrast = _ssd1306_contrast_option(mp_options_get(options, "contrast"));

	if(!mp_i2c_open(kernel, &lcd->bus, options, who)) {
		mp_printk("SSD1306: SDA and SCL are required");
		mp_drv_ssd1306_fini(lcd);
		return(FALSE);
	}

	reset = mp_options_get(options, "reset");
	if(reset && reset[0]) {
		lcd->reset = mp_gpio_text_handle(reset, who);
		if(!lcd->reset) {
			mp_printk("SSD1306: reset is not free");
			mp_drv_ssd1306_fini(lcd);
			return(FALSE);
		}
		mp_gpio_set(lcd->reset);
		mp_gpio_direction(lcd->reset, MP_GPIO_OUTPUT);
		mp_gpio_unset(lcd->reset);
		mp_clock_delay(1);
		mp_gpio_set(lcd->reset);
		mp_clock_delay(1);
	}

	command = 0xAE;
	if(!_ssd1306_command(lcd, &command, 1)) {
		mp_printk("SSD1306: the panel did not acknowledge");
		mp_drv_ssd1306_fini(lcd);
		return(FALSE);
	}
	if(_ssd1306_setup(lcd) == FALSE) {
		mp_printk("SSD1306: the panel did not take the setup");
		mp_drv_ssd1306_fini(lcd);
		return(FALSE);
	}

	_ssd1306_picture(lcd);
	lcd->on = 1;
	if(mp_drv_ssd1306_flush(lcd) == FALSE) {
		mp_printk("SSD1306: the picture did not reach the panel");
		mp_drv_ssd1306_fini(lcd);
		return(FALSE);
	}
	command = 0xAF;
	if(!_ssd1306_command(lcd, &command, 1)) {
		mp_printk("SSD1306: the panel did not turn on");
		mp_drv_ssd1306_fini(lcd);
		return(FALSE);
	}

#ifdef SUPPORT_COMMON_DEVICE
	mp_device_identify(lcd, "SSD1306 OLED", "SSD1306", mp_machine_serial(), NULL);
#endif
	mp_printk("SSD1306 ready, %s", who);
	return(TRUE);
}

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _ssd1306_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_ssd1306_init(kernel, self, options, who));
}

static void _ssd1306_device_close(void *self) {
	mp_drv_ssd1306_fini(self);
}

static void _ssd1306_device_set(void *self, int value) {
	if(mp_drv_ssd1306_power(self, value) == FALSE)
		mp_printk("SSD1306: power did not take");
}

mp_i2c_bus_t *mp_drv_ssd1306_i2c(mp_drv_ssd1306_t *lcd) {
	if(!lcd)
		return(NULL);
	return(&lcd->bus);
}

#ifdef SUPPORT_COMMON_PUI

/* 1 bit per pixel, row-major, MSB first. Bit 1 is lit. gram bit (y & 7) is the pixel in its page. */
static void _ssd1306_pui_write(mp_pui_display_t *display, int x, int y, int w, int h, const unsigned char *pixels) {
	mp_drv_ssd1306_t *lcd = display->user;
	int row, col, stride;

	if(!lcd || !pixels || w <= 0 || h <= 0)
		return;
	stride = (w + 7) / 8;
	for(row = 0; row < h; row++) {
		int yy = y + row;
		int page, bit;

		if(yy < 0 || yy >= MP_DRV_SSD1306_HEIGHT)
			continue;
		page = yy >> 3;
		bit = 1 << (yy & 7);
		lcd->dirty |= (unsigned char)(1u << page);
		for(col = 0; col < w; col++) {
			int xx = x + col;
			unsigned char *cell;
			int on;

			if(xx < 0 || xx >= MP_DRV_SSD1306_WIDTH)
				continue;
			on = pixels[row * stride + (col >> 3)] & (0x80 >> (col & 7));
			cell = &lcd->gram[page * MP_DRV_SSD1306_WIDTH + xx];
			if(on)
				*cell |= (unsigned char)bit;
			else
				*cell &= (unsigned char)~bit;
		}
	}
}

static void _ssd1306_pui_flush(mp_pui_display_t *display) {
	(void)mp_drv_ssd1306_flush(display->user);
}

static void _ssd1306_pui_power(mp_pui_display_t *display, int on) {
	(void)mp_drv_ssd1306_power(display->user, on);
}

static void *_ssd1306_device_face(void *self) {
	mp_drv_ssd1306_t *lcd = self;

	lcd->pui.width = MP_DRV_SSD1306_WIDTH;
	lcd->pui.height = MP_DRV_SSD1306_HEIGHT;
	lcd->pui.write = _ssd1306_pui_write;
	lcd->pui.flush = _ssd1306_pui_flush;
	lcd->pui.power = _ssd1306_pui_power;
	lcd->pui.backlight = NULL;
	lcd->pui.user = lcd;
	return(&lcd->pui);
}

#endif

const mp_device_ops_t mp_drv_ssd1306_device = {
	"lcd/ssd1306", MP_DEVICE_DISPLAY, _ssd1306_device_open, _ssd1306_device_close,
#ifdef SUPPORT_COMMON_PUI
	_ssd1306_device_face,
#else
	NULL,
#endif
	_ssd1306_device_set,
	"128x64 SSD1306", "1.0.0"
};

#endif

#endif
