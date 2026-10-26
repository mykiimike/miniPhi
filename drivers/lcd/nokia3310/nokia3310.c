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

#ifdef SUPPORT_DRV_LCD_NOKIA3310

#define _NOKIA_W 84
#define _NOKIA_H 48
#define _NOKIA_PAGES 6

/* "miniPhi" in 5x7 columns, a 0 between letters. Bit 0 is the top pixel. */
static const unsigned char _nokia_name[] = {
	0x7C, 0x04, 0x18, 0x04, 0x78, 0,
	0x00, 0x44, 0x7D, 0x40, 0x00, 0,
	0x7C, 0x08, 0x04, 0x04, 0x78, 0,
	0x00, 0x44, 0x7D, 0x40, 0x00, 0,
	0x7F, 0x09, 0x09, 0x09, 0x06, 0,
	0x7F, 0x08, 0x04, 0x04, 0x78, 0,
	0x00, 0x44, 0x7D, 0x40, 0x00,
};

static mp_ret_t _nokia_shift(mp_drv_nokia3310_t *lcd, unsigned char data) {
	unsigned int spin;

	spin = 0;
	while(!(_SPI_REG8(lcd->spi.gate, _SPI_IFG) & UCTXIFG)) {
		if(++spin > 10000)
			return(FALSE);
	}
	mp_spi_tx(&lcd->spi, data);
	spin = 0;
	while(_SPI_REG8(lcd->spi.gate, _SPI_STATW) & UCBUSY) {
		if(++spin > 10000)
			return(FALSE);
	}
	(void)mp_spi_rx(&lcd->spi);
	return(TRUE);
}

/* #SCE rises only once the shifter is idle, so D/C cannot change mid-byte */
static mp_ret_t _nokia_command(mp_drv_nokia3310_t *lcd, unsigned char command) {
	mp_ret_t ret;

	mp_gpio_unset(lcd->dc);
	mp_gpio_unset(lcd->cs);
	ret = _nokia_shift(lcd, command);
	mp_gpio_set(lcd->cs);
	return(ret);
}

static mp_ret_t _nokia_page(mp_drv_nokia3310_t *lcd, int page) {
	int column;

	if(_nokia_command(lcd, 0x80) == FALSE)
		return(FALSE);
	if(_nokia_command(lcd, (unsigned char)(0x40 | page)) == FALSE)
		return(FALSE);
	mp_gpio_set(lcd->dc);
	mp_gpio_unset(lcd->cs);
	for(column = 0; column < _NOKIA_W; column++) {
		if(_nokia_shift(lcd, lcd->gram[page * _NOKIA_W + column]) == FALSE) {
			mp_gpio_set(lcd->cs);
			return(FALSE);
		}
	}
	mp_gpio_set(lcd->cs);
	return(TRUE);
}

static mp_ret_t _nokia_flush(mp_drv_nokia3310_t *lcd) {
	int page;

	if(lcd->on == 0)
		return(TRUE);
	for(page = 0; page < _NOKIA_PAGES; page++) {
		if((lcd->dirty & (1u << page)) == 0)
			continue;
		if(_nokia_page(lcd, page) == FALSE)
			return(FALSE);
	}
	lcd->dirty = 0;
	return(TRUE);
}

static mp_ret_t _nokia_setup(mp_drv_nokia3310_t *lcd) {
	/* extended set, then Vop, temperature 0, bias 4 for a 1:48 mux */
	if(_nokia_command(lcd, 0x21) == FALSE)
		return(FALSE);
	if(_nokia_command(lcd, (unsigned char)(0x80 | lcd->vop)) == FALSE)
		return(FALSE);
	if(_nokia_command(lcd, 0x04) == FALSE)
		return(FALSE);
	if(_nokia_command(lcd, 0x14) == FALSE)
		return(FALSE);
	if(_nokia_command(lcd, 0x20) == FALSE)
		return(FALSE);
	if(_nokia_command(lcd, 0x0C) == FALSE)
		return(FALSE);
	return(TRUE);
}

static void _nokia_picture(mp_drv_nokia3310_t *lcd) {
	int page, column, at;
	unsigned char *row;

	memset(lcd->gram, 0, sizeof(lcd->gram));
	for(column = 0; column < _NOKIA_W; column++) {
		lcd->gram[column] = 0xFF;
		lcd->gram[5 * _NOKIA_W + column] = 0xFF;
	}
	for(page = 1; page <= 4; page++) {
		if(page == 2)
			continue;
		row = lcd->gram + page * _NOKIA_W;
		for(column = 0; column < _NOKIA_W; column++)
			row[column] = (column & 4) ? 0x0F : 0xF0;
	}
	row = lcd->gram + 2 * _NOKIA_W;
	for(column = 0; column < _NOKIA_W; column++)
		row[column] = 0xFF;
	at = (_NOKIA_W - (int)sizeof(_nokia_name)) / 2;
	for(column = 0; column < (int)sizeof(_nokia_name); column++)
		row[at + column] &= (unsigned char)~_nokia_name[column];
	for(page = 0; page < _NOKIA_PAGES; page++) {
		lcd->gram[page * _NOKIA_W] = 0xFF;
		lcd->gram[page * _NOKIA_W + _NOKIA_W - 1] = 0xFF;
	}
	lcd->dirty = (1u << _NOKIA_PAGES) - 1;
}

#ifdef SUPPORT_COMMON_TUI
static void _nokia_write(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *pixels);
static void _nokia_flush_face(mp_tui_display_t *display);
static void _nokia_power_face(mp_tui_display_t *display, int on);
#endif

static mp_gpio_port_t *_nokia_pin(mp_options_t *options, char *key, char *who) {
	char *value = mp_options_get(options, key);

	if(!value)
		return(NULL);
	return(mp_gpio_text_handle(value, who));
}

mp_ret_t mp_drv_nokia3310_init(mp_kernel_t *kernel, mp_drv_nokia3310_t *lcd, mp_options_t *options, char *who) {
	char *contrast;
	unsigned long vop;

	memset(lcd, 0, sizeof(*lcd));
	lcd->kernel = kernel;
	if(!who || !who[0])
		who = "nokia";

	lcd->dc = _nokia_pin(options, "dc", who);
	lcd->reset = _nokia_pin(options, "reset", who);
	lcd->cs = _nokia_pin(options, "cs", who);
	if(!lcd->dc || !lcd->reset || !lcd->cs) {
		mp_printk("Nokia 3310: data/command, reset and chip select are required");
		mp_drv_nokia3310_fini(lcd);
		return(FALSE);
	}

	/* #SCE stays high across the reset, so a clock edge cannot be latched */
	mp_gpio_set(lcd->cs);
	mp_gpio_unset(lcd->dc);
	mp_gpio_set(lcd->reset);
	mp_gpio_direction(lcd->cs, MP_GPIO_OUTPUT);
	mp_gpio_direction(lcd->dc, MP_GPIO_OUTPUT);
	mp_gpio_direction(lcd->reset, MP_GPIO_OUTPUT);

	mp_gpio_unset(lcd->reset);
	mp_clock_delay(2);
	mp_gpio_set(lcd->reset);
	mp_clock_delay(2);

	if(mp_spi_open(kernel, &lcd->spi, options, who) == FALSE) {
		mp_drv_nokia3310_fini(lcd);
		return(FALSE);
	}
	if(mp_spi_setup(&lcd->spi, options) == FALSE) {
		/* setup already released the bus */
		lcd->spi.gate = NULL;
		lcd->spi.simo = NULL;
		lcd->spi.somi = NULL;
		lcd->spi.clk = NULL;
		mp_drv_nokia3310_fini(lcd);
		return(FALSE);
	}

	contrast = mp_options_get(options, "contrast");
	vop = contrast ? (unsigned long)atol(contrast) : 0x48UL;
	if(vop > 0x7FUL)
		vop = 0x48UL;
	lcd->vop = (unsigned char)vop;

	if(_nokia_setup(lcd) == FALSE) {
		mp_printk("Nokia 3310: the panel did not take the setup");
		mp_drv_nokia3310_fini(lcd);
		return(FALSE);
	}

	lcd->on = 1;
	_nokia_picture(lcd);
	if(_nokia_flush(lcd) == FALSE) {
		mp_printk("Nokia 3310: the picture did not reach the panel");
		mp_drv_nokia3310_fini(lcd);
		return(FALSE);
	}

#ifdef SUPPORT_COMMON_TUI
	lcd->display.width = _NOKIA_W;
	lcd->display.height = _NOKIA_H;
	lcd->display.write = _nokia_write;
	lcd->display.flush = _nokia_flush_face;
	lcd->display.power = _nokia_power_face;
	lcd->display.user = lcd;
#endif

#ifdef SUPPORT_COMMON_DEVICE
	mp_device_identify(lcd, "Nokia 3310", "PCD8544", mp_machine_serial(), NULL);
#endif
	mp_printk("Nokia 3310 ready, %s", who);
	return(TRUE);
}

mp_ret_t mp_drv_nokia3310_fini(mp_drv_nokia3310_t *lcd) {
	if(lcd->on && lcd->cs && lcd->spi.gate) {
		_nokia_command(lcd, 0x24);
		lcd->on = 0;
	}
	if(lcd->cs) {
		mp_gpio_release(lcd->cs);
		lcd->cs = NULL;
	}
	if(lcd->dc) {
		mp_gpio_release(lcd->dc);
		lcd->dc = NULL;
	}
	if(lcd->reset) {
		mp_gpio_release(lcd->reset);
		lcd->reset = NULL;
	}
	if(lcd->spi.gate)
		mp_spi_close(&lcd->spi);
	return(TRUE);
}

#ifdef SUPPORT_COMMON_TUI

static int _nokia_ink(mp_tui_color_t color) {
	unsigned r = (color >> 11) & 31;
	unsigned g = (color >> 5) & 63;
	unsigned b = color & 31;

	/* a light source pixel is a dark mark on the pale glass */
	return((r + (g >> 1) + b) >= 32);
}

static void _nokia_mark(mp_drv_nokia3310_t *lcd, int x, int y, int ink) {
	int page = y >> 3;
	unsigned char bit = (unsigned char)(1u << (y & 7));
	unsigned char *cell = &lcd->gram[page * _NOKIA_W + x];

	if(ink)
		*cell |= bit;
	else
		*cell &= (unsigned char)~bit;
	lcd->dirty |= (unsigned char)(1u << page);
}

static void _nokia_write(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *pixels) {
	mp_drv_nokia3310_t *lcd = display->user;
	int stride = w;
	int row, column;

	if(x < 0) {
		pixels += -x;
		w += x;
		x = 0;
	}
	if(y < 0) {
		pixels += (unsigned)(-y) * (unsigned)stride;
		h += y;
		y = 0;
	}
	if(x >= _NOKIA_W || y >= _NOKIA_H || w <= 0 || h <= 0)
		return;
	if(x + w > _NOKIA_W)
		w = _NOKIA_W - x;
	if(y + h > _NOKIA_H)
		h = _NOKIA_H - y;

	for(row = 0; row < h; row++) {
		const mp_tui_color_t *src = pixels + row * stride;

		for(column = 0; column < w; column++)
			_nokia_mark(lcd, x + column, y + row, _nokia_ink(src[column]));
	}
}

static void _nokia_flush_face(mp_tui_display_t *display) {
	_nokia_flush(display->user);
}

static void _nokia_power_face(mp_tui_display_t *display, int on) {
	mp_drv_nokia3310_t *lcd = display->user;

	if(on) {
		_nokia_command(lcd, 0x20);
		_nokia_command(lcd, 0x0C);
		lcd->on = 1;
		lcd->dirty = (1u << _NOKIA_PAGES) - 1;
		_nokia_flush(lcd);
		return;
	}
	_nokia_command(lcd, 0x24);
	lcd->on = 0;
}

#endif

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _nokia_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_nokia3310_init(kernel, self, options, who));
}

static void _nokia_device_close(void *self) {
	mp_drv_nokia3310_fini(self);
}

static void *_nokia_device_face(void *self) {
#ifdef SUPPORT_COMMON_TUI
	return(&((mp_drv_nokia3310_t *)self)->display);
#else
	(void)self;
	return(NULL);
#endif
}

const mp_device_ops_t mp_drv_nokia3310_device = {
	"lcd/nokia3310", MP_DEVICE_DISPLAY, _nokia_device_open, _nokia_device_close, _nokia_device_face, NULL,
	"84x48 PCD8544", "1.0.0"
};

#endif

#endif
