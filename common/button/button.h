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

#ifdef SUPPORT_COMMON_BUTTON

#ifndef _HAVE_MP_COMMON_BUTTON_H
	#define _HAVE_MP_COMMON_BUTTON_H

	/**
	 * One GPIO button, with no controller chip.
	 * The board owns the pin and the polarity. This module arms the interrupt
	 * and registers one MP_DEVICE_BUTTON. The first subscriber starts it.
	 * face.active is 0 for active low, 1 for active high.
	 */

	typedef struct mp_button_s mp_button_t;

	struct mp_button_s {
		mp_device_button_t face;
		mp_kernel_t *kernel;
		mp_task_t *task;
		mp_device_driver_t binding;
		mp_device_registry_t device;

		mp_gpio_port_t *pin;
		/** address buttonN.model.serial */
		const char *model;
		const char *serial;

		/** last raw pin level, written from the interrupt */
		volatile int level;
		mp_bool_t irq;
	};

	/**
	 * Register the button. The pin is already claimed; this call owns it and
	 * releases it on failure. FALSE when the pin, the model or the serial is missing.
	 */
	mp_ret_t mp_button_init(mp_kernel_t *kernel, mp_button_t *button);
	void mp_button_fini(mp_button_t *button);

	/**
	 * The board fills one mp_button_t per button and calls mp_button_init.
	 * The kernel calls this once the machine is up.
	 */
	mp_ret_t mp_board_button(mp_kernel_t *kernel);

#endif

#endif
