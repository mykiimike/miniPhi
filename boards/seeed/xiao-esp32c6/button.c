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
 * The bare XIAO ESP32-C6 module carries no button. The Seeed Studio XIAO
 * Expansion Board v1.2 adds one active-low user button on D1 (pa1). The
 * PowerBread hat adds a second active-low button on D0 (pa0). A press
 * pulls the pad low; an internal pull-up keeps it high at rest. PUI reads
 * D1 as "next" (advance) and D0 as "prev" (step back); the prev button is
 * optional, so a board with only D1 still works.
 */

static mp_button_t _mp_board_button_next;
static mp_button_t _mp_board_button_prev;

mp_ret_t mp_board_button(mp_kernel_t *kernel) {
	mp_button_t *next = &_mp_board_button_next;
	mp_button_t *prev = &_mp_board_button_prev;

	memset(next, 0, sizeof(*next));
	next->pin = mp_gpio_text_handle("pa1", "Button");
	if(!next->pin) {
		mp_printk("XIAO button: pa1 is not free");
		return(FALSE);
	}
	next->model = "gpio";
	next->serial = "next";
	if(mp_button_init(kernel, next) == FALSE)
		return(FALSE);

	/* the prev button is optional: a board without D0 keeps next-only */
	memset(prev, 0, sizeof(*prev));
	prev->pin = mp_gpio_text_handle("pa0", "Button");
	if(!prev->pin) {
		mp_printk("XIAO button: pa0 is not free (prev disabled)");
		return(TRUE);
	}
	prev->model = "gpio";
	prev->serial = "prev";
	if(mp_button_init(kernel, prev) == FALSE) {
		mp_printk("XIAO button: pa0 init failed (prev disabled)");
		return(TRUE);
	}
	return(TRUE);
}

#endif
