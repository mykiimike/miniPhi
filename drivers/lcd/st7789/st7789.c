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

#ifdef SUPPORT_DRV_LCD_ST7789

#define _ST7789_SWRESET 0x01
#define _ST7789_SLPOUT 0x11
#define _ST7789_NORON 0x13
#define _ST7789_INVON 0x21
#define _ST7789_DISPOFF 0x28
#define _ST7789_DISPON 0x29
#define _ST7789_CASET 0x2A
#define _ST7789_RASET 0x2B
#define _ST7789_RAMWR 0x2C
#define _ST7789_MADCTL 0x36
#define _ST7789_COLMOD 0x3A

/* MX | MV | ML: the Waveshare 1.47 landscape scan */
#define _ST7789_MADCTL_LANDSCAPE 0x70
#define _ST7789_MADCTL_PORTRAIT 0x00
/* MY | MV */
#define _ST7789_MADCTL_LANDSCAPE_FLIP 0xA0
/* MY | MX */
#define _ST7789_MADCTL_PORTRAIT_FLIP 0xC0
/* the 240x320 glass is wired BGR */
#define _ST7789_MADCTL_BGR 0x08

/* RGB565 black: the GRAM holds noise after reset */
#define _ST7789_BLACK 0x0000

/* Cycle counter, not the tick. A wait that sleeps on SysTick never returns
 * when the tick is not running, and the glass stays black with the
 * backlight already on. */
static void _st7789_wait(unsigned long ms) {
	unsigned long hz = mp_clock_get_speed();
	unsigned long cycles;
	unsigned long start;

	if(hz < 1000000UL)
		hz = 48000000UL;
	cycles = (hz / 1000UL) * ms;
	start = DWT->CYCCNT;
	if(DWT->CYCCNT != start) {
		while((DWT->CYCCNT - start) < cycles);
		return;
	}
	while(cycles > 4) {
		__asm volatile("nop");
		cycles -= 4;
	}
}

static void _st7789_release(mp_gpio_port_t **pin) {
	if(*pin) {
		mp_gpio_release(*pin);
		*pin = NULL;
	}
}

/* Direct SIO: the pins are driven without the GPIO layer in the way. */
static void _st7789_level(int pin, int high) {
	if(pin < 0)
		return;
	if(high)
		SIO_GPIO_OUT_SET = 1UL << pin;
	else
		SIO_GPIO_OUT_CLR = 1UL << pin;
}

static void _st7789_drive(int pin, int high) {
	if(pin < 0)
		return;
	IO_BANK0_GPIO_CTRL(pin) = IO_GPIO_FUNCSEL_SIO;
	PADS_BANK0_GPIO(pin) = (PADS_SCHMITT | PADS_DRIVE_4) & ~PADS_ISO;
	SIO_GPIO_OE_SET = 1UL << pin;
	_st7789_level(pin, high);
}

/* SPI0 SCK is GP18 and SPI0 TX is GP19, the pins of this glass. The
 * peripheral shifts a byte per FIFO write; the bitbang did it in software. */
static int _st7789_hw;

static void _st7789_spi_up(int sck, int mosi) {
	unsigned int spins = 1000000;

	_st7789_hw = 0;
	if(sck != 18 || mosi != 19)
		return;

	RESETS_RESET &= ~RESETS_SPI0;
	while(spins-- && !(RESETS_RESET_DONE & RESETS_SPI0));
	/* clk_peri stays on PLL_USB so the governor can move clk_sys
	 * without retiming the glass */
	CLOCKS_CLK_PERI_CTRL = CLOCKS_PERI_ENABLE | (CLOCKS_PERI_AUX_USB << 5);

	IO_BANK0_GPIO_CTRL(sck) = IO_GPIO_FUNCSEL_SPI;
	IO_BANK0_GPIO_CTRL(mosi) = IO_GPIO_FUNCSEL_SPI;

	SPI0->CR1 = 0;
	{
		unsigned long peri = mp_clock_peri();
		unsigned long div;

		if(peri < 2000000UL)
			peri = 48000000UL;
		div = (peri + 24000000UL - 1UL) / 24000000UL;
		if(div < 2)
			div = 2;
		if(div & 1UL)
			div++;
		SPI0->CPSR = (uint32_t)div;
	}
	SPI0->CR0 = SPI_CR0_DSS_8;
	SPI0->CR1 = SPI_CR1_SSE;
	_st7789_hw = 1;
}

static void _st7789_spi_idle(void) {
	unsigned int spins = 1000000;

	while(spins-- && (SPI0->SR & SPI_SR_BSY));
	while(SPI0->SR & SPI_SR_RNE)
		(void)SPI0->DR;
}

static void _st7789_byte_pins(int sck, int mosi, unsigned char byte) {
	int bit;

	for(bit = 7; bit >= 0; bit--) {
		_st7789_level(mosi, (byte >> bit) & 1);
		_st7789_level(sck, 1);
		_st7789_level(sck, 0);
	}
}

static void _st7789_tx(mp_drv_st7789_t *lcd, const unsigned char *data, int count) {
	int sent = 0;
	int got = 0;
	unsigned int spins = 1000000;

	if(_st7789_hw) {
		while(got < count && spins) {
			/* at most 8 bytes in flight: the RX FIFO holds 8, an overrun
			 * drops a byte and this loop waits out its spins */
			if(sent < count && sent - got < 8 && (SPI0->SR & SPI_SR_TNF))
				SPI0->DR = data[sent++];
			if(SPI0->SR & SPI_SR_RNE) {
				(void)SPI0->DR;
				got++;
				spins = 1000000;
			}
			else
				spins--;
		}
		_st7789_spi_idle();
		return;
	}
	for(sent = 0; sent < count; sent++)
		_st7789_byte_pins(lcd->n_sck, lcd->n_mosi, data[sent]);
}

static void _st7789_byte(mp_drv_st7789_t *lcd, unsigned char byte) {
	_st7789_tx(lcd, &byte, 1);
}

static void _st7789_cs(mp_drv_st7789_t *lcd, int active) {
	_st7789_level(lcd->n_cs, active ? 0 : 1);
}

static void _st7789_dc(mp_drv_st7789_t *lcd, int data) {
	_st7789_level(lcd->n_dc, data ? 1 : 0);
}

/* One RAMWR for a full top-to-bottom pass. A fresh CASET/RASET on every
 * row makes this glass drop rows. */
static mp_drv_st7789_t *_st7789_stream_lcd;
static int _st7789_stream_y;

static void _st7789_stream_end(void) {
	if(!_st7789_stream_lcd)
		return;
	_st7789_cs(_st7789_stream_lcd, 0);
	_st7789_stream_lcd = NULL;
}

static void _st7789_cmd(mp_drv_st7789_t *lcd, unsigned char command, const unsigned char *data, int count) {
	_st7789_stream_end();
	_st7789_cs(lcd, 1);
	_st7789_dc(lcd, 0);
	_st7789_byte(lcd, command);
	if(count > 0 && data) {
		_st7789_dc(lcd, 1);
		_st7789_tx(lcd, data, count);
	}
	_st7789_dc(lcd, 1);
	_st7789_cs(lcd, 0);
}

static void _st7789_face(char *orientation, int bgr, int columns, int rows, int offset, unsigned char *madctl, int *width, int *height, int *xoff, int *yoff) {
	unsigned char color = bgr ? _ST7789_MADCTL_BGR : 0;

	if(orientation && mp_options_cmp(orientation, "portrait") == TRUE) {
		*madctl = (unsigned char)(_ST7789_MADCTL_PORTRAIT | color);
		*width = columns;
		*height = rows;
		*xoff = offset;
		*yoff = 0;
	}
	else if(orientation && mp_options_cmp(orientation, "portrait-flipped") == TRUE) {
		*madctl = (unsigned char)(_ST7789_MADCTL_PORTRAIT_FLIP | color);
		*width = columns;
		*height = rows;
		*xoff = offset;
		*yoff = 0;
	}
	else if(orientation && mp_options_cmp(orientation, "landscape-flipped") == TRUE) {
		*madctl = (unsigned char)(_ST7789_MADCTL_LANDSCAPE_FLIP | color);
		*width = rows;
		*height = columns;
		*xoff = 0;
		*yoff = offset;
	}
	else {
		*madctl = (unsigned char)(_ST7789_MADCTL_LANDSCAPE | color);
		*width = rows;
		*height = columns;
		*xoff = 0;
		*yoff = offset;
	}
}

/* 172x320 is the centered IPS glass. 240x320 fills the RAM and is BGR. */
static int _st7789_glass(mp_drv_st7789_t *lcd, char *glass, char *orientation) {
	int columns, rows, offset, bgr;

	if(!glass || !glass[0] || mp_options_cmp(glass, "172x320") == TRUE) {
		columns = MP_DRV_ST7789_SHORT;
		rows = MP_DRV_ST7789_LONG;
		offset = MP_DRV_ST7789_OFFSET;
		bgr = 0;
		/* 0x55 leaves this glass blank */
		lcd->colmod = 0x05;
		lcd->invert = 1;
	}
	else if(mp_options_cmp(glass, "240x320") == TRUE) {
		columns = MP_DRV_ST7789_GRAM;
		rows = MP_DRV_ST7789_LONG;
		offset = 0;
		bgr = 1;
		lcd->colmod = 0x55;
		lcd->invert = 0;
	}
	else {
		mp_printk("ST7789: glass %s is not 172x320 or 240x320", glass);
		return(0);
	}
	_st7789_face(orientation, bgr, columns, rows, offset, &lcd->madctl, &lcd->width, &lcd->height, &lcd->xoff, &lcd->yoff);
	return(1);
}

static void _st7789_window(mp_drv_st7789_t *lcd, int x, int y, int w, int h) {
	unsigned int x0 = (unsigned int)(x + lcd->xoff);
	unsigned int x1 = (unsigned int)(x + w - 1 + lcd->xoff);
	unsigned int y0 = (unsigned int)(y + lcd->yoff);
	unsigned int y1 = (unsigned int)(y + h - 1 + lcd->yoff);
	unsigned char column[4];
	unsigned char row[4];

	column[0] = (unsigned char)(x0 >> 8);
	column[1] = (unsigned char)x0;
	column[2] = (unsigned char)(x1 >> 8);
	column[3] = (unsigned char)x1;
	row[0] = (unsigned char)(y0 >> 8);
	row[1] = (unsigned char)y0;
	row[2] = (unsigned char)(y1 >> 8);
	row[3] = (unsigned char)y1;
	_st7789_cmd(lcd, _ST7789_CASET, column, 4);
	_st7789_cmd(lcd, _ST7789_RASET, row, 4);

	_st7789_cs(lcd, 1);
	_st7789_dc(lcd, 0);
	_st7789_byte(lcd, _ST7789_RAMWR);
	_st7789_dc(lcd, 1);
}

static void _st7789_chunk(mp_drv_st7789_t *lcd, const uint16_t *pixels, int count) {
	unsigned char chunk[64];
	int done = 0;

	while(done < count) {
		int n = count - done;
		int index;

		if(n > 32)
			n = 32;
		for(index = 0; index < n; index++) {
			uint16_t color = pixels[done + index];

			chunk[index * 2] = (unsigned char)(color >> 8);
			chunk[index * 2 + 1] = (unsigned char)color;
		}
		_st7789_tx(lcd, chunk, n * 2);
		done += n;
	}
}

static void _st7789_fill(mp_drv_st7789_t *lcd, uint16_t color) {
	int count = lcd->width * lcd->height;
	uint16_t row[32];
	int index;

	for(index = 0; index < 32; index++)
		row[index] = color;
	_st7789_window(lcd, 0, 0, lcd->width, lcd->height);
	while(count > 0) {
		int n = count > 32 ? 32 : count;

		_st7789_chunk(lcd, row, n);
		count -= n;
	}
	_st7789_cs(lcd, 0);
}

/* Porch, power and gamma. Both glasses use this sequence; the window,
 * the color order and the inversion are what glass changes. */
static void _st7789_panel(mp_drv_st7789_t *lcd) {
	static const unsigned char seq[] = {
		0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
		0xB7, 1, 0x35,
		0xBB, 1, 0x35,
		0xC0, 1, 0x2C,
		0xC2, 1, 0x01,
		0xC3, 1, 0x13,
		0xC4, 1, 0x20,
		0xC6, 1, 0x0F,
		0xD0, 2, 0xA4, 0xA1,
		0xD6, 1, 0xA1,
		0xE0, 14, 0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30,
		0xE1, 14, 0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32,
	};
	unsigned int at = 0;

	while(at < sizeof(seq)) {
		unsigned char count = seq[at + 1];

		_st7789_cmd(lcd, seq[at], &seq[at + 2], count);
		at += 2U + count;
	}
}

static int _st7789_setup(mp_drv_st7789_t *lcd) {
	unsigned char colmod = lcd->colmod;

	_st7789_drive(lcd->n_cs, 1);
	_st7789_drive(lcd->n_dc, 1);
	_st7789_drive(lcd->n_sck, 0);
	_st7789_drive(lcd->n_mosi, 0);
	_st7789_drive(lcd->n_rst, 1);
	/* off until the first full frame is in GRAM, so the paint is not a sweep */
	_st7789_drive(lcd->n_bl, 0);
	_st7789_spi_up(lcd->n_sck, lcd->n_mosi);
	if(lcd->n_rst >= 0) {
		_st7789_level(lcd->n_rst, 0);
		_st7789_wait(10);
		_st7789_level(lcd->n_rst, 1);
		_st7789_wait(120);
	}

	_st7789_cmd(lcd, _ST7789_SWRESET, NULL, 0);
	_st7789_wait(150);
	_st7789_cmd(lcd, _ST7789_SLPOUT, NULL, 0);
	_st7789_wait(120);
	_st7789_cmd(lcd, _ST7789_COLMOD, &colmod, 1);
	_st7789_cmd(lcd, _ST7789_MADCTL, &lcd->madctl, 1);
	_st7789_panel(lcd);
	/* the 172-wide IPS glass is inverted; the 240x320 glass is not */
	if(lcd->invert)
		_st7789_cmd(lcd, _ST7789_INVON, NULL, 0);
	_st7789_cmd(lcd, _ST7789_NORON, NULL, 0);
	_st7789_cmd(lcd, _ST7789_DISPON, NULL, 0);
	_st7789_wait(20);
	return(1);
}

static void _st7789_percent(mp_drv_st7789_t *lcd, int percent) {
	_st7789_drive(lcd->n_bl, percent > 0);
}

mp_ret_t mp_drv_st7789_power(mp_drv_st7789_t *lcd, int on) {
	if(!lcd || !lcd->dc)
		return(FALSE);
	if(on) {
		_st7789_cmd(lcd, _ST7789_DISPON, NULL, 0);
		_st7789_percent(lcd, 100);
		lcd->on = 1;
	}
	else {
		_st7789_cmd(lcd, _ST7789_DISPOFF, NULL, 0);
		_st7789_percent(lcd, 0);
		lcd->on = 0;
	}
	return(TRUE);
}

void mp_drv_st7789_backlight(mp_drv_st7789_t *lcd, int percent) {
	_st7789_percent(lcd, percent);
}

mp_ret_t mp_drv_st7789_fini(mp_drv_st7789_t *lcd) {
	if(lcd->on && lcd->dc)
		mp_drv_st7789_power(lcd, 0);
	lcd->on = 0;
	_st7789_release(&lcd->dc);
	_st7789_release(&lcd->cs);
	_st7789_release(&lcd->reset);
	_st7789_release(&lcd->backlight);
	mp_spi_close(&lcd->bus);
	return(TRUE);
}

static mp_gpio_port_t *_st7789_pin(mp_drv_st7789_t *lcd, mp_options_t *options, char *key, char *who, int output, int level) {
	char *text = mp_options_get(options, key);
	mp_gpio_port_t *pin;

	if(!text || !text[0])
		return(NULL);
	pin = mp_gpio_text_handle(text, who);
	if(!pin) {
		mp_printk("ST7789: %s is not free", key);
		mp_drv_st7789_fini(lcd);
		return(NULL);
	}
	if(output) {
		mp_gpio_mode(pin, MP_GPIO_MODE_OUTPUT);
		if(level)
			mp_gpio_set(pin);
		else
			mp_gpio_unset(pin);
	}
	return(pin);
}

mp_ret_t mp_drv_st7789_init(mp_kernel_t *kernel, mp_drv_st7789_t *lcd, mp_options_t *options, char *who) {
	char *dc;

	memset(lcd, 0, sizeof(*lcd));
	lcd->n_dc = lcd->n_cs = lcd->n_sck = lcd->n_mosi = lcd->n_rst = lcd->n_bl = -1;
	lcd->kernel = kernel;
	if(!who || !who[0])
		who = "st7789";
	if(!_st7789_glass(lcd, mp_options_get(options, "glass"), mp_options_get(options, "orientation"))) {
		mp_drv_st7789_fini(lcd);
		return(FALSE);
	}

	if(!mp_spi_open(kernel, &lcd->bus, options, who)) {
		mp_printk("ST7789: SCK and MOSI are required");
		mp_drv_st7789_fini(lcd);
		return(FALSE);
	}
	mp_spi_setup(&lcd->bus, 0);

	dc = mp_options_get(options, "dc");
	if(!dc || !dc[0]) {
		mp_printk("ST7789: DC is required");
		mp_drv_st7789_fini(lcd);
		return(FALSE);
	}
	lcd->dc = mp_gpio_text_handle(dc, who);
	if(!lcd->dc) {
		mp_printk("ST7789: DC is not free");
		mp_drv_st7789_fini(lcd);
		return(FALSE);
	}
	mp_gpio_mode(lcd->dc, MP_GPIO_MODE_OUTPUT);
	mp_gpio_set(lcd->dc);

	lcd->cs = _st7789_pin(lcd, options, "cs", who, 1, 1);
	if(mp_options_get(options, "cs") && mp_options_get(options, "cs")[0] && !lcd->cs)
		return(FALSE);
	lcd->reset = _st7789_pin(lcd, options, "reset", who, 1, 1);
	if(mp_options_get(options, "reset") && mp_options_get(options, "reset")[0] && !lcd->reset)
		return(FALSE);
	lcd->backlight = _st7789_pin(lcd, options, "backlight", who, 1, 1);
	if(mp_options_get(options, "backlight") && mp_options_get(options, "backlight")[0] && !lcd->backlight)
		return(FALSE);
	lcd->n_sck = lcd->bus.sclk ? (int)lcd->bus.sclk->pin : -1;
	lcd->n_mosi = lcd->bus.mosi ? (int)lcd->bus.mosi->pin : -1;
	lcd->n_dc = lcd->dc ? (int)lcd->dc->pin : -1;
	lcd->n_cs = lcd->cs ? (int)lcd->cs->pin : -1;
	lcd->n_rst = lcd->reset ? (int)lcd->reset->pin : -1;
	lcd->n_bl = lcd->backlight ? (int)lcd->backlight->pin : -1;
	_st7789_percent(lcd, 0);

	if(!_st7789_setup(lcd)) {
		mp_drv_st7789_fini(lcd);
		return(FALSE);
	}
	lcd->on = 1;
	_st7789_fill(lcd, _ST7789_BLACK);
	_st7789_percent(lcd, 0);

#ifdef SUPPORT_COMMON_DEVICE
	mp_device_identify(lcd, "ST7789 TFT", "ST7789V3", mp_machine_serial(), NULL);
#endif
	mp_printk("ST7789 ready, %dx%d, %s", lcd->width, lcd->height, who);
	return(TRUE);
}

#ifdef SUPPORT_COMMON_PUI

static void _st7789_pui_write(mp_pui_display_t *display, int x, int y, int w, int h, const unsigned char *pixels) {
	mp_drv_st7789_t *lcd = display->user;

	if(!lcd || !pixels || w <= 0 || h <= 0)
		return;
	if(x < 0 || y < 0 || x + w > lcd->width || y + h > lcd->height)
		return;
	/* continue the open frame: the controller already has the window */
	if(_st7789_stream_lcd == lcd && y == _st7789_stream_y && x == 0 && w == lcd->width && h == 1) {
		_st7789_chunk(lcd, (const uint16_t *)pixels, w);
		_st7789_stream_y = y + 1;
		if(_st7789_stream_y >= lcd->height)
			_st7789_stream_end();
		return;
	}
	_st7789_stream_end();
	/* a full-width row opens the window down to the bottom, so the rows
	 * that follow land in their own GRAM line instead of wrapping in the
	 * one-row window of this call */
	if(x == 0 && w == lcd->width && h == 1)
		_st7789_window(lcd, 0, y, lcd->width, lcd->height - y);
	else
		_st7789_window(lcd, x, y, w, h);
	_st7789_chunk(lcd, (const uint16_t *)pixels, w * h);
	if(x == 0 && w == lcd->width && h == 1 && y + 1 < lcd->height) {
		_st7789_stream_lcd = lcd;
		_st7789_stream_y = y + 1;
		return;
	}
	_st7789_cs(lcd, 0);
}

static void _st7789_pui_power(mp_pui_display_t *display, int on) {
	(void)mp_drv_st7789_power(display->user, on);
}

static void _st7789_pui_backlight(mp_pui_display_t *display, int percent) {
	_st7789_percent(display->user, percent);
}

#endif

#ifdef SUPPORT_COMMON_TUI

static void _st7789_write(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *pixels) {
	mp_drv_st7789_t *lcd = display->user;

	if(!lcd || !pixels || w <= 0 || h <= 0)
		return;
	if(x < 0 || y < 0 || x + w > lcd->width || y + h > lcd->height)
		return;
	_st7789_window(lcd, x, y, w, h);
	_st7789_chunk(lcd, pixels, w * h);
	_st7789_cs(lcd, 0);
}

static void _st7789_tui_backlight(mp_tui_display_t *display, int percent) {
	_st7789_percent(display->user, percent);
}

static void _st7789_tui_power(mp_tui_display_t *display, int on) {
	(void)mp_drv_st7789_power(display->user, on);
}

#endif

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _st7789_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_st7789_init(kernel, self, options, who));
}

static void _st7789_device_close(void *self) {
	mp_drv_st7789_fini(self);
}

static void _st7789_device_set(void *self, int value) {
	if(mp_drv_st7789_power(self, value) == FALSE)
		mp_printk("ST7789: power did not take");
}

static void *_st7789_device_face(void *self) {
	mp_drv_st7789_t *lcd = self;

#ifdef SUPPORT_COMMON_PUI
	lcd->pui.width = lcd->width;
	lcd->pui.height = lcd->height;
	lcd->pui.bpp = 16;
	lcd->pui.write = _st7789_pui_write;
	lcd->pui.flush = NULL;
	lcd->pui.power = _st7789_pui_power;
	lcd->pui.backlight = _st7789_pui_backlight;
	lcd->pui.user = lcd;
	return(&lcd->pui);
#elif defined(SUPPORT_COMMON_TUI)
	lcd->tui.width = lcd->width;
	lcd->tui.height = lcd->height;
	lcd->tui.write = _st7789_write;
	lcd->tui.flush = NULL;
	lcd->tui.backlight = _st7789_tui_backlight;
	lcd->tui.power = _st7789_tui_power;
	lcd->tui.user = lcd;
	return(&lcd->tui);
#else
	(void)lcd;
	return(NULL);
#endif
}

const mp_device_ops_t mp_drv_st7789_device = {
	"lcd/st7789", MP_DEVICE_DISPLAY, _st7789_device_open, _st7789_device_close, _st7789_device_face, _st7789_device_set,
	"ST7789 TFT", "1.0.0"
};

#endif

#endif
