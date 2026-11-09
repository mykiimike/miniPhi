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

#ifndef _HAVE_MP_BOARD_DIAL_H
#define _HAVE_MP_BOARD_DIAL_H

/*
 * Calibrated from the console dump on this PowerBread (raw ADC, 12-bit):
 *   rest 2100, press 2600, down 2900, up 3500.
 * Bands meet at the midpoints (2350, 2750, 3200) which act as
 * hysteresis gaps: a value in a gap keeps the previous state.
 */
#ifndef MP_DIAL_REST_LO
#define MP_DIAL_REST_LO 0
#endif
#ifndef MP_DIAL_REST_HI
#define MP_DIAL_REST_HI 2349
#endif
#ifndef MP_DIAL_PRESS_LO
#define MP_DIAL_PRESS_LO 2351
#endif
#ifndef MP_DIAL_PRESS_HI
#define MP_DIAL_PRESS_HI 2749
#endif
#ifndef MP_DIAL_DOWN_LO
#define MP_DIAL_DOWN_LO 2751
#endif
#ifndef MP_DIAL_DOWN_HI
#define MP_DIAL_DOWN_HI 3199
#endif
#ifndef MP_DIAL_UP_LO
#define MP_DIAL_UP_LO 3201
#endif
#ifndef MP_DIAL_UP_HI
#define MP_DIAL_UP_HI 4095
#endif

/* calibration dump off once the bands are set */
#ifndef MP_DIAL_CALIBRATE
#define MP_DIAL_CALIBRATE 0
#endif

typedef enum {
	MP_DIAL_REST = 0,
	MP_DIAL_UP,
	MP_DIAL_DOWN,
	MP_DIAL_PRESS,
} mp_dial_event_t;

#ifndef MP_DIAL_T
#define MP_DIAL_T
typedef struct mp_dial_s mp_dial_t;
#endif

struct mp_dial_s {
	mp_kernel_t *kernel;
	mp_task_t *task;
	int channel;
	int raw;
	mp_dial_event_t state;
	int up_lo, up_hi;
	int down_lo, down_hi;
	int press_lo, press_hi;
	int rest_lo, rest_hi;
	int pending;
	int hold;
	int primed;
	int dump;
};

/**
 * The static dial on pa2. NULL when the sampling task cannot start.
 * The pad is analog: this does not claim it through the GPIO handle.
 * Up, down and press are three MP_DEVICE_BUTTON devices (serials
 * "dial up", "dial down", "dial press"). Rest is not a button.
 */
mp_dial_t *mp_board_dial(mp_kernel_t *kernel);

#endif
