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
 * Olimex STM32-E407 WKUP. PA0, active high.
 */

static mp_button_t _mp_board_wkup;

mp_ret_t mp_board_button(mp_kernel_t *kernel) {
	memset(&_mp_board_wkup, 0, sizeof(_mp_board_wkup));
	_mp_board_wkup.pin = mp_gpio_text_handle("pa0", "Button");
	if(!_mp_board_wkup.pin) {
		mp_printk("STM32-E407 button: PA0 is not free");
		return(FALSE);
	}
	_mp_board_wkup.face.active = 1;
	_mp_board_wkup.model = "gpio";
	_mp_board_wkup.serial = "wkup";
	return(mp_button_init(kernel, &_mp_board_wkup));
}

#endif
