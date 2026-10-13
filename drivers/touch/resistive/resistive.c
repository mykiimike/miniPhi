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

#ifdef SUPPORT_DRV_TOUCH_RESISTIVE

/**
@defgroup mpDrvResistive 4-wire resistive touch

@ingroup mpDriver

@{
*/

/* the glass is 240 columns by 320 rows; orientation turns that into the display */
#define RESISTIVE_COLUMNS 240
#define RESISTIVE_ROWS    320

/* an open plate sits at a rail; a finger reads between these */
#define RESISTIVE_RAW_MIN 80
#define RESISTIVE_RAW_MAX 4015

/* open plate is near 4095; a light touch only has to pull it down from there */
#define RESISTIVE_TOUCH_MAX 3200

/* a light contact is noisier than a hard one */
#define RESISTIVE_SPREAD 350

/* one bad sample does not mean the finger left */
#define RESISTIVE_MISS 4

static const char *_orientations[] = { "landscape", "portrait", "landscape-flipped", "portrait-flipped" };

static MP_TASK(_resistive_task);

static unsigned long _resistive_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks = hz ? (ms * hz) / 1000 : ms;

	return(ticks ? ticks : 1);
}

/* the plate needs time to settle; a light touch is a high resistance and a slow edge */
static void _resistive_settle(int spins) {
	volatile int count = spins;

	while(count)
		count--;
}

static int _resistive_channel(mp_gpio_port_t *pin) {
	/* STM32F1: PA0-PA7 are IN0-IN7, PB0-PB1 are IN8-IN9, PC0-PC5 are IN10-IN15 */
	if(pin->port == 0 && pin->pin <= 7)
		return((int)pin->pin);
	if(pin->port == 1 && pin->pin <= 1)
		return(8 + (int)pin->pin);
	if(pin->port == 2 && pin->pin <= 5)
		return(10 + (int)pin->pin);
	return(-1);
}

static mp_gpio_port_t *_resistive_pin(mp_options_t *options, char *key, char *who) {
	char *value = mp_options_get(options, key);

	if(!value || !*value)
		return(NULL);
	return(mp_gpio_text_handle(value, who));
}

static int _resistive_number(mp_options_t *options, char *key, int fallback) {
	char *value = mp_options_get(options, key);

	return(value && *value ? atoi(value) : fallback);
}

static void _resistive_drive(mp_gpio_port_t *pin, int high) {
	mp_gpio_direction(pin, MP_GPIO_OUTPUT);
	if(high)
		mp_gpio_set(pin);
	else
		mp_gpio_unset(pin);
}

static void _resistive_float(mp_gpio_port_t *pin) {
	mp_gpio_mode(pin, MP_GPIO_MODE_INPUT);
}

static void _resistive_analog(mp_gpio_port_t *pin) {
	mp_gpio_mode(pin, MP_GPIO_MODE_ANALOG);
}

/* ODR high selects the pull-up */
static void _resistive_pullup(mp_gpio_port_t *pin) {
	mp_gpio_set(pin);
	mp_gpio_mode(pin, MP_GPIO_MODE_INPUT_PULL);
}

/* XL low and YD pulled up, then read on the ADC: a digital input only sees a hard press */
static int _resistive_pressed(mp_drv_resistive_t *panel) {
	int level;

	_resistive_float(panel->yu);
	_resistive_float(panel->xr);
	_resistive_drive(panel->xl, 0);
	_resistive_pullup(panel->yd);
	_resistive_settle(6000);
	_resistive_analog(panel->yd);
	_resistive_settle(400);
	level = mp_adc_convert(panel->channelYd);
	return(level >= 0 && level < RESISTIVE_TOUCH_MAX);
}

/* drive one plate between low and high, measure the other */
static int _resistive_sample(mp_gpio_port_t *low, mp_gpio_port_t *high, mp_gpio_port_t *idle, mp_gpio_port_t *sense, int channel) {
	int first, second, delta;

	_resistive_drive(low, 0);
	_resistive_drive(high, 1);
	_resistive_float(idle);
	_resistive_analog(sense);
	_resistive_settle(2500);

	first = mp_adc_convert(channel);
	second = mp_adc_convert(channel);
	if(first < 0 || second < 0)
		return(-1);
	delta = first - second;
	if(delta < 0)
		delta = -delta;
	if(delta > RESISTIVE_SPREAD)
		return(-1);
	if(first < RESISTIVE_RAW_MIN || first > RESISTIVE_RAW_MAX)
		return(-1);
	return((first + second) / 2);
}

static int _resistive_scale(int raw, int min, int max, int span) {
	if(span <= 1 || max <= min)
		return(0);
	if(raw < min)
		raw = min;
	if(raw > max)
		raw = max;
	return((raw - min) * (span - 1) / (max - min));
}

/* same geometry as the ILI932x: landscape puts panel rows across the display */
static void _resistive_map(mp_drv_resistive_t *panel, int column, int row, int *x, int *y) {
	if(panel->flipX)
		column = RESISTIVE_COLUMNS - 1 - column;
	if(panel->flipY)
		row = RESISTIVE_ROWS - 1 - row;

	switch(panel->orientation) {
		case MP_DRV_RESISTIVE_PORTRAIT:
			*x = column;
			*y = row;
			break;
		case MP_DRV_RESISTIVE_PORTRAIT_FLIPPED:
			*x = RESISTIVE_COLUMNS - 1 - column;
			*y = RESISTIVE_ROWS - 1 - row;
			break;
		case MP_DRV_RESISTIVE_LANDSCAPE_FLIPPED:
			*x = RESISTIVE_ROWS - 1 - row;
			*y = column;
			break;
		default:
			*x = row;
			*y = RESISTIVE_COLUMNS - 1 - column;
			break;
	}
}

static void _resistive_release(mp_drv_resistive_t *panel) {
	panel->seen = 0;
	panel->miss = 0;
	panel->tracked = 0;
	if(!panel->down)
		return;
	panel->down = 0;
	mp_input_touch(&panel->input, panel->input.x, panel->input.y, 0);
}

static int _resistive_dropout(mp_drv_resistive_t *panel) {
	if(panel->down && panel->miss < RESISTIVE_MISS) {
		panel->miss++;
		return(1);
	}
	_resistive_release(panel);
	return(0);
}

/* a still finger wanders a few pixels; that used to shake every line while scrolling */
static void _resistive_track(mp_drv_resistive_t *panel, int x, int y) {
	int dx, dy;

	if(!panel->tracked) {
		panel->fx = x;
		panel->fy = y;
		panel->tracked = 1;
		mp_input_touch(&panel->input, x, y, 1);
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
	mp_input_touch(&panel->input, panel->fx, panel->fy, 1);
}

static MP_TASK(_resistive_task) {
	mp_drv_resistive_t *panel = task->user;
	int rawX, rawY, column, row, x, y;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(!_resistive_pressed(panel)) {
		_resistive_dropout(panel);
		return;
	}

	/* X runs from XL to XR and is read on YD; Y runs from YD to YU and is read on XL */
	rawX = _resistive_sample(panel->xl, panel->xr, panel->yu, panel->yd, panel->channelYd);
	rawY = _resistive_sample(panel->yd, panel->yu, panel->xr, panel->xl, panel->channelXl);
	if(rawX < 0 || rawY < 0) {
		_resistive_dropout(panel);
		return;
	}
	panel->miss = 0;

	column = _resistive_scale(rawX, panel->xMin, panel->xMax, RESISTIVE_COLUMNS);
	row = _resistive_scale(rawY, panel->yMin, panel->yMax, RESISTIVE_ROWS);
	_resistive_map(panel, column, row, &x, &y);

	/* the first sample only confirms the press, so a spike does not click */
	if(!panel->down) {
		if(++panel->seen < 2)
			return;
		panel->down = 1;
	}
	_resistive_track(panel, x, y);
}

static void _resistive_idle(mp_drv_resistive_t *panel) {
	_resistive_float(panel->yu);
	_resistive_float(panel->xl);
	_resistive_float(panel->xr);
	_resistive_float(panel->yd);
}

mp_ret_t mp_drv_resistive_init(mp_kernel_t *kernel, mp_drv_resistive_t *panel, mp_options_t *options, char *who) {
	char *value;
	unsigned int index;
	int width, height;
	unsigned long period;

	memset(panel, 0, sizeof(*panel));
	panel->kernel = kernel;
	panel->xMin = _resistive_number(options, "xMin", 250);
	panel->xMax = _resistive_number(options, "xMax", 3850);
	panel->yMin = _resistive_number(options, "yMin", 250);
	panel->yMax = _resistive_number(options, "yMax", 3850);
	panel->flipX = mp_options_get(options, "flipX") != NULL;
	panel->flipY = mp_options_get(options, "flipY") != NULL;

	value = mp_options_get(options, "orientation");
	for(index = 0; value && index < sizeof(_orientations) / sizeof(_orientations[0]); index++)
		if(mp_options_cmp((char *)_orientations[index], value) == TRUE)
			panel->orientation = (mp_drv_resistive_orientation_t)index;

	panel->yd = _resistive_pin(options, "yd", who);
	panel->yu = _resistive_pin(options, "yu", who);
	panel->xl = _resistive_pin(options, "xl", who);
	panel->xr = _resistive_pin(options, "xr", who);
	if(!panel->yd || !panel->yu || !panel->xl || !panel->xr)
		goto fail;
	panel->channelYd = _resistive_channel(panel->yd);
	panel->channelXl = _resistive_channel(panel->xl);
	if(panel->channelYd < 0 || panel->channelXl < 0) {
		mp_printk("RESISTIVE: YD and XL must be ADC pins");
		goto fail;
	}

	if(mp_adc_open(kernel) == FALSE)
		goto fail;

	if(panel->orientation & 1) {
		width = RESISTIVE_COLUMNS;
		height = RESISTIVE_ROWS;
	}
	else {
		width = RESISTIVE_ROWS;
		height = RESISTIVE_COLUMNS;
	}
	mp_input_init(kernel, &panel->input, MP_INPUT_CAP_TOUCH, who);
	mp_input_bounds(&panel->input, width, height);
	_resistive_idle(panel);

	period = _resistive_ticks((unsigned long)_resistive_number(options, "period", 20));
	panel->task = mp_task_create(&kernel->tasks, "Touch", _resistive_task, panel, period);
	if(!panel->task) {
		mp_input_fini(&panel->input);
		mp_adc_close();
		goto fail;
	}

	mp_printk("Resistive touch %s, plates on P%c%u P%c%u P%c%u P%c%u",
		_orientations[panel->orientation],
		'A' + panel->yd->port, panel->yd->pin,
		'A' + panel->yu->port, panel->yu->pin,
		'A' + panel->xl->port, panel->xl->pin,
		'A' + panel->xr->port, panel->xr->pin);
	return(TRUE);

fail:
	if(panel->yd) mp_gpio_release(panel->yd);
	if(panel->yu) mp_gpio_release(panel->yu);
	if(panel->xl) mp_gpio_release(panel->xl);
	if(panel->xr) mp_gpio_release(panel->xr);
	return(FALSE);
}

mp_ret_t mp_drv_resistive_fini(mp_drv_resistive_t *panel) {
	if(panel->down)
		mp_input_touch(&panel->input, panel->input.x, panel->input.y, 0);
	if(panel->task)
		mp_task_destroy(panel->task);
	if(panel->input.kernel)
		mp_input_fini(&panel->input);
	_resistive_idle(panel);
	if(panel->yd) mp_gpio_release(panel->yd);
	if(panel->yu) mp_gpio_release(panel->yu);
	if(panel->xl) mp_gpio_release(panel->xl);
	if(panel->xr) mp_gpio_release(panel->xr);
	mp_adc_close();
	panel->kernel = NULL;
	return(TRUE);
}

/**@}*/

#endif
