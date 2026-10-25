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

#ifdef SUPPORT_COMMON_RESISTIVE

/**
 * 4-wire resistive sheet. The board fills the glass. A press connects the
 * two plates. YD and YU are one plate, XL and XR the other.
 */

/* an open plate sits at a rail; a finger reads between these */
#define _MP_RESISTIVE_RAW_MIN 80
#define _MP_RESISTIVE_RAW_MAX 4015

/* open plate is near 4095; a light touch only has to pull it down from there */
#define _MP_RESISTIVE_TOUCH_MAX 3200

/* a light contact is noisier than a hard one */
#define _MP_RESISTIVE_SPREAD 350

/* one bad sample does not mean the finger left */
#define _MP_RESISTIVE_MISS 4

/* counts a sample may fall outside the calibration; further out is a plate losing contact */
#define _MP_RESISTIVE_EDGE 100

/* pixels two samples of a new press may differ by */
#define _MP_RESISTIVE_AGREE 10

/* pixels a held finger may move in one period */
#define _MP_RESISTIVE_JUMP 80

static const char *_mp_resistive_orientation[] = {
	"landscape", "portrait", "landscape-flipped", "portrait-flipped"
};

static MP_TASK(_mp_resistive_task);

static int _mp_resistive_far(int a, int b, int limit) {
	int d = a - b;

	return((d < 0 ? -d : d) > limit);
}

static unsigned long _mp_resistive_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

/* the plate needs time to settle; a light touch is a high resistance and a slow edge */
static void _mp_resistive_settle(int spins) {
	volatile int count = spins;

	while(count)
		count--;
}

static void _mp_resistive_drive(mp_gpio_port_t *pin, int high) {
	mp_gpio_direction(pin, MP_GPIO_OUTPUT);
	if(high)
		mp_gpio_set(pin);
	else
		mp_gpio_unset(pin);
}

static void _mp_resistive_float(mp_gpio_port_t *pin) {
	mp_gpio_mode(pin, MP_GPIO_MODE_INPUT);
}

static void _mp_resistive_analog(mp_gpio_port_t *pin) {
	mp_gpio_mode(pin, MP_GPIO_MODE_ANALOG);
}

/* ODR high selects the pull-up */
static void _mp_resistive_pullup(mp_gpio_port_t *pin) {
	mp_gpio_set(pin);
	mp_gpio_mode(pin, MP_GPIO_MODE_INPUT_PULL);
}

static void _mp_resistive_plates(mp_resistive_t *panel) {
	_mp_resistive_float(panel->yu);
	_mp_resistive_float(panel->xl);
	_mp_resistive_float(panel->xr);
	_mp_resistive_float(panel->yd);
}

static void _mp_resistive_pins_release(mp_resistive_t *panel) {
	if(panel->yd)
		mp_gpio_release(panel->yd);
	if(panel->yu)
		mp_gpio_release(panel->yu);
	if(panel->xl)
		mp_gpio_release(panel->xl);
	if(panel->xr)
		mp_gpio_release(panel->xr);
	panel->yd = NULL;
	panel->yu = NULL;
	panel->xl = NULL;
	panel->xr = NULL;
}

/* XL low and YD pulled up, then read on the ADC: a digital input only sees a hard press */
static int _mp_resistive_pressed(mp_resistive_t *panel) {
	int level;

	_mp_resistive_float(panel->yu);
	_mp_resistive_float(panel->xr);
	_mp_resistive_drive(panel->xl, 0);
	_mp_resistive_pullup(panel->yd);
	_mp_resistive_settle(6000);
	_mp_resistive_analog(panel->yd);
	_mp_resistive_settle(400);
	level = mp_adc_convert(panel->channelYd);
	return(level >= 0 && level < _MP_RESISTIVE_TOUCH_MAX);
}

/* drive one plate between low and high, measure the other */
static int _mp_resistive_sample(mp_gpio_port_t *low, mp_gpio_port_t *high, mp_gpio_port_t *idle, mp_gpio_port_t *sense, int channel) {
	int first, second, delta;

	_mp_resistive_drive(low, 0);
	_mp_resistive_drive(high, 1);
	_mp_resistive_float(idle);
	_mp_resistive_analog(sense);
	_mp_resistive_settle(2500);

	first = mp_adc_convert(channel);
	second = mp_adc_convert(channel);
	if(first < 0 || second < 0)
		return(-1);
	delta = first - second;
	if(delta < 0)
		delta = -delta;
	if(delta > _MP_RESISTIVE_SPREAD)
		return(-1);
	if(first < _MP_RESISTIVE_RAW_MIN || first > _MP_RESISTIVE_RAW_MAX)
		return(-1);
	return((first + second) / 2);
}

static int _mp_resistive_scale(int raw, int min, int max, int span) {
	if(span <= 1 || max <= min)
		return(0);
	if(raw < min)
		raw = min;
	if(raw > max)
		raw = max;
	return((raw - min) * (span - 1) / (max - min));
}

/* landscape puts panel rows across the display, the same corner as the ILI932x */
static void _mp_resistive_map(mp_resistive_t *panel, int column, int row, int *x, int *y) {
	if(panel->flipX)
		column = panel->columns - 1 - column;
	if(panel->flipY)
		row = panel->rows - 1 - row;

	switch(panel->orientation) {
		case MP_RESISTIVE_PORTRAIT:
			*x = column;
			*y = row;
			break;
		case MP_RESISTIVE_PORTRAIT_FLIPPED:
			*x = panel->columns - 1 - column;
			*y = panel->rows - 1 - row;
			break;
		case MP_RESISTIVE_LANDSCAPE_FLIPPED:
			*x = panel->rows - 1 - row;
			*y = column;
			break;
		default:
			*x = row;
			*y = panel->columns - 1 - column;
			break;
	}
}

static void _mp_resistive_release(mp_resistive_t *panel) {
	panel->seen = 0;
	panel->miss = 0;
	panel->tracked = 0;
	if(!panel->down)
		return;
	panel->down = 0;
	mp_device_input_touch(&panel->input, panel->input.x, panel->input.y, 0);
}

static int _mp_resistive_dropout(mp_resistive_t *panel) {
	if(panel->down && panel->miss < _MP_RESISTIVE_MISS) {
		panel->miss++;
		return(1);
	}
	_mp_resistive_release(panel);
	return(0);
}

/* a still finger wanders a few pixels; that used to shake every line while scrolling */
static void _mp_resistive_track(mp_resistive_t *panel, int x, int y) {
	int dx, dy;

	if(!panel->tracked) {
		panel->fx = x;
		panel->fy = y;
		panel->tracked = 1;
		mp_device_input_touch(&panel->input, x, y, 1);
		return;
	}

	panel->fx = (panel->fx * 3 + x) / 4;
	panel->fy = (panel->fy * 3 + y) / 4;
	dx = panel->fx - panel->input.x;
	dy = panel->fy - panel->input.y;
	if(dx < 0)
		dx = -dx;
	if(dy < 0)
		dy = -dy;
	if(dx < 3 && dy < 3)
		return;
	mp_device_input_touch(&panel->input, panel->fx, panel->fy, 1);
}

static void _mp_resistive_pen(void *user) {
	mp_resistive_t *panel = user;

	mp_gpio_interrupt_disable(panel->yd);
	mp_task_wake(panel->task);
}

/* the wiring of _mp_resistive_pressed() left in place, with a falling edge on YD */
static mp_bool_t _mp_resistive_arm(mp_resistive_t *panel) {
	_mp_resistive_float(panel->yu);
	_mp_resistive_float(panel->xr);
	_mp_resistive_drive(panel->xl, 0);
	_mp_resistive_pullup(panel->yd);
	if(mp_gpio_interrupt_set(panel->yd, _mp_resistive_pen, panel, "Touch pen") == FALSE)
		return(NO);
	mp_gpio_interrupt_hi2lo(panel->yd);
	mp_gpio_interrupt_enable(panel->yd);
	panel->armed = YES;
	return(YES);
}

static void _mp_resistive_disarm(mp_resistive_t *panel) {
	if(panel->armed == NO)
		return;
	mp_gpio_interrupt_unset(panel->yd);
	panel->armed = NO;
}

static void _mp_resistive_every(mp_task_t *task, unsigned long ticks) {
	if(task->signal != MP_TASK_SIG_OK || task->delay != ticks)
		mp_task_delay(task, ticks);
}

/* nobody touches: sample slower, and slower still with the pen armed while the screen is off */
static void _mp_resistive_rest(mp_resistive_t *panel, mp_task_t *task) {
	if(panel->input.dozing) {
		_mp_resistive_every(task, panel->doze);
		if(_mp_resistive_arm(panel) == YES)
			return;
	}
	_mp_resistive_every(task, panel->idle);
}

static MP_TASK(_mp_resistive_task) {
	mp_resistive_t *panel = task->user;
	int rawX, rawY, column, row, x, y;

	_mp_resistive_disarm(panel);

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(!_mp_resistive_pressed(panel)) {
		if(!_mp_resistive_dropout(panel))
			_mp_resistive_rest(panel, task);
		return;
	}
	_mp_resistive_every(task, panel->period);

	/* X runs from XL to XR and is read on YD; Y runs from YD to YU and is read on XL */
	rawX = _mp_resistive_sample(panel->xl, panel->xr, panel->yu, panel->yd, panel->channelYd);
	rawY = _mp_resistive_sample(panel->yd, panel->yu, panel->xr, panel->xl, panel->channelXl);
	if(rawX < 0 || rawY < 0 ||
		rawX < panel->xMin - _MP_RESISTIVE_EDGE || rawX > panel->xMax + _MP_RESISTIVE_EDGE ||
		rawY < panel->yMin - _MP_RESISTIVE_EDGE || rawY > panel->yMax + _MP_RESISTIVE_EDGE) {
		_mp_resistive_dropout(panel);
		return;
	}
	panel->miss = 0;

	column = _mp_resistive_scale(rawX, panel->xMin, panel->xMax, panel->columns);
	row = _mp_resistive_scale(rawY, panel->yMin, panel->yMax, panel->rows);
	_mp_resistive_map(panel, column, row, &x, &y);

	/* a light finger first gives a wrong place; two close samples are where it really is */
	if(!panel->down) {
		if(!panel->seen || _mp_resistive_far(x, panel->px, _MP_RESISTIVE_AGREE) || _mp_resistive_far(y, panel->py, _MP_RESISTIVE_AGREE)) {
			panel->seen = 1;
			panel->px = x;
			panel->py = y;
			return;
		}
		x = (x + panel->px) / 2;
		y = (y + panel->py) / 2;
		panel->down = 1;
	}
	/* a finger does not cross the screen in one period; that is a plate losing contact */
	else if(panel->tracked && (_mp_resistive_far(x, panel->fx, _MP_RESISTIVE_JUMP) || _mp_resistive_far(y, panel->fy, _MP_RESISTIVE_JUMP))) {
		_mp_resistive_dropout(panel);
		return;
	}
	_mp_resistive_track(panel, x, y);
}

static mp_ret_t _mp_resistive_load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	(void)kernel;
	(void)driver;
	(void)self;
	(void)options;
	return(TRUE);
}

static void _mp_resistive_unload(void *self) {
	_mp_resistive_pins_release(self);
}

static void _mp_resistive_start(void *self) {
	mp_resistive_t *panel = self;
	int width, height;

	if(mp_adc_open(panel->kernel) == FALSE)
		return;
	panel->adc = YES;

	if(panel->orientation == MP_RESISTIVE_PORTRAIT || panel->orientation == MP_RESISTIVE_PORTRAIT_FLIPPED) {
		width = panel->columns;
		height = panel->rows;
	}
	else {
		width = panel->rows;
		height = panel->columns;
	}
	mp_device_input_init(panel->kernel, &panel->input, MP_DEVICE_INPUT_CAP_TOUCH, "Touch");
	mp_device_input_bounds(&panel->input, width, height);
	_mp_resistive_plates(panel);

	panel->task = mp_task_create(&panel->kernel->tasks, "Touch", _mp_resistive_task, panel, panel->idle);
	if(!panel->task) {
		mp_device_input_fini(&panel->input);
		mp_adc_close();
		panel->adc = NO;
		return;
	}
	panel->input.task = panel->task;
}

static void _mp_resistive_stop(void *self) {
	mp_resistive_t *panel = self;

	if(panel->down)
		mp_device_input_touch(&panel->input, panel->input.x, panel->input.y, 0);
	if(panel->task) {
		mp_task_destroy(panel->task);
		panel->task = NULL;
	}
	_mp_resistive_disarm(panel);
	if(panel->input.kernel)
		mp_device_input_fini(&panel->input);
	if(panel->yd)
		_mp_resistive_plates(panel);
	if(panel->adc == YES) {
		mp_adc_close();
		panel->adc = NO;
	}
}

mp_ret_t mp_resistive_init(mp_kernel_t *kernel, mp_resistive_t *panel) {
	static const mp_device_driver_ops_t ops = {
		"touch/resistive", "4-wire resistive touch", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_TOUCH),
		_mp_resistive_load, _mp_resistive_unload, NULL,
		_mp_resistive_start, _mp_resistive_stop
	};
	const char *orient;

	if(!kernel || !panel)
		return(FALSE);
	panel->kernel = kernel;
	if(!panel->yd || !panel->yu || !panel->xl || !panel->xr)
		goto fail;
	if(!panel->model || !panel->model[0] || !panel->serial || !panel->serial[0])
		goto fail;
	if(panel->columns < 2 || panel->rows < 2 || panel->xMax <= panel->xMin || panel->yMax <= panel->yMin)
		goto fail;

	panel->channelYd = mp_adc_channel(panel->yd);
	panel->channelXl = mp_adc_channel(panel->xl);
	if(panel->channelYd < 0 || panel->channelXl < 0) {
		mp_printk("Resistive: YD and XL must be ADC pins");
		goto fail;
	}

	panel->period = _mp_resistive_ticks(panel->period ? panel->period : 20);
	panel->idle = _mp_resistive_ticks(panel->idle ? panel->idle : 100);
	panel->doze = _mp_resistive_ticks(panel->doze ? panel->doze : 50);

	if(mp_device_driver_register(kernel, &panel->binding, &ops, panel, NULL) == FALSE)
		goto fail;
	if(!mp_device_registry_register(&panel->binding, &panel->touch, MP_DEVICE_TOUCH, 0, &panel->input, panel->model, panel->serial, NULL)) {
		mp_device_driver_unregister(kernel, &panel->binding);
		return(FALSE);
	}
	panel->touch.label = "Resistive touch";

	if((unsigned)panel->orientation >= sizeof(_mp_resistive_orientation) / sizeof(_mp_resistive_orientation[0]))
		panel->orientation = MP_RESISTIVE_LANDSCAPE;
	orient = _mp_resistive_orientation[panel->orientation];
	mp_printk("Resistive touch %s, plates on P%c%u P%c%u P%c%u P%c%u",
		orient,
		'A' + panel->yd->port, panel->yd->pin,
		'A' + panel->yu->port, panel->yu->pin,
		'A' + panel->xl->port, panel->xl->pin,
		'A' + panel->xr->port, panel->xr->pin);
	return(TRUE);

fail:
	_mp_resistive_pins_release(panel);
	return(FALSE);
}

void mp_resistive_fini(mp_resistive_t *panel) {
	if(!panel || !panel->kernel)
		return;
	mp_device_registry_unregister(&panel->touch);
	mp_device_driver_unregister(panel->kernel, &panel->binding);
	panel->kernel = NULL;
}

#endif
