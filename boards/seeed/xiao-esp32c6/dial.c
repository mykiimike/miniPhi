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
#include "dial.h"

/**
 * PowerBread resistive dial on D2 (pa2, ADC1 channel 2). One sample is
 * thresholded into up, down, press or rest. A change that has held for a
 * few samples presses the new face and releases the previous one. Rest has
 * no face. The first stable reading sets the state and presses nothing.
 */

#define _DIAL_GPIO    2
#define _DIAL_HOLD    3
#define _DIAL_PERIOD  20UL
#define _DIAL_DUMP    10

static mp_dial_t _dial;

#ifdef SUPPORT_COMMON_DEVICE

static mp_device_button_t _faces[3];
static mp_device_driver_t _binding[3];
static mp_device_registry_t _device[3];

/* index is mp_dial_event_t. Rest has no face. */
static mp_device_button_t *_state_face[] = {
	NULL,
	&_faces[0],
	&_faces[1],
	&_faces[2],
};

static mp_ret_t _dial_load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	(void)kernel;
	(void)driver;
	(void)self;
	(void)options;
	return(TRUE);
}

static void _dial_buttons(mp_kernel_t *kernel) {
	static int done;
	static const mp_device_driver_ops_t ops = {
		"button/dial", "Dial button", "1.0.0",
		(unsigned int)(1u << MP_DEVICE_BUTTON),
		_dial_load, NULL, NULL, NULL, NULL
	};
	static const char *serial[3] = { "dial up", "dial down", "dial press" };
	int i;

	if(done)
		return;
	for(i = 0; i < 3; i++) {
		if(mp_device_driver_register(kernel, &_binding[i], &ops, &_faces[i], NULL) == FALSE) {
			mp_printk("dial: button %s driver refused", serial[i]);
			return;
		}
		if(!mp_device_registry_register(&_binding[i], &_device[i], MP_DEVICE_BUTTON, 0, &_faces[i], "dial", serial[i], NULL)) {
			mp_device_driver_unregister(kernel, &_binding[i]);
			mp_printk("dial: button %s refused", serial[i]);
			return;
		}
		_device[i].label = "Dial button";
		mp_printk("Dial button %s", serial[i]);
	}
	done = 1;
}

/* press the new face, then release the previous one. Rest releases only. */
static void _dial_edges(int prev, int band) {
	mp_device_button_t *nextFace = (band >= 0 && band < 4) ? _state_face[band] : NULL;
	mp_device_button_t *prevFace = (prev >= 0 && prev < 4) ? _state_face[prev] : NULL;

	if(nextFace)
		mp_device_button_edge(nextFace, 1);
	if(prevFace)
		mp_device_button_edge(prevFace, 0);
}

#endif

static unsigned long _ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);
	ticks = (ms / 1000UL) * hz + ((ms % 1000UL) * hz) / 1000UL;
	return(ticks ? ticks : 1);
}

/* -1 is the gap between bands: the previous state stays */
static int _band(mp_dial_t *dial, int raw) {
	if(raw >= dial->press_lo && raw <= dial->press_hi)
		return(MP_DIAL_PRESS);
	if(raw >= dial->down_lo && raw <= dial->down_hi)
		return(MP_DIAL_DOWN);
	if(raw >= dial->up_lo && raw <= dial->up_hi)
		return(MP_DIAL_UP);
	if(raw >= dial->rest_lo && raw <= dial->rest_hi)
		return(MP_DIAL_REST);
	return(-1);
}

static const char *_band_name(int band) {
	if(band == MP_DIAL_UP)
		return("up");
	if(band == MP_DIAL_DOWN)
		return("down");
	if(band == MP_DIAL_PRESS)
		return("press");
	if(band == MP_DIAL_REST)
		return("rest");
	return("gap");
}

static void _apply(mp_dial_t *dial, int band) {
	if(band < 0) {
		dial->hold = 0;
		return;
	}
	if(!dial->primed) {
		if(band != dial->pending) {
			dial->pending = band;
			dial->hold = 1;
			return;
		}
		dial->hold++;
		if(dial->hold < _DIAL_HOLD)
			return;
		dial->state = (mp_dial_event_t)band;
		dial->hold = 0;
		dial->primed = 1;
		return;
	}
	if(band == (int)dial->state) {
		dial->hold = 0;
		dial->pending = band;
		return;
	}
	if(band != dial->pending) {
		dial->pending = band;
		dial->hold = 1;
		return;
	}
	dial->hold++;
	if(dial->hold < _DIAL_HOLD)
		return;
	{
		int prev = (int)dial->state;

		dial->state = (mp_dial_event_t)band;
		dial->hold = 0;
#ifdef SUPPORT_COMMON_DEVICE
		_dial_edges(prev, band);
#else
		(void)prev;
#endif
	}
}

static MP_TASK(_dial_task) {
	mp_dial_t *dial = task->user;
	int raw;
	int band;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	raw = mp_adc_read_gpio(dial->channel);
	dial->raw = raw;
	if(raw >= 0) {
		band = _band(dial, raw);
		_apply(dial, band);
	}
	else
		band = -1;

#if MP_DIAL_CALIBRATE
	/* print every sample so every position (incl. gaps) shows a line */
	if(raw >= 0)
		mp_printk("dial cal raw=%d state=%s", raw, _band_name(_band(dial, raw)));
#endif

	mp_task_delay(task, _ticks(_DIAL_PERIOD));
}

mp_dial_t *mp_board_dial(mp_kernel_t *kernel) {
	mp_dial_t *dial = &_dial;

	if(!kernel)
		return(NULL);
	if(dial->task)
		return(dial);

	memset(dial, 0, sizeof(*dial));
	dial->kernel = kernel;
	dial->channel = _DIAL_GPIO;
	dial->state = MP_DIAL_REST;
	dial->pending = -1;
	dial->press_lo = MP_DIAL_PRESS_LO;
	dial->press_hi = MP_DIAL_PRESS_HI;
	dial->down_lo = MP_DIAL_DOWN_LO;
	dial->down_hi = MP_DIAL_DOWN_HI;
	dial->up_lo = MP_DIAL_UP_LO;
	dial->up_hi = MP_DIAL_UP_HI;
	dial->rest_lo = MP_DIAL_REST_LO;
	dial->rest_hi = MP_DIAL_REST_HI;

#ifdef SUPPORT_COMMON_DEVICE
	_dial_buttons(kernel);
#endif

	dial->task = mp_task_create(&kernel->tasks, "Dial", _dial_task, dial, _ticks(_DIAL_PERIOD));
	if(!dial->task) {
		mp_printk("dial: no task");
		return(NULL);
	}
	mp_printk("dial on pa2, cal raw follows");
	return(dial);
}
