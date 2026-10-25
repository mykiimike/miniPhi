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

#ifdef SUPPORT_COMMON_RESISTIVE

#ifndef _HAVE_MP_COMMON_RESISTIVE_H
	#define _HAVE_MP_COMMON_RESISTIVE_H

	/**
	 * 4-wire resistive sheet, with no controller chip.
	 * The board owns the glass: the four pins, the size, the calibration and the
	 * orientation. This module drives the plates, reads the two ADC channels and
	 * registers one MP_DEVICE_TOUCH. The first subscriber starts the sampling.
	 */

	typedef enum {
		MP_RESISTIVE_LANDSCAPE,
		MP_RESISTIVE_PORTRAIT,
		MP_RESISTIVE_LANDSCAPE_FLIPPED,
		MP_RESISTIVE_PORTRAIT_FLIPPED,
	} mp_resistive_orientation_t;

	typedef struct mp_resistive_s mp_resistive_t;

	struct mp_resistive_s {
		mp_device_input_t input;
		mp_kernel_t *kernel;
		mp_task_t *task;
		mp_device_driver_t binding;
		mp_device_registry_t touch;

		mp_gpio_port_t *yd;
		mp_gpio_port_t *yu;
		mp_gpio_port_t *xl;
		mp_gpio_port_t *xr;
		int channelYd;
		int channelXl;

		mp_resistive_orientation_t orientation;
		/** panel columns and rows, before orientation turns them into the display */
		int columns;
		int rows;
		int flipX;
		int flipY;
		/** raw ADC window, 0 to 4095 */
		int xMin, xMax, yMin, yMax;

		/** address touchN.model.serial */
		const char *model;
		const char *serial;

		int down;
		int seen;
		int miss;
		int px, py;
		int fx, fy;
		int tracked;

		/**
		 * Milliseconds until mp_resistive_init, ticks after it.
		 * 0 selects 20, 100 and 50.
		 */
		unsigned long period;
		unsigned long idle;
		unsigned long doze;
		mp_bool_t armed;
		mp_bool_t adc;
	};

	/**
	 * Register the panel. The four pins are already claimed; this call owns them
	 * and releases them on failure. FALSE when a pin is missing, YD or XL is not
	 * an ADC pin, or the glass has no size.
	 */
	mp_ret_t mp_resistive_init(mp_kernel_t *kernel, mp_resistive_t *panel);
	void mp_resistive_fini(mp_resistive_t *panel);

	/**
	 * The board fills one mp_resistive_t for its glass and calls mp_resistive_init.
	 * The kernel calls this once the machine is up.
	 */
	mp_ret_t mp_board_resistive(mp_kernel_t *kernel);

#endif

#endif
