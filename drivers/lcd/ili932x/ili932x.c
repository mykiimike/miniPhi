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

#ifdef SUPPORT_DRV_LCD_ILI932X

/**
@defgroup mpDriverILI932X ILI932x TFT

@ingroup mpDriver

The panel is 240 columns (h) by 320 lines (v). The orientation maps
display coordinates onto them and sets the entry mode so the pixels of
a window arrive row after row of the display.

The backlight is a pin, or a PWM channel when backlightTimer names the
timer of that pin: the TUI screen saver dims it, then turns it off.
Timers stop in Stop, so a backlight between off and full holds the LOW
power level. Standby (option standby) powers the panel down and the TUI
redraws it on the way back. Left off, the glass stays up so a touch wakes
the screen.

@{
*/

#define ILI932X_WIDTH 240
#define ILI932X_HEIGHT 320

#define ILI932X_DELAY 0xFFFF

/* register, value; ILI932X_DELAY waits value ms */
static const uint16_t _ili932x_init[] = {
	0x00E5, 0x8000,
	0x0000, 0x0001,
	ILI932X_DELAY, 50,
	0x0001, 0x0100,
	0x0002, 0x0700,
	0x0004, 0x0000,
	0x0008, 0x0202,
	0x0009, 0x0000,
	0x000A, 0x0000,
	0x000C, 0x0000,
	0x000D, 0x0000,
	0x000F, 0x0000,
	/* power on */
	0x0010, 0x0000,
	0x0011, 0x0000,
	0x0012, 0x0000,
	0x0013, 0x0000,
	ILI932X_DELAY, 200,
	0x0010, 0x17B0,
	0x0011, 0x0137,
	ILI932X_DELAY, 50,
	0x0012, 0x0139,
	ILI932X_DELAY, 50,
	0x0013, 0x1D00,
	0x0029, 0x0013,
	ILI932X_DELAY, 50,
	/* gamma */
	0x0030, 0x0000,
	0x0031, 0x0505,
	0x0032, 0x0004,
	0x0035, 0x0006,
	0x0036, 0x0707,
	0x0037, 0x0105,
	0x0038, 0x0002,
	0x0039, 0x0707,
	0x003C, 0x0704,
	0x003D, 0x0807,
	/* 320 gate lines, no scrolling, no partial images */
	0x0060, 0x2700,
	0x0061, 0x0001,
	0x006A, 0x0000,
	0x0080, 0x0000,
	0x0081, 0x0000,
	0x0082, 0x0000,
	0x0083, 0x0000,
	0x0084, 0x0000,
	0x0085, 0x0000,
	0x0090, 0x0010,
	0x0092, 0x0000,
	0x0093, 0x0003,
	0x0095, 0x0110,
	0x0097, 0x0000,
	0x0098, 0x0000,
};

/* entry mode: BGR, then ID1 ID0 AM per orientation */
static const uint16_t _ili932x_entry[] = { 0x1028, 0x1030, 0x1018, 0x1000 };

static const char *_ili932x_orientations[] = { "landscape", "portrait", "landscape-flipped", "portrait-flipped" };

static void _ili932x_reg(mp_drv_ili932x_t *lcd, uint16_t reg, uint16_t value) {
	mp_fsmc_write_cmd(&lcd->bus, reg);
	mp_fsmc_write_data(&lcd->bus, value);
}

static uint16_t _ili932x_read(mp_drv_ili932x_t *lcd, uint16_t reg) {
	mp_fsmc_write_cmd(&lcd->bus, reg);
	return(mp_fsmc_read_data(&lcd->bus));
}

static void _ili932x_wait(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz + 999) / 1000 : ms;
	mp_clock_delay((int)(ticks ? ticks : 1));
}

/* panel h, v of a display point */
static void _ili932x_map(mp_drv_ili932x_t *lcd, int x, int y, int *h, int *v) {
	switch(lcd->orientation) {
		case MP_DRV_ILI932X_PORTRAIT:
			*h = x;
			*v = y;
			break;
		case MP_DRV_ILI932X_PORTRAIT_FLIPPED:
			*h = ILI932X_WIDTH - 1 - x;
			*v = ILI932X_HEIGHT - 1 - y;
			break;
		case MP_DRV_ILI932X_LANDSCAPE_FLIPPED:
			*h = y;
			*v = ILI932X_HEIGHT - 1 - x;
			break;
		default:
			*h = ILI932X_WIDTH - 1 - y;
			*v = x;
			break;
	}
}

static void _ili932x_write(mp_tui_display_t *display, int x, int y, int w, int h, const mp_tui_color_t *pixels) {
	mp_drv_ili932x_t *lcd = display->user;
	int h0, v0, h1, v1;
	long count = (long)w * h;

	_ili932x_map(lcd, x, y, &h0, &v0);
	_ili932x_map(lcd, x + w - 1, y + h - 1, &h1, &v1);

	_ili932x_reg(lcd, 0x0050, h0 < h1 ? h0 : h1);
	_ili932x_reg(lcd, 0x0051, h0 < h1 ? h1 : h0);
	_ili932x_reg(lcd, 0x0052, v0 < v1 ? v0 : v1);
	_ili932x_reg(lcd, 0x0053, v0 < v1 ? v1 : v0);
	_ili932x_reg(lcd, 0x0020, h0);
	_ili932x_reg(lcd, 0x0021, v0);

	mp_fsmc_write_cmd(&lcd->bus, 0x0022);
	while(count--)
		mp_fsmc_write_data(&lcd->bus, *pixels++);
}

static void _ili932x_percent(mp_drv_ili932x_t *lcd, int percent) {
	unsigned long period;

	if(percent < 0)
		percent = 0;
	if(percent > 100)
		percent = 100;
	lcd->brightness = percent;

	if(lcd->dimmable == YES) {
		period = (unsigned long)mp_timer_get_precision(&lcd->timer);
		/* above the period is always active: steady through Stop */
		mp_timer_pwm_set(&lcd->pwm, percent >= 100 ? period : period * (unsigned long)percent / 100UL);
		if(percent > 0 && percent < 100)
			mp_power_hold(lcd->kernel, &lcd->dimmed, MP_POWER_LOW, "Backlight");
		else
			mp_power_release(lcd->kernel, &lcd->dimmed);
		return;
	}

	if(!lcd->backlight)
		return;
	if(percent > 0)
		mp_gpio_set(lcd->backlight);
	else
		mp_gpio_unset(lcd->backlight);
}

static void _ili932x_backlight(mp_tui_display_t *display, int percent) {
	_ili932x_percent(display->user, percent);
}

/* standby keeps GRAM; out of it, the power registers are written again */
static void _ili932x_power(mp_tui_display_t *display, int on) {
	mp_drv_ili932x_t *lcd = display->user;

	if(lcd->canStandby == NO)
		return;

	if(on && lcd->standby == YES) {
		_ili932x_reg(lcd, 0x0010, 0x17B0);
		_ili932x_reg(lcd, 0x0011, 0x0137);
		_ili932x_reg(lcd, 0x0012, 0x0139);
		_ili932x_reg(lcd, 0x0013, 0x1D00);
		_ili932x_reg(lcd, 0x0029, 0x0013);
		_ili932x_wait(50);
		_ili932x_reg(lcd, 0x0007, 0x0173);
		lcd->standby = NO;
	}
	else if(!on && lcd->standby == NO) {
		_ili932x_reg(lcd, 0x0007, 0x0000);
		_ili932x_wait(20);
		_ili932x_reg(lcd, 0x0010, 0x17B1);
		lcd->standby = YES;
	}
}

static mp_ret_t _ili932x_dimmer(mp_kernel_t *kernel, mp_drv_ili932x_t *lcd, mp_options_t *options, char *who) {
	char *gate = mp_options_get(options, "backlightTimer");
	char *pin = mp_options_get(options, "backlight");
	char *low = mp_options_get(options, "backlightLow");
	mp_options_t timer[] = {
		{ "gate", gate },
		{ "frequency", "1000" },
		{ "precision", "100" },
		{ NULL, NULL }
	};
	mp_options_t pwm[] = {
		{ "port", pin },
		{ "polarity", low && mp_options_cmp(low, "true") == TRUE ? "low" : "high" },
		{ NULL, NULL }
	};

	if(!gate || !*gate || !pin || !*pin)
		return(FALSE);
	if(mp_timer_create(kernel, &lcd->timer, timer, who) == FALSE)
		return(FALSE);
	if(mp_timer_pwm_create(&lcd->timer, &lcd->pwm, pwm, who) == FALSE) {
		mp_timer_destroy(&lcd->timer);
		return(FALSE);
	}
	lcd->dimmable = YES;
	return(TRUE);
}

static mp_gpio_port_t *_ili932x_output(mp_options_t *options, char *key, char *who) {
	char *value = mp_options_get(options, key);
	mp_gpio_port_t *port;

	if(!value || !*value)
		return(NULL);
	port = mp_gpio_text_handle(value, who);
	if(port)
		mp_gpio_direction(port, MP_GPIO_OUTPUT);
	return(port);
}

mp_ret_t mp_drv_ili932x_init(mp_kernel_t *kernel, mp_drv_ili932x_t *lcd, mp_options_t *options, char *who) {
	const char *model;
	const char *serial;
	char *value;
	unsigned int index;

	memset(lcd, 0, sizeof(*lcd));
	lcd->kernel = kernel;
	model = mp_device_option(options, "model");
	serial = mp_machine_serial();
	if(!model || !serial) {
		mp_printk("ILI932x: model is required, and the board serial");
		return(FALSE);
	}
	lcd->serial = serial;

	value = mp_options_get(options, "standby");
	lcd->canStandby = value && mp_options_cmp(value, "true") == TRUE ? YES : NO;

	value = mp_options_get(options, "orientation");
	for(index = 0; value && index < sizeof(_ili932x_orientations) / sizeof(_ili932x_orientations[0]); index++)
		if(mp_options_cmp((char *)_ili932x_orientations[index], value) == TRUE)
			lcd->orientation = (mp_drv_ili932x_orientation_t)index;

	if(mp_fsmc_open(kernel, &lcd->bus, options, who) == FALSE)
		return(FALSE);

	/* dark until the panel is set up */
	if(_ili932x_dimmer(kernel, lcd, options, who) == TRUE)
		_ili932x_percent(lcd, 0);
	else {
		lcd->backlight = _ili932x_output(options, "backlight", who);
		if(lcd->backlight) {
			value = mp_options_get(options, "backlightLow");
			if(value && mp_options_cmp(value, "true") == TRUE)
				lcd->backlight->reverse = YES;
			mp_gpio_unset(lcd->backlight);
		}
	}

	lcd->reset = _ili932x_output(options, "reset", who);
	if(lcd->reset) {
		mp_gpio_unset(lcd->reset);
		_ili932x_wait(20);
		mp_gpio_set(lcd->reset);
		_ili932x_wait(120);
	}

	lcd->id = _ili932x_read(lcd, 0x0000);
	if(lcd->id != 0x9320 && lcd->id != 0x9325) {
		mp_printk("ILI932x %s: no panel, register 0 reads 0x%04x", who, lcd->id);
		mp_drv_ili932x_fini(lcd);
		return(FALSE);
	}

	for(index = 0; index < sizeof(_ili932x_init) / sizeof(_ili932x_init[0]); index += 2) {
		if(_ili932x_init[index] == ILI932X_DELAY)
			_ili932x_wait(_ili932x_init[index + 1]);
		else
			_ili932x_reg(lcd, _ili932x_init[index], _ili932x_init[index + 1]);
	}
	_ili932x_reg(lcd, 0x0003, _ili932x_entry[lcd->orientation]);
	_ili932x_reg(lcd, 0x0007, 0x0173);

	lcd->display.width = (lcd->orientation & 1) ? ILI932X_WIDTH : ILI932X_HEIGHT;
	lcd->display.height = (lcd->orientation & 1) ? ILI932X_HEIGHT : ILI932X_WIDTH;
	lcd->display.write = _ili932x_write;
	lcd->display.flush = NULL;
	lcd->display.backlight = (lcd->dimmable == YES || lcd->backlight) ? _ili932x_backlight : NULL;
	lcd->display.power = _ili932x_power;
	lcd->display.user = lcd;

	mp_drv_ili932x_backlight(lcd, YES);

	snprintf(lcd->part, sizeof(lcd->part), "ili%04x", (unsigned int)lcd->id);
	mp_device_identify(lcd, lcd->id == 0x9320 ? "Ilitek ILI9320" : "Ilitek ILI9325", lcd->part, lcd->serial, NULL);
	mp_printk("ILI%04x %dx%d %s called %s", lcd->id, lcd->display.width, lcd->display.height,
		_ili932x_orientations[lcd->orientation], who);
	return(TRUE);
}

mp_ret_t mp_drv_ili932x_fini(mp_drv_ili932x_t *lcd) {
	if(!lcd->bus.gate)
		return(FALSE);

	if(lcd->id)
		_ili932x_reg(lcd, 0x0007, 0x0000);
	if(lcd->dimmable == YES) {
		_ili932x_percent(lcd, 0);
		mp_timer_pwm_destroy(&lcd->pwm);
		mp_timer_destroy(&lcd->timer);
		lcd->dimmable = NO;
	}
	if(lcd->backlight) {
		mp_gpio_unset(lcd->backlight);
		mp_gpio_release(lcd->backlight);
		lcd->backlight = NULL;
	}
	if(lcd->reset) {
		mp_gpio_release(lcd->reset);
		lcd->reset = NULL;
	}
	mp_fsmc_close(&lcd->bus);
	lcd->id = 0;
	return(TRUE);
}

void mp_drv_ili932x_backlight(mp_drv_ili932x_t *lcd, mp_bool_t on) {
	_ili932x_percent(lcd, on == YES ? 100 : 0);
}

#ifdef SUPPORT_COMMON_DEVICE
static mp_ret_t _ili932x_device_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_ili932x_init(kernel, self, options, who));
}

static void _ili932x_device_close(void *self) {
	mp_drv_ili932x_fini(self);
}

static void *_ili932x_device_face(void *self) {
	return(&((mp_drv_ili932x_t *)self)->display);
}

const mp_device_ops_t mp_drv_ili932x_device = {
	"ILI932X", MP_DEVICE_DISPLAY, _ili932x_device_open, _ili932x_device_close, _ili932x_device_face, NULL,
	"ILI9320 / ILI9325 TFT", "1.0.0"
};
#endif

/**@}*/

#endif
