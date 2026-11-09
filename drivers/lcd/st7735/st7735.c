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

#ifdef SUPPORT_DRV_LCD_ST7735

#define _ST7735_SLPOUT 0x11
#define _ST7735_SLPIN 0x10
#define _ST7735_DISPOFF 0x28
#define _ST7735_DISPON 0x29
#define _ST7735_CASET 0x2A
#define _ST7735_RASET 0x2B
#define _ST7735_RAMWR 0x2C
#define _ST7735_MADCTL 0x36
#define _ST7735_COLMOD 0x3A
#define _ST7735_FRMCTR1 0xB1
#define _ST7735_INVCTR 0xB4
#define _ST7735_MV 0x20

/* red 0xF800 in PUI order: low byte, then high byte */
#define _ST7735_RED_LO 0x00
#define _ST7735_RED_HI 0xF8

static void _st7735_wait(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz + 999) / 1000 : ms;

	mp_clock_delay((int)(ticks ? ticks : 1));
}

static void _st7735_release(mp_gpio_port_t **pin) {
	if(*pin) {
		mp_gpio_release(*pin);
		*pin = NULL;
	}
}

/* DC low for the command byte, high for the parameters that follow. */
static void _st7735_cmd(mp_drv_st7735_t *lcd, unsigned char command, const unsigned char *data, int count) {
	mp_gpio_unset(lcd->dc);
	mp_spi_write_byte(&lcd->bus, command);
	if(count > 0 && data) {
		mp_gpio_set(lcd->dc);
		mp_spi_tx(&lcd->bus, data, count);
	}
	mp_gpio_set(lcd->dc);
}

/*
 * MV exchanges GRAM axes, so the 24-column offset lands on the rows and the
 * 160-wide face uses CASET with no extra offset. Portrait keeps the 24 on CASET.
 */
static void _st7735_origin(unsigned char madctl, int *ox, int *oy) {
	if(madctl & _ST7735_MV) {
		*ox = MP_DRV_ST7735_OFFSET_Y;
		*oy = MP_DRV_ST7735_OFFSET_X;
	}
	else {
		*ox = MP_DRV_ST7735_OFFSET_X;
		*oy = MP_DRV_ST7735_OFFSET_Y;
	}
}

static void _st7735_window(mp_drv_st7735_t *lcd, int x, int y, int w, int h) {
	int ox, oy;
	unsigned int x0, x1, y0, y1;
	unsigned char column[4];
	unsigned char row[4];

	_st7735_origin(lcd->madctl, &ox, &oy);
	x0 = (unsigned int)(x + ox);
	x1 = (unsigned int)(x + w - 1 + ox);
	y0 = (unsigned int)(y + oy);
	y1 = (unsigned int)(y + h - 1 + oy);
	column[0] = (unsigned char)(x0 >> 8);
	column[1] = (unsigned char)x0;
	column[2] = (unsigned char)(x1 >> 8);
	column[3] = (unsigned char)x1;
	row[0] = (unsigned char)(y0 >> 8);
	row[1] = (unsigned char)y0;
	row[2] = (unsigned char)(y1 >> 8);
	row[3] = (unsigned char)y1;
	_st7735_cmd(lcd, _ST7735_CASET, column, 4);
	_st7735_cmd(lcd, _ST7735_RASET, row, 4);
	_st7735_cmd(lcd, _ST7735_RAMWR, NULL, 0);
}

/*
 * PUI stores RGB565 low byte first. The panel clocks the high byte first.
 * count is the number of pixels.
 */
static void _st7735_pixels(mp_drv_st7735_t *lcd, const unsigned char *pixels, int count) {
	unsigned char chunk[64];
	int done;

	mp_gpio_set(lcd->dc);
	done = 0;
	while(done < count) {
		int n = count - done;
		int index;

		if(n > 32)
			n = 32;
		for(index = 0; index < n; index++) {
			chunk[index * 2] = pixels[(done + index) * 2 + 1];
			chunk[index * 2 + 1] = pixels[(done + index) * 2];
		}
		mp_spi_tx(&lcd->bus, chunk, n * 2);
		done += n;
	}
}

static void _st7735_fill(mp_drv_st7735_t *lcd, unsigned char lo, unsigned char hi) {
	unsigned char row[MP_DRV_ST7735_WIDTH * 2];
	int x, y;

	for(x = 0; x < MP_DRV_ST7735_WIDTH; x++) {
		row[x * 2] = lo;
		row[x * 2 + 1] = hi;
	}
	_st7735_window(lcd, 0, 0, MP_DRV_ST7735_WIDTH, MP_DRV_ST7735_HEIGHT);
	for(y = 0; y < MP_DRV_ST7735_HEIGHT; y++)
		_st7735_pixels(lcd, row, MP_DRV_ST7735_WIDTH);
}

static unsigned char _st7735_madctl(char *text) {
	if(text && strcmp(text, "flip") == 0)
		return(MP_DRV_ST7735_MADCTL_FLIP);
	if(text && strcmp(text, "inverted") == 0)
		return(MP_DRV_ST7735_MADCTL_INVERTED);
	return(MP_DRV_ST7735_MADCTL_NATIVE);
}

static void _st7735_reset(mp_drv_st7735_t *lcd) {
	mp_gpio_mode(lcd->reset, MP_GPIO_MODE_OUTPUT);
	mp_gpio_unset(lcd->reset);
	_st7735_wait(10);
	mp_gpio_set(lcd->reset);
	_st7735_wait(120);
}

static mp_ret_t _st7735_setup(mp_drv_st7735_t *lcd) {
	unsigned char madctl;
	unsigned char colmod;
	unsigned char invctr;
	unsigned char frame[2];

	_st7735_cmd(lcd, _ST7735_SLPOUT, NULL, 0);
	_st7735_wait(120);
	madctl = lcd->madctl;
	colmod = 0x05;
	invctr = 0x01;
	frame[0] = 0x00;
	frame[1] = 0x10;
	_st7735_cmd(lcd, _ST7735_MADCTL, &madctl, 1);
	_st7735_cmd(lcd, _ST7735_COLMOD, &colmod, 1);
	_st7735_cmd(lcd, _ST7735_INVCTR, &invctr, 1);
	_st7735_cmd(lcd, _ST7735_FRMCTR1, frame, 2);
	_st7735_cmd(lcd, _ST7735_DISPON, NULL, 0);
	return(TRUE);
}

mp_ret_t mp_drv_st7735_power(mp_drv_st7735_t *lcd, int on) {
	if(!lcd || !lcd->bus.sclk || !lcd->dc)
		return(FALSE);
	if(on) {
		_st7735_cmd(lcd, _ST7735_SLPOUT, NULL, 0);
		_st7735_wait(120);
		_st7735_cmd(lcd, _ST7735_DISPON, NULL, 0);
		lcd->on = 1;
		return(TRUE);
	}
	_st7735_cmd(lcd, _ST7735_DISPOFF, NULL, 0);
	_st7735_cmd(lcd, _ST7735_SLPIN, NULL, 0);
	lcd->on = 0;
	return(TRUE);
}

mp_ret_t mp_drv_st7735_fini(mp_drv_st7735_t *lcd) {
	if(!lcd)
		return(TRUE);
	if(lcd->on && lcd->bus.sclk && lcd->dc)
		mp_drv_st7735_power(lcd, 0);
	lcd->on = 0;
	_st7735_release(&lcd->dc);
	_st7735_release(&lcd->reset);
	mp_spi_close(&lcd->bus);
	return(TRUE);
}

mp_ret_t mp_drv_st7735_init(mp_kernel_t *kernel, mp_drv_st7735_t *lcd, mp_options_t *options, char *who) {
	char *dc, *reset;

	memset(lcd, 0, sizeof(*lcd));
	lcd->kernel = kernel;
	if(!who || !who[0])
		who = "st7735";
	lcd->madctl = _st7735_madctl(mp_options_get(options, "orientation"));

	if(!mp_spi_open(kernel, &lcd->bus, options, who)) {
		mp_printk("ST7735: SCK and MOSI are required");
		mp_drv_st7735_fini(lcd);
		return(FALSE);
	}
	/* the ST7735S takes a 15 MHz write clock, so let the bit-bang run at the
	 * GPIO-toggle limit instead of the 100 kHz default */
	mp_spi_setup(&lcd->bus, 0);

	dc = mp_options_get(options, "dc");
	if(!dc || !dc[0]) {
		mp_printk("ST7735: DC is required");
		mp_drv_st7735_fini(lcd);
		return(FALSE);
	}
	lcd->dc = mp_gpio_text_handle(dc, who);
	if(!lcd->dc) {
		mp_printk("ST7735: DC is not free");
		mp_drv_st7735_fini(lcd);
		return(FALSE);
	}
	mp_gpio_mode(lcd->dc, MP_GPIO_MODE_OUTPUT);
	mp_gpio_set(lcd->dc);

	reset = mp_options_get(options, "reset");
	if(reset && reset[0]) {
		lcd->reset = mp_gpio_text_handle(reset, who);
		if(!lcd->reset) {
			mp_printk("ST7735: reset is not free");
			mp_drv_st7735_fini(lcd);
			return(FALSE);
		}
		_st7735_reset(lcd);
	}

	if(_st7735_setup(lcd) == FALSE) {
		mp_printk("ST7735: the panel did not take the setup");
		mp_drv_st7735_fini(lcd);
		return(FALSE);
	}
	lcd->on = 1;
	_st7735_fill(lcd, _ST7735_RED_LO, _ST7735_RED_HI);

#ifdef SUPPORT_COMMON_DEVICE
	mp_device_identify(lcd, "ST7735 TFT", "ST7735", mp_machine_serial(), NULL);
#endif
	mp_printk("ST7735 ready, %s", who);
	return(TRUE);
}

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _st7735_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_st7735_init(kernel, self, options, who));
}

static void _st7735_device_close(void *self) {
	mp_drv_st7735_fini(self);
}

static void _st7735_device_set(void *self, int value) {
	if(mp_drv_st7735_power(self, value) == FALSE)
		mp_printk("ST7735: power did not take");
}

#ifdef SUPPORT_COMMON_PUI

static void _st7735_pui_write(mp_pui_display_t *display, int x, int y, int w, int h, const unsigned char *pixels) {
	mp_drv_st7735_t *lcd = display->user;

	if(!lcd || !pixels || w <= 0 || h <= 0)
		return;
	if(x < 0 || y < 0 || x + w > MP_DRV_ST7735_WIDTH || y + h > MP_DRV_ST7735_HEIGHT)
		return;
	_st7735_window(lcd, x, y, w, h);
	_st7735_pixels(lcd, pixels, w * h);
}

static void _st7735_pui_flush(mp_pui_display_t *display) {
	(void)display;
}

static void _st7735_pui_power(mp_pui_display_t *display, int on) {
	(void)mp_drv_st7735_power(display->user, on);
}

static void *_st7735_device_face(void *self) {
	mp_drv_st7735_t *lcd = self;

	lcd->pui.width = MP_DRV_ST7735_WIDTH;
	lcd->pui.height = MP_DRV_ST7735_HEIGHT;
	lcd->pui.bpp = 16;
	lcd->pui.write = _st7735_pui_write;
	lcd->pui.flush = _st7735_pui_flush;
	lcd->pui.power = _st7735_pui_power;
	lcd->pui.backlight = NULL;
	lcd->pui.user = lcd;
	return(&lcd->pui);
}

#endif

const mp_device_ops_t mp_drv_st7735_device = {
	"lcd/st7735", MP_DEVICE_DISPLAY, _st7735_device_open, _st7735_device_close,
#ifdef SUPPORT_COMMON_PUI
	_st7735_device_face,
#else
	NULL,
#endif
	_st7735_device_set,
	"160x80 ST7735", "1.0.0"
};

#endif

#endif
