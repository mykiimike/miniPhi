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

#ifdef SUPPORT_COMMON_BUTTON

/**
 * Olimex MSP430-5438-STK joystick. Five active-low buttons on port 1.
 */

static mp_button_t _mp_board_buttons[5];

static mp_ret_t _mp_board_button_one(mp_kernel_t *kernel, int index, char *pin, const char *serial) {
	mp_button_t *button = &_mp_board_buttons[index];

	memset(button, 0, sizeof(*button));
	button->pin = mp_gpio_text_handle(pin, "Button");
	if(!button->pin) {
		mp_printk("MSP430-5438 button %s: %s is not free", serial, pin);
		return(FALSE);
	}
	button->model = "gpio";
	button->serial = serial;
	return(mp_button_init(kernel, button));
}

mp_ret_t mp_board_button(mp_kernel_t *kernel) {
	mp_ret_t ok = TRUE;

	if(_mp_board_button_one(kernel, 0, "p1.6", "left") == FALSE)
		ok = FALSE;
	if(_mp_board_button_one(kernel, 1, "p1.5", "right") == FALSE)
		ok = FALSE;
	if(_mp_board_button_one(kernel, 2, "p1.4", "center") == FALSE)
		ok = FALSE;
	if(_mp_board_button_one(kernel, 3, "p1.3", "up") == FALSE)
		ok = FALSE;
	if(_mp_board_button_one(kernel, 4, "p1.2", "down") == FALSE)
		ok = FALSE;
	return(ok);
}

#endif
