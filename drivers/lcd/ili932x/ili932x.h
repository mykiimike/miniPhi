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

#ifndef _HAVE_MP_DRV_ILI932X_H
	#define _HAVE_MP_DRV_ILI932X_H

	/**
	 * ILI9320 and ILI9325 240x320 TFT on the FSMC parallel bus.
	 * The driver is an mp_tui_display_t: give &lcd->display to mp_tui_init().
	 */

	typedef enum {
		MP_DRV_ILI932X_LANDSCAPE,
		MP_DRV_ILI932X_PORTRAIT,
		MP_DRV_ILI932X_LANDSCAPE_FLIPPED,
		MP_DRV_ILI932X_PORTRAIT_FLIPPED,
	} mp_drv_ili932x_orientation_t;

	typedef struct mp_drv_ili932x_s mp_drv_ili932x_t;

	struct mp_drv_ili932x_s {
		mp_tui_display_t display;
		mp_kernel_t *kernel;
		mp_fsmc_t bus;
		mp_gpio_port_t *reset;
		/** the backlight pin, when it is not on a PWM */
		mp_gpio_port_t *backlight;
		mp_drv_ili932x_orientation_t orientation;
		/** register 0x00: 0x9320 or 0x9325 */
		uint16_t id;

		/** backlightTimer: the pin is a PWM channel */
		mp_bool_t dimmable;
		mp_timer_t timer;
		mp_pwm_t pwm;
		int brightness;
		mp_power_hold_t dimmed;

		/** the panel is in standby, see mp_tui_display_t.power */
		mp_bool_t standby;
	};

	/**
	 * options: the fsmc bus ones, reset and backlight pins (both optional),
	 * backlightLow, backlightTimer (the timer whose channel is the backlight
	 * pin, to dim it), orientation (landscape, portrait, landscape-flipped,
	 * portrait-flipped).
	 */
	mp_ret_t mp_drv_ili932x_init(mp_kernel_t *kernel, mp_drv_ili932x_t *lcd, mp_options_t *options, char *who);
	mp_ret_t mp_drv_ili932x_fini(mp_drv_ili932x_t *lcd);
	void mp_drv_ili932x_backlight(mp_drv_ili932x_t *lcd, mp_bool_t on);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_DISPLAY */
	extern const mp_device_driver_t mp_drv_ili932x_device;
#endif

#endif
