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

#ifdef SUPPORT_DRV_TOUCH_XPT2046

/**
 * XPT2046 / ADS7846. One conversion is 24 clocks under CS: the command,
 * then two zero bytes. CS rises before the next command. PD1 and PD0 stay
 * 0 so the chip powers down and PENIRQ can be read, and only while CS is high.
 *
 * 0xD0 is X (start, A=101, 12-bit, differential).
 * 0x90 is Y (A=001). 0xB0 is Z1 (A=011).
 */

#define _XPT_CMD_X 0xD0
#define _XPT_CMD_Y 0x90
#define _XPT_CMD_Z1 0xB0

/* 200..3900 is a starting window, not a calibration of one glass. The board overrides it. */
#define _XPT_WINDOW_MIN 200
#define _XPT_WINDOW_MAX 3900
#define _XPT_ZMIN 200
#define _XPT_COLUMNS 240
#define _XPT_ROWS 320

/* a light finger is noisier than a hard one */
#define _XPT_SPREAD 350

/* one bad sample does not mean the finger left */
#define _XPT_MISS 4

/* counts a sample may fall outside the window; further out is a dropout */
#define _XPT_EDGE 100

/* pixels two samples of a new press may differ by */
#define _XPT_AGREE 10

/* pixels a held finger may move in one period */
#define _XPT_JUMP 80

enum {
	_XPT_LANDSCAPE = 0,
	_XPT_PORTRAIT,
	_XPT_LANDSCAPE_FLIPPED,
	_XPT_PORTRAIT_FLIPPED
};

static const char *_xpt_orient_name[] = {
	"landscape", "portrait", "landscape-flipped", "portrait-flipped"
};

static MP_TASK(_xpt_task);

static int _xpt_far(int a, int b, int limit) {
	int d = a - b;

	return((d < 0 ? -d : d) > limit);
}

static unsigned long _xpt_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

static void _xpt_release_pin(mp_gpio_port_t **pin) {
	if(*pin) {
		mp_gpio_release(*pin);
		*pin = NULL;
	}
}

/* busy is the top bit of those 16 clocks; three clocks after the result are unused */
static int _xpt_command(mp_drv_xpt2046_t *touch, unsigned char cmd) {
	unsigned char hi, lo;
	unsigned int raw;

	mp_gpio_unset(touch->cs);
	(void)mp_spi_xfer(&touch->bus, cmd);
	hi = mp_spi_xfer(&touch->bus, 0);
	lo = mp_spi_xfer(&touch->bus, 0);
	mp_gpio_set(touch->cs);
	raw = ((unsigned int)hi << 8) | lo;
	return((int)((raw >> 3) & 0x0FFFu));
}

static int _xpt_pair(mp_drv_xpt2046_t *touch, unsigned char cmd) {
	int first, second, delta;

	first = _xpt_command(touch, cmd);
	second = _xpt_command(touch, cmd);
	delta = first - second;
	if(delta < 0)
		delta = -delta;
	if(delta > _XPT_SPREAD)
		return(-1);
	return((first + second) / 2);
}

/* min above max mirrors the axis */
static int _xpt_scale(int raw, int min, int max, int span) {
	if(span <= 1 || max == min)
		return(0);
	if(max < min)
		return(span - 1 - _xpt_scale(raw, max, min, span));
	if(raw < min)
		raw = min;
	if(raw > max)
		raw = max;
	return((raw - min) * (span - 1) / (max - min));
}

static int _xpt_outside(int raw, int a, int b) {
	int lo = a < b ? a : b;
	int hi = a < b ? b : a;

	return(raw < lo - _XPT_EDGE || raw > hi + _XPT_EDGE);
}

/* landscape puts panel rows across the display, the same four cases as _mp_resistive_map */
static void _xpt_map(mp_drv_xpt2046_t *touch, int column, int row, int *x, int *y) {
	switch(touch->orientation) {
		case _XPT_PORTRAIT:
			*x = column;
			*y = row;
			break;
		case _XPT_PORTRAIT_FLIPPED:
			*x = touch->columns - 1 - column;
			*y = touch->rows - 1 - row;
			break;
		case _XPT_LANDSCAPE_FLIPPED:
			*x = touch->rows - 1 - row;
			*y = column;
			break;
		default:
			*x = row;
			*y = touch->columns - 1 - column;
			break;
	}
}

static void _xpt_release(mp_drv_xpt2046_t *touch) {
	touch->seen = 0;
	touch->miss = 0;
	touch->tracked = 0;
	if(!touch->down)
		return;
	touch->down = 0;
	mp_device_input_touch(&touch->input, touch->input.x, touch->input.y, 0);
}

static int _xpt_dropout(mp_drv_xpt2046_t *touch) {
	if(touch->down && touch->miss < _XPT_MISS) {
		touch->miss++;
		return(1);
	}
	_xpt_release(touch);
	return(0);
}

static void _xpt_track(mp_drv_xpt2046_t *touch, int x, int y) {
	int dx, dy;

	if(!touch->tracked) {
		touch->fx = x;
		touch->fy = y;
		touch->tracked = 1;
		mp_device_input_touch(&touch->input, x, y, 1);
		return;
	}

	touch->fx = (touch->fx * 3 + x) / 4;
	touch->fy = (touch->fy * 3 + y) / 4;
	dx = touch->fx - touch->input.x;
	dy = touch->fy - touch->input.y;
	if(dx < 0)
		dx = -dx;
	if(dy < 0)
		dy = -dy;
	if(dx < 3 && dy < 3)
		return;
	mp_device_input_touch(&touch->input, touch->fx, touch->fy, 1);
}

static void _xpt_pen(void *user) {
	mp_drv_xpt2046_t *touch = user;

	mp_gpio_interrupt_disable(touch->irq);
	if(touch->input.task)
		mp_task_wake(touch->input.task);
}

/* the edge is armed only with CS high: PENIRQ is not valid while CS is low */
static mp_bool_t _xpt_arm(mp_drv_xpt2046_t *touch) {
	mp_gpio_set(touch->cs);
	if(mp_gpio_interrupt_set(touch->irq, _xpt_pen, touch, "Touch pen") == FALSE)
		return(NO);
	mp_gpio_interrupt_hi2lo(touch->irq);
	mp_gpio_interrupt_enable(touch->irq);
	touch->armed = YES;
	return(YES);
}

static void _xpt_disarm(mp_drv_xpt2046_t *touch) {
	if(!touch || touch->armed == NO)
		return;
	mp_gpio_interrupt_unset(touch->irq);
	touch->armed = NO;
}

static void _xpt_every(mp_task_t *task, unsigned long ticks) {
	if(task->signal != MP_TASK_SIG_OK || task->delay != ticks)
		mp_task_delay(task, ticks);
}

static void _xpt_rest(mp_drv_xpt2046_t *touch, mp_task_t *task) {
	if(touch->input.dozing) {
		_xpt_every(task, touch->doze);
		if(!touch->irq || _xpt_arm(touch) == YES)
			return;
	}
	_xpt_every(task, touch->idle);
}

static MP_TASK(_xpt_task) {
	mp_drv_xpt2046_t *touch = task->user;
	int z1, rawX, rawY, column, row, x, y;

	_xpt_disarm(touch);

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* IRQ is active low, and the level is meaningless while CS is low */
	if(touch->irq) {
		mp_gpio_set(touch->cs);
		if(mp_gpio_read(touch->irq) == YES) {
			if(!_xpt_dropout(touch))
				_xpt_rest(touch, task);
			return;
		}
	}

	z1 = _xpt_command(touch, _XPT_CMD_Z1);
	if(z1 < touch->zMin) {
		if(!_xpt_dropout(touch))
			_xpt_rest(touch, task);
		return;
	}
	_xpt_every(task, touch->period);

	rawX = _xpt_pair(touch, _XPT_CMD_X);
	rawY = _xpt_pair(touch, _XPT_CMD_Y);
	if(rawX < 0 || rawY < 0 ||
		_xpt_outside(rawX, touch->xMin, touch->xMax) ||
		_xpt_outside(rawY, touch->yMin, touch->yMax)) {
		_xpt_dropout(touch);
		return;
	}
	touch->miss = 0;

	column = _xpt_scale(rawX, touch->xMin, touch->xMax, touch->columns);
	row = _xpt_scale(rawY, touch->yMin, touch->yMax, touch->rows);
	_xpt_map(touch, column, row, &x, &y);

	/* a light finger first gives a wrong place; two close samples are where it really is */
	if(!touch->down) {
		if(!touch->seen || _xpt_far(x, touch->px, _XPT_AGREE) || _xpt_far(y, touch->py, _XPT_AGREE)) {
			touch->seen = 1;
			touch->px = x;
			touch->py = y;
			return;
		}
		x = (x + touch->px) / 2;
		y = (y + touch->py) / 2;
		touch->down = 1;
	}
	/* a finger does not cross the screen in one period; that is the sheet losing contact */
	else if(touch->tracked && (_xpt_far(x, touch->fx, _XPT_JUMP) || _xpt_far(y, touch->fy, _XPT_JUMP))) {
		_xpt_dropout(touch);
		return;
	}
	_xpt_track(touch, x, y);
}

static void _xpt_start(void *self) {
	mp_drv_xpt2046_t *touch = self;
	int width, height;
	mp_task_t *task;

	if(touch->orientation == _XPT_PORTRAIT || touch->orientation == _XPT_PORTRAIT_FLIPPED) {
		width = touch->columns;
		height = touch->rows;
	}
	else {
		width = touch->rows;
		height = touch->columns;
	}
	touch->down = 0;
	touch->seen = 0;
	touch->miss = 0;
	touch->tracked = 0;
	mp_device_input_init(touch->kernel, &touch->input, MP_DEVICE_INPUT_CAP_TOUCH, "Touch");
	mp_device_input_bounds(&touch->input, width, height);
	task = mp_task_create(&touch->kernel->tasks, "Touch", _xpt_task, touch, touch->idle);
	if(!task) {
		mp_device_input_fini(&touch->input);
		return;
	}
	touch->input.task = task;
}

static void _xpt_stop(void *self) {
	mp_drv_xpt2046_t *touch = self;

	if(touch->down)
		mp_device_input_touch(&touch->input, touch->input.x, touch->input.y, 0);
	if(touch->input.task)
		mp_task_destroy(touch->input.task);
	_xpt_disarm(touch);
	touch->input.task = NULL;
	touch->down = 0;
	touch->seen = 0;
	touch->miss = 0;
	touch->tracked = 0;
	if(touch->input.kernel)
		mp_device_input_fini(&touch->input);
}

static void _xpt_shutdown(mp_drv_xpt2046_t *touch) {
	if(!touch || !touch->kernel)
		return;
	_xpt_disarm(touch);
	if(touch->registry.driver)
		mp_device_registry_unregister(&touch->registry);
	if(touch->binding.kernel)
		mp_device_driver_unregister(touch->kernel, &touch->binding);
	mp_spi_close(&touch->bus);
	_xpt_release_pin(&touch->cs);
	_xpt_release_pin(&touch->irq);
	touch->kernel = NULL;
}

static int _xpt_orientation(char *text) {
	if(text && mp_options_cmp(text, "portrait") == TRUE)
		return(_XPT_PORTRAIT);
	if(text && mp_options_cmp(text, "landscape-flipped") == TRUE)
		return(_XPT_LANDSCAPE_FLIPPED);
	if(text && mp_options_cmp(text, "portrait-flipped") == TRUE)
		return(_XPT_PORTRAIT_FLIPPED);
	return(_XPT_LANDSCAPE);
}

static mp_ret_t _xpt_opt_int(mp_options_t *options, char *key, int fallback, int *out) {
	char *text = mp_options_get(options, key);
	int value = 0;
	int sign = 1;

	if(!text || !text[0]) {
		*out = fallback;
		return(TRUE);
	}
	if(text[0] == '-') {
		sign = -1;
		text++;
	}
	if(text[0] < '0' || text[0] > '9') {
		mp_printk("XPT2046: %s is not a number", key);
		return(FALSE);
	}
	while(text[0] >= '0' && text[0] <= '9') {
		if(value > 1000000) {
			mp_printk("XPT2046: %s is not a number", key);
			return(FALSE);
		}
		value = value * 10 + (text[0] - '0');
		text++;
	}
	if(text[0]) {
		mp_printk("XPT2046: %s is not a number", key);
		return(FALSE);
	}
	*out = sign * value;
	return(TRUE);
}

static mp_gpio_port_t *_xpt_pin(mp_options_t *options, char *key, char *who, int required, int output, int level) {
	char *text = mp_options_get(options, key);
	mp_gpio_port_t *pin;

	if(!text || !text[0]) {
		if(required)
			mp_printk("XPT2046: %s is required", key);
		return(NULL);
	}
	pin = mp_gpio_text_handle(text, who);
	if(!pin) {
		mp_printk("XPT2046: %s is not free", key);
		return(NULL);
	}
	if(output) {
		mp_gpio_mode(pin, MP_GPIO_MODE_OUTPUT);
		if(level)
			mp_gpio_set(pin);
		else
			mp_gpio_unset(pin);
	}
	else
		mp_gpio_mode(pin, MP_GPIO_MODE_INPUT);
	return(pin);
}

mp_ret_t mp_drv_xpt2046_init(mp_kernel_t *kernel, mp_drv_xpt2046_t *touch, mp_options_t *options, char *who) {
	static const mp_device_driver_ops_t ops = {
		"touch/xpt2046", "XPT2046 touch controller", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_TOUCH),
		NULL, NULL, NULL,
		_xpt_start, _xpt_stop
	};
	char *model;
	char *serial;
	char *cs;

	if(!kernel || !touch)
		return(FALSE);
	memset(touch, 0, sizeof(*touch));
	touch->kernel = kernel;
	if(!who || !who[0])
		who = "XPT2046";

	if(_xpt_opt_int(options, "columns", _XPT_COLUMNS, &touch->columns) == FALSE ||
		_xpt_opt_int(options, "rows", _XPT_ROWS, &touch->rows) == FALSE ||
		_xpt_opt_int(options, "xmin", _XPT_WINDOW_MIN, &touch->xMin) == FALSE ||
		_xpt_opt_int(options, "xmax", _XPT_WINDOW_MAX, &touch->xMax) == FALSE ||
		_xpt_opt_int(options, "ymin", _XPT_WINDOW_MIN, &touch->yMin) == FALSE ||
		_xpt_opt_int(options, "ymax", _XPT_WINDOW_MAX, &touch->yMax) == FALSE ||
		_xpt_opt_int(options, "zmin", _XPT_ZMIN, &touch->zMin) == FALSE)
		goto fail;
	if(touch->columns < 2 || touch->rows < 2 || touch->xMax == touch->xMin || touch->yMax == touch->yMin) {
		mp_printk("XPT2046: columns, rows and the raw window");
		goto fail;
	}
	touch->orientation = _xpt_orientation(mp_options_get(options, "orientation"));
	touch->period = _xpt_ticks(20);
	touch->idle = _xpt_ticks(100);
	touch->doze = _xpt_ticks(50);

	model = mp_options_get(options, "model");
	touch->model = (model && model[0]) ? model : "xpt2046";
	serial = mp_options_get(options, "serial");
	touch->serial = (serial && serial[0]) ? serial : mp_machine_serial();
	if(!touch->serial || !touch->serial[0]) {
		mp_printk("XPT2046: no serial");
		goto fail;
	}

	/* half_us stays at the bus default, about 100 kHz */
	if(mp_spi_open(kernel, &touch->bus, options, who) == FALSE) {
		mp_printk("XPT2046: SCK and MOSI are required");
		goto fail;
	}
	if(!touch->bus.miso) {
		mp_printk("XPT2046: MISO is required");
		goto fail;
	}

	cs = mp_options_get(options, "cs");
	if(!cs || !cs[0]) {
		mp_printk("XPT2046: cs is required");
		goto fail;
	}
	touch->cs = _xpt_pin(options, "cs", who, 1, 1, 1);
	if(!touch->cs)
		goto fail;
	if(mp_options_get(options, "irq") && mp_options_get(options, "irq")[0]) {
		touch->irq = _xpt_pin(options, "irq", who, 0, 0, 0);
		if(!touch->irq)
			goto fail;
	}

	if(mp_device_driver_register(kernel, &touch->binding, &ops, touch, NULL) == FALSE)
		goto fail;
	if(!mp_device_registry_register(&touch->binding, &touch->registry, MP_DEVICE_TOUCH, 0, &touch->input, touch->model, touch->serial, NULL)) {
		mp_printk("XPT2046: can not register the touch");
		goto fail;
	}
	touch->registry.label = "XPT2046 touch";
	mp_device_identify(touch, "XPT2046 touch", touch->model, touch->serial, NULL);
	if(touch->irq)
		mp_printk("XPT2046 touch %s, %dx%d, CS P%c%u, IRQ P%c%u",
			_xpt_orient_name[touch->orientation],
			touch->columns, touch->rows,
			'A' + touch->cs->port, touch->cs->pin,
			'A' + touch->irq->port, touch->irq->pin);
	else
		mp_printk("XPT2046 touch %s, %dx%d, CS P%c%u",
			_xpt_orient_name[touch->orientation],
			touch->columns, touch->rows,
			'A' + touch->cs->port, touch->cs->pin);
	return(TRUE);

fail:
	_xpt_shutdown(touch);
	return(FALSE);
}

mp_ret_t mp_drv_xpt2046_fini(mp_drv_xpt2046_t *touch) {
	if(!touch || !touch->kernel)
		return(FALSE);
	_xpt_shutdown(touch);
	return(TRUE);
}

#ifdef SUPPORT_COMMON_DEVICE

static mp_ret_t _xpt_open(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who) {
	return(mp_drv_xpt2046_init(kernel, self, options, who));
}

static void _xpt_close(void *self) {
	mp_drv_xpt2046_fini(self);
}

static void *_xpt_face(void *self) {
	mp_drv_xpt2046_t *touch = self;

	if(!touch)
		return(NULL);
	return(&touch->input);
}

const mp_device_ops_t mp_drv_xpt2046_device = {
	"touch/xpt2046", MP_DEVICE_TOUCH, _xpt_open, _xpt_close, _xpt_face, NULL,
	"XPT2046 touch controller", "1.0.0"
};

#endif

#endif
