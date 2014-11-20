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

#ifndef _HAVE_MP_DRV_BUTTON_H
	#define _HAVE_MP_DRV_BUTTON_H

	#ifdef SUPPORT_DRV_BUTTON
		typedef struct mp_drv_button_s mp_drv_button_t;
		typedef struct mp_drv_button_event_s mp_drv_button_event_t;

		typedef void (*mp_drv_button_on_t)(void *user);
		typedef void (*mp_drv_button_event_on_t)(void *user);

		struct mp_drv_button_s {
			/** user callback */
			mp_drv_button_on_t onSwitch;

			/** user pointer */
			void *user;

			/** is pressed YES/NO */
			char pressed;

			/** how many time it was pressed */
			unsigned long pressDelay;

			/** GPIO port */
			mp_gpio_port_t *gpio;

			/** list of sub button events */
			mp_list_t events;
		};

		struct mp_drv_button_event_s {
			/** configured delay */
			int delay;

			/** configured time count */
			int time;

			/** applyied callback */
			mp_drv_button_event_on_t cb;

			/** user pointer */
			void *user;

			/** how many down time */
			int howManyDown;

			/** internal delay between each button down */
			unsigned long downDelay;

			/** button link pointer */
			mp_drv_button_t *button;

			/** list link with button */
			mp_list_item_t item;
		};


		mp_ret_t mp_drv_button_init(mp_kernel_t *kernel, mp_drv_button_t *button, mp_options_t *options, char *who);
		mp_ret_t mp_drv_button_fini(mp_drv_button_t *button);

		mp_ret_t mp_drv_button_event_create(
			mp_drv_button_t *button, mp_drv_button_event_t *bac,
			int delay, int time, mp_drv_button_event_on_t cb, void *user
		);
		mp_ret_t mp_drv_button_event_destroy(mp_drv_button_event_t *bac);
	#endif

#endif
