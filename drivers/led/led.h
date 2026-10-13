/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

#ifdef SUPPORT_DRV_LED

#ifndef _HAVE_MP_DRV_LED_H
#define _HAVE_MP_DRV_LED_H

	typedef struct mp_drv_led_s mp_drv_led_t;

	struct mp_drv_led_s {
		mp_gpio_port_t *gpio;

		mp_bool_t state;

		/** in some bad case the set/unset must be reverted */
		mp_bool_t reverted;
	};

	mp_ret_t mp_drv_led_init(mp_kernel_t *kernel, mp_drv_led_t *led, mp_options_t *options, char *who);
	mp_ret_t mp_drv_led_fini(mp_drv_led_t *led);
	void mp_drv_led_turnOff(mp_drv_led_t *led);
	void mp_drv_led_turnOn(mp_drv_led_t *led);
	void mp_drv_led_turn(mp_drv_led_t *led);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_LED */
	extern const mp_device_driver_t mp_drv_led_device;
#endif

#endif

#endif
