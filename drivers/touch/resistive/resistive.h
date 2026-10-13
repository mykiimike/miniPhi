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

#ifndef _HAVE_MP_DRV_RESISTIVE_H
	#define _HAVE_MP_DRV_RESISTIVE_H

	/**
	 * 4-wire resistive touch panel on two ADC channels.
	 * The driver is an mp_input_t with MP_INPUT_CAP_TOUCH: it feeds
	 * mp_input_touch(), and the TUI reads it with the other inputs.
	 *
	 * One plate is driven, the other is measured. YD and YU are one plate,
	 * XL and XR the other, each on an ADC-capable pin. There is no
	 * controller chip and no pen interrupt: the driver notices a press
	 * because the plates connect.
	 */

	typedef enum {
		MP_DRV_RESISTIVE_LANDSCAPE,
		MP_DRV_RESISTIVE_PORTRAIT,
		MP_DRV_RESISTIVE_LANDSCAPE_FLIPPED,
		MP_DRV_RESISTIVE_PORTRAIT_FLIPPED,
	} mp_drv_resistive_orientation_t;

	typedef struct mp_drv_resistive_s mp_drv_resistive_t;

	struct mp_drv_resistive_s {
		mp_input_t input;
		mp_kernel_t *kernel;
		mp_task_t *task;

		mp_gpio_port_t *yd;
		mp_gpio_port_t *yu;
		mp_gpio_port_t *xl;
		mp_gpio_port_t *xr;
		int channelYd;
		int channelXl;

		mp_drv_resistive_orientation_t orientation;
		int flipX;
		int flipY;
		int xMin, xMax, yMin, yMax;

		int down;
		int seen;
		int miss;
		/** last position sent, after the plate noise is filtered out */
		int fx, fy;
		int tracked;
	};

	/**
	 * options: yd, yu, xl, xr (ADC pins), orientation (landscape, portrait,
	 * landscape-flipped, portrait-flipped), flipX, flipY, xMin, xMax, yMin,
	 * yMax (raw ADC window, 0 to 4095), period in ms.
	 */
	mp_ret_t mp_drv_resistive_init(mp_kernel_t *kernel, mp_drv_resistive_t *panel, mp_options_t *options, char *who);
	mp_ret_t mp_drv_resistive_fini(mp_drv_resistive_t *panel);

#endif
