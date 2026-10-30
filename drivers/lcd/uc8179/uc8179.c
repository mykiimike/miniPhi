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

#ifdef SUPPORT_DRV_LCD_UC8179

#define _UC_W MP_DRV_UC8179_WIDTH
#define _UC_H MP_DRV_UC8179_HEIGHT
#define _UC_BYTES (_UC_W * _UC_H / 8)
/* a full refresh is a few seconds; longer than this and the controller is stuck */
#define _UC_BUSY_MS 8000

#ifdef SUPPORT_COMMON_TUI
static void _uc_write(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *pixels);
static void _uc_flush_face(mp_tui_display_t *display);
static void _uc_power_face(mp_tui_display_t *display, int on);
#endif

static mp_gpio_port_t *_uc_pin(mp_options_t *options, char *key, char *who) {
	char *value = mp_options_get(options, key);

	if(!value)
		return(NULL);
	return(mp_gpio_text_handle(value, who));
}

/* mode 0: the controller samples MOSI on the rising edge, clock idles low */
static void _uc_shift(mp_drv_uc8179_t *panel, unsigned char data) {
	int bit;

	for(bit = 7; bit >= 0; bit--) {
		if(data & (unsigned char)(1u << bit))
			mp_gpio_set(panel->mosi);
		else
			mp_gpio_unset(panel->mosi);
		mp_gpio_set(panel->sck);
		mp_gpio_unset(panel->sck);
	}
}

static void _uc_cmd(mp_drv_uc8179_t *panel, unsigned char command) {
	mp_gpio_unset(panel->dc);
	mp_gpio_unset(panel->cs);
	_uc_shift(panel, command);
	mp_gpio_set(panel->cs);
}

static void _uc_data(mp_drv_uc8179_t *panel, unsigned char data) {
	mp_gpio_set(panel->dc);
	mp_gpio_unset(panel->cs);
	_uc_shift(panel, data);
	mp_gpio_set(panel->cs);
}

/* BUSY is high when the controller can take the next command */
static mp_ret_t _uc_wait(mp_drv_uc8179_t *panel) {
	unsigned long start = mp_clock_ticks();

	while((mp_clock_ticks() - start) < _UC_BUSY_MS) {
		_uc_cmd(panel, 0x71);
		if(mp_gpio_read(panel->busy) == YES)
			break;
		mp_clock_delay(1);
	}
	if(mp_gpio_read(panel->busy) == NO) {
		mp_printk("UC8179: the panel stayed busy");
		return(FALSE);
	}
	mp_clock_delay(20);
	return(TRUE);
}

static void _uc_reset(mp_drv_uc8179_t *panel) {
	mp_gpio_set(panel->reset);
	mp_clock_delay(5);
	mp_gpio_unset(panel->reset);
	mp_clock_delay(20);
	mp_gpio_set(panel->reset);
	mp_clock_delay(150);
}

/* the OTP waveform of the 7.5 inch mono panel: panel setting 0x1F, no host LUT */
static mp_ret_t _uc_setup(mp_drv_uc8179_t *panel) {
	_uc_cmd(panel, 0x01);
	_uc_data(panel, 0x07);
	_uc_data(panel, 0x07);
	_uc_data(panel, 0x3F);
	_uc_data(panel, 0x3F);

	_uc_cmd(panel, 0x06);
	_uc_data(panel, 0x17);
	_uc_data(panel, 0x17);
	_uc_data(panel, 0x28);
	_uc_data(panel, 0x17);

	_uc_cmd(panel, 0x04);
	mp_clock_delay(100);
	if(_uc_wait(panel) == FALSE)
		return(FALSE);

	_uc_cmd(panel, 0x00);
	_uc_data(panel, panel->psr);

	_uc_cmd(panel, 0x61);
	_uc_data(panel, (unsigned char)(_UC_W >> 8));
	_uc_data(panel, (unsigned char)(_UC_W & 0xFF));
	_uc_data(panel, (unsigned char)(_UC_H >> 8));
	_uc_data(panel, (unsigned char)(_UC_H & 0xFF));

	_uc_cmd(panel, 0x15);
	_uc_data(panel, 0x00);

	_uc_cmd(panel, 0x50);
	_uc_data(panel, 0x10);
	_uc_data(panel, 0x07);

	_uc_cmd(panel, 0x60);
	_uc_data(panel, 0x22);
	return(TRUE);
}

/* x1 and y1 are inclusive. The controller stores the end as that pixel minus one:
 * the full 800x480 glass is the bytes 00 00 03 1E, 00 00 01 DE.
 * scan is 1 to drive every gate, 0 to stay inside this window. */
static void _uc_window(mp_drv_uc8179_t *panel, int x0, int y0, int x1, int y1, int scan) {
	unsigned endx = (unsigned)x1 - 1u;
	unsigned endy = (unsigned)y1 - 1u;

	_uc_cmd(panel, 0x50);
	_uc_data(panel, 0xA9);
	_uc_data(panel, 0x07);
	_uc_cmd(panel, 0x91);
	_uc_cmd(panel, 0x90);
	_uc_data(panel, (unsigned char)(x0 >> 8));
	_uc_data(panel, (unsigned char)x0);
	_uc_data(panel, (unsigned char)(endx >> 8));
	_uc_data(panel, (unsigned char)endx);
	_uc_data(panel, (unsigned char)(y0 >> 8));
	_uc_data(panel, (unsigned char)y0);
	_uc_data(panel, (unsigned char)(endy >> 8));
	_uc_data(panel, (unsigned char)endy);
	_uc_data(panel, scan ? 0x01 : 0x00);
}

static void _uc_span(mp_drv_uc8179_t *panel, unsigned char command, const unsigned char *base, int x, int y, int w, int h) {
	int row;
	int col;
	int bytes;
	const unsigned char *src;

	bytes = w >> 3;
	_uc_cmd(panel, command);
	mp_gpio_set(panel->dc);
	mp_gpio_unset(panel->cs);
	for(row = 0; row < h; row++) {
		src = base + ((y + row) * (_UC_W / 8)) + (x >> 3);
		for(col = 0; col < bytes; col++)
			_uc_shift(panel, src[col]);
	}
	mp_gpio_set(panel->cs);
}

static mp_ret_t _uc_frame(mp_drv_uc8179_t *panel) {
	if(panel->fast) {
		if(_uc_setup(panel) == FALSE)
			return(FALSE);
		panel->fast = 0;
	}
	_uc_window(panel, 0, 0, _UC_W - 1, _UC_H - 1, 1);
	_uc_span(panel, 0x10, panel->plane, 0, 0, _UC_W, _UC_H);
	_uc_span(panel, 0x13, panel->plane, 0, 0, _UC_W, _UC_H);
	_uc_cmd(panel, 0x12);
	mp_clock_delay(1);
	if(_uc_wait(panel) == FALSE)
		return(FALSE);
#ifdef SUPPORT_COMMON_TUI
	memcpy(panel->sent, panel->plane, _UC_BYTES);
#endif
	return(TRUE);
}

/* inclusive end, gates stay inside the window. The horizontal end is the last
 * pixel of a byte, so its low bits are 111, as the controller expects. */
static void _uc_partial_window(mp_drv_uc8179_t *panel, int x0, int y0, int x1, int y1) {
	_uc_cmd(panel, 0x50);
	_uc_data(panel, 0xA9);
	_uc_data(panel, 0x07);
	_uc_cmd(panel, 0x91);
	_uc_cmd(panel, 0x90);
	_uc_data(panel, (unsigned char)(x0 >> 8));
	_uc_data(panel, (unsigned char)x0);
	_uc_data(panel, (unsigned char)(x1 >> 8));
	_uc_data(panel, (unsigned char)x1);
	_uc_data(panel, (unsigned char)(y0 >> 8));
	_uc_data(panel, (unsigned char)y0);
	_uc_data(panel, (unsigned char)(y1 >> 8));
	_uc_data(panel, (unsigned char)y1);
	_uc_data(panel, 0x00);
}

/* the new pixels only, plus a byte and a line of context so an edge is not dropped */
static mp_ret_t _uc_part(mp_drv_uc8179_t *panel, int x, int y, int w, int h) {
	int x0 = x & ~7;
	int y0 = y;
	int x1 = (x + w + 7) & ~7;
	int y1 = y + h;

	if(x1 > _UC_W)
		x1 = _UC_W;
	if(x0 >= 8)
		x0 -= 8;
	if(y0 > 0)
		y0--;
	if(x1 <= _UC_W - 8)
		x1 += 8;
	else
		x1 = _UC_W;
	if(y1 < _UC_H)
		y1++;
	w = x1 - x0;
	h = y1 - y0;
	if(w <= 0 || h <= 0)
		return(TRUE);
	_uc_cmd(panel, 0xE0);
	_uc_data(panel, 0x02);
	_uc_cmd(panel, 0xE5);
	_uc_data(panel, 0x6E);
	panel->fast = 1;
	_uc_partial_window(panel, x0, y0, x1 - 1, y1 - 1);
	_uc_span(panel, 0x13, panel->plane, x0, y0, w, h);
	_uc_cmd(panel, 0x12);
	mp_clock_delay(1);
	return(_uc_wait(panel));
}

static void _uc_sleep(mp_drv_uc8179_t *panel) {
	_uc_cmd(panel, 0x50);
	_uc_data(panel, 0xF7);
	_uc_cmd(panel, 0x02);
	if(_uc_wait(panel) == FALSE)
		return;
	_uc_cmd(panel, 0x07);
	_uc_data(panel, 0xA5);
}

static void _uc_pixel(mp_drv_uc8179_t *panel, int x, int y, int white) {
	unsigned index;
	unsigned char mask;

	if(x < 0 || y < 0 || x >= _UC_W || y >= _UC_H)
		return;
	index = (unsigned)(y * (_UC_W / 8) + (x >> 3));
	mask = (unsigned char)(0x80u >> (x & 7));
	if(white)
		panel->plane[index] |= mask;
	else
		panel->plane[index] &= (unsigned char)~mask;
}

static void _uc_picture(mp_drv_uc8179_t *panel) {
	int x;
	int y;

	memset(panel->plane, 0xFF, _UC_BYTES);
	for(x = 0; x < _UC_W; x++) {
		_uc_pixel(panel, x, 0, 0);
		_uc_pixel(panel, x, 1, 0);
		_uc_pixel(panel, x, _UC_H - 2, 0);
		_uc_pixel(panel, x, _UC_H - 1, 0);
	}
	for(y = 0; y < _UC_H; y++) {
		_uc_pixel(panel, 0, y, 0);
		_uc_pixel(panel, 1, y, 0);
		_uc_pixel(panel, _UC_W - 2, y, 0);
		_uc_pixel(panel, _UC_W - 1, y, 0);
	}
}

static void _uc_release(mp_gpio_port_t **port) {
	if(*port) {
		mp_gpio_release(*port);
		*port = NULL;
	}
}

mp_ret_t mp_drv_uc8179_init(mp_kernel_t *kernel, mp_drv_uc8179_t *panel, mp_options_t *options, char *who) {
	memset(panel, 0, sizeof(*panel));
	panel->kernel = kernel;
	if(!who || !who[0])
		who = "uc8179";

	panel->sck = _uc_pin(options, "sck", who);
	panel->mosi = _uc_pin(options, "mosi", who);
	panel->cs = _uc_pin(options, "cs", who);
	panel->dc = _uc_pin(options, "dc", who);
	panel->reset = _uc_pin(options, "reset", who);
	panel->busy = _uc_pin(options, "busy", who);
	panel->power = _uc_pin(options, "power", who);
	panel->psr = 0x1F;
	{
		char *turn = mp_options_get(options, "orientation");

		/* source shift reverses: a left-right mirror. Both axes reverse for 180 degrees. */
		if(turn && turn[0] == 'f')
			panel->psr = 0x1B;
		else if(turn && turn[0] == 'i')
			panel->psr = 0x13;
	}
	if(!panel->sck || !panel->mosi || !panel->cs || !panel->dc || !panel->reset || !panel->busy || !panel->power) {
		mp_printk("UC8179: clock, MOSI, chip select, data/command, reset, busy and power are required");
		mp_drv_uc8179_fini(panel);
		return(FALSE);
	}

	mp_gpio_direction(panel->power, MP_GPIO_OUTPUT);
	mp_gpio_set(panel->power);
	mp_clock_delay(20);
	mp_gpio_direction(panel->sck, MP_GPIO_OUTPUT);
	mp_gpio_direction(panel->mosi, MP_GPIO_OUTPUT);
	mp_gpio_direction(panel->cs, MP_GPIO_OUTPUT);
	mp_gpio_direction(panel->dc, MP_GPIO_OUTPUT);
	mp_gpio_direction(panel->reset, MP_GPIO_OUTPUT);
	mp_gpio_unset(panel->sck);
	mp_gpio_unset(panel->mosi);
	mp_gpio_unset(panel->dc);
	mp_gpio_set(panel->cs);
	mp_gpio_set(panel->reset);
	mp_gpio_mode(panel->busy, MP_GPIO_MODE_INPUT_PULL);

	_uc_reset(panel);
	if(_uc_setup(panel) == FALSE) {
		mp_printk("UC8179: the panel did not take the setup");
		mp_drv_uc8179_fini(panel);
		return(FALSE);
	}

	panel->on = 1;
	_uc_picture(panel);
	if(_uc_frame(panel) == FALSE) {
		mp_printk("UC8179: the picture did not reach the panel");
		mp_drv_uc8179_fini(panel);
		return(FALSE);
	}

#ifdef SUPPORT_COMMON_TUI
	panel->display.width = _UC_W;
	panel->display.height = _UC_H;
	panel->display.write = _uc_write;
	panel->display.flush = _uc_flush_face;
	panel->display.power = _uc_power_face;
	panel->display.user = panel;
#endif

#ifdef SUPPORT_COMMON_DEVICE
	mp_device_identify(panel, "7.5 inch ePaper", "UC8179", mp_machine_serial(), NULL);
#endif
	mp_printk("UC8179 ready, %s", who);
	return(TRUE);
}

mp_ret_t mp_drv_uc8179_fini(mp_drv_uc8179_t *panel) {
	if(panel->on && panel->cs)
		_uc_sleep(panel);
	panel->on = 0;
	if(panel->power)
		mp_gpio_unset(panel->power);
	_uc_release(&panel->power);
	_uc_release(&panel->sck);
	_uc_release(&panel->mosi);
	_uc_release(&panel->cs);
	_uc_release(&panel->dc);
	_uc_release(&panel->reset);
	_uc_release(&panel->busy);
	return(TRUE);
}

#ifdef SUPPORT_COMMON_TUI

static int _uc_white(mp_tui_color_t color) {
	unsigned r = (color >> 11) & 31;
	unsigned g = (color >> 5) & 63;
	unsigned b = color & 31;

	/* a light source pixel is white on the panel */
	return((r + (g >> 1) + b) >= 32);
}

static void _uc_box(mp_drv_uc8179_t *panel, int x, int y, int w, int h) {
	int x1;
	int y1;
	int right;
	int bottom;

	if(w <= 0 || h <= 0)
		return;
	if(x < 0) {
		w += x;
		x = 0;
	}
	if(y < 0) {
		h += y;
		y = 0;
	}
	if(x >= _UC_W || y >= _UC_H || w <= 0 || h <= 0)
		return;
	if(x + w > _UC_W)
		w = _UC_W - x;
	if(y + h > _UC_H)
		h = _UC_H - y;
	if(!panel->box) {
		panel->boxX = (unsigned short)x;
		panel->boxY = (unsigned short)y;
		panel->boxW = (unsigned short)w;
		panel->boxH = (unsigned short)h;
		panel->box = 1;
		return;
	}
	x1 = x + w;
	y1 = y + h;
	right = (int)panel->boxX + (int)panel->boxW;
	bottom = (int)panel->boxY + (int)panel->boxH;
	if(x < (int)panel->boxX)
		panel->boxX = (unsigned short)x;
	if(y < (int)panel->boxY)
		panel->boxY = (unsigned short)y;
	if(x1 > right)
		right = x1;
	if(y1 > bottom)
		bottom = y1;
	panel->boxW = (unsigned short)(right - (int)panel->boxX);
	panel->boxH = (unsigned short)(bottom - (int)panel->boxY);
}

static void _uc_write(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *pixels) {
	mp_drv_uc8179_t *panel = display->user;
	int row;
	int col;

	if(!panel || !pixels || w <= 0 || h <= 0)
		return;
	for(row = 0; row < h; row++) {
		for(col = 0; col < w; col++)
			_uc_pixel(panel, x + col, y + row, _uc_white(pixels[row * w + col]));
	}
	_uc_box(panel, x, y, w, h);
}

#define _UC_RUN_MAX 4

typedef struct {
	unsigned short x;
	unsigned short y;
	unsigned short w;
	unsigned short h;
} _uc_run_t;

/* the bytes of this row that differ from the glass, inside x0..x1 inclusive */
static int _uc_row(mp_drv_uc8179_t *panel, int y, int x0, int x1, int *first, int *last) {
	int stride = _UC_W / 8;
	int c;
	int c0;
	int c1;
	unsigned char *now;
	unsigned char *was;

	c0 = x0 >> 3;
	c1 = x1 >> 3;
	if(c0 < 0)
		c0 = 0;
	if(c1 >= stride)
		c1 = stride - 1;
	now = panel->plane + y * stride;
	was = panel->sent + y * stride;
	*first = -1;
	*last = -1;
	for(c = c0; c <= c1; c++) {
		if(now[c] == was[c])
			continue;
		if(*first < 0)
			*first = c;
		*last = c;
	}
	return(*first >= 0);
}

static void _uc_run_wide(_uc_run_t *run, int first, int last) {
	int left = first * 8;
	int right = (last + 1) * 8;
	int end = (int)run->x + (int)run->w;

	if(left < (int)run->x) {
		run->w = (unsigned short)(end - left);
		run->x = (unsigned short)left;
		end = (int)run->x + (int)run->w;
	}
	if(right > end)
		run->w = (unsigned short)(right - (int)run->x);
}

/* contiguous rows of pixels that differ, at most _UC_RUN_MAX; a further run grows the last one */
static int _uc_runs(mp_drv_uc8179_t *panel, int x, int y, int w, int h, _uc_run_t *run) {
	int row;
	int n = 0;
	int open = 0;
	int first;
	int last;
	int y1 = y + h - 1;
	int x1 = x + w - 1;

	for(row = y; row <= y1; row++) {
		if(_uc_row(panel, row, x, x1, &first, &last) == 0) {
			open = 0;
			continue;
		}
		if(!open) {
			if(n == _UC_RUN_MAX) {
				_uc_run_wide(&run[n - 1], first, last);
				run[n - 1].h = (unsigned short)(row - (int)run[n - 1].y + 1);
			} else {
				run[n].x = (unsigned short)(first * 8);
				run[n].y = (unsigned short)row;
				run[n].w = (unsigned short)((last - first + 1) * 8);
				run[n].h = 1;
				n++;
			}
			open = 1;
			continue;
		}
		_uc_run_wide(&run[n - 1], first, last);
		run[n - 1].h = (unsigned short)(row - (int)run[n - 1].y + 1);
	}
	return(n);
}

static void _uc_flush_face(mp_tui_display_t *display) {
	mp_drv_uc8179_t *panel = display->user;
	_uc_run_t run[_UC_RUN_MAX];
	int n;
	int i;
	int x0;
	int y0;
	int x1;
	int y1;
	int bx;
	int by;
	int bw;
	int bh;

	if(!panel || !panel->on || !panel->box)
		return;
	bx = (int)panel->boxX;
	by = (int)panel->boxY;
	bw = (int)panel->boxW;
	bh = (int)panel->boxH;
	n = _uc_runs(panel, bx, by, bw, bh, run);
	panel->box = 0;
	if(n == 0)
		return;
	x0 = (int)run[0].x;
	y0 = (int)run[0].y;
	x1 = x0 + (int)run[0].w;
	y1 = y0 + (int)run[0].h;
	for(i = 1; i < n; i++) {
		if((int)run[i].x < x0)
			x0 = (int)run[i].x;
		if((int)run[i].y < y0)
			y0 = (int)run[i].y;
		if((int)run[i].x + (int)run[i].w > x1)
			x1 = (int)run[i].x + (int)run[i].w;
		if((int)run[i].y + (int)run[i].h > y1)
			y1 = (int)run[i].y + (int)run[i].h;
	}
	/* the opening picture touches every pixel and needs the full waveform */
	if(x1 - x0 >= _UC_W && y1 - y0 >= _UC_H) {
		if(_uc_frame(panel) == TRUE)
			panel->glassAt = mp_clock_ticks();
		return;
	}
	/* a page that ticks would stack waveforms and eat the edges */
	if(panel->glassAt && (mp_clock_ticks() - panel->glassAt) < mp_clock_tick_hz() * 2UL) {
		panel->boxX = (unsigned short)bx;
		panel->boxY = (unsigned short)by;
		panel->boxW = (unsigned short)bw;
		panel->boxH = (unsigned short)bh;
		panel->box = 1;
		return;
	}
	if(_uc_part(panel, x0, y0, x1 - x0, y1 - y0) == FALSE) {
		panel->boxX = (unsigned short)bx;
		panel->boxY = (unsigned short)by;
		panel->boxW = (unsigned short)bw;
		panel->boxH = (unsigned short)bh;
		panel->box = 1;
		return;
	}
	panel->glassAt = mp_clock_ticks();
}

static void _uc_power_face(mp_tui_display_t *display, int on) {
	mp_drv_uc8179_t *panel = display->user;

	if(!panel)
		return;
	if(on) {
		if(panel->on)
			return;
		_uc_reset(panel);
		if(_uc_setup(panel) == FALSE)
			return;
		panel->on = 1;
		panel->fast = 0;
		panel->box = 0;
		_uc_frame(panel);
		return;
	}
	if(!panel->on)
		return;
	_uc_sleep(panel);
	panel->on = 0;
}

#endif

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _uc_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_uc8179_init(kernel, self, options, who));
}

static void _uc_device_close(void *self) {
	mp_drv_uc8179_fini(self);
}

static void *_uc_device_face(void *self) {
#ifdef SUPPORT_COMMON_TUI
	return(&((mp_drv_uc8179_t *)self)->display);
#else
	(void)self;
	return(NULL);
#endif
}

const mp_device_ops_t mp_drv_uc8179_device = {
	"lcd/uc8179", MP_DEVICE_DISPLAY, _uc_device_open, _uc_device_close, _uc_device_face, NULL,
	"800x480 UC8179", "1.0.0"
};

#endif

#endif
