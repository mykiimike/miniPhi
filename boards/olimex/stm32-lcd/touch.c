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

#ifdef SUPPORT_COMMON_RESISTIVE

/**
 * Olimex STM32-LCD glass. The 4-wire sheet sits on the ILI9320: 240 columns
 * by 320 rows, YD YU XL XR on PC0 to PC3, landscape with the rows mirrored.
 */

static mp_resistive_t _mp_board_touch;

static mp_gpio_port_t *_mp_board_touch_pin(char *text) {
	return(mp_gpio_text_handle(text, "Touch"));
}

mp_ret_t mp_board_resistive(mp_kernel_t *kernel) {
	memset(&_mp_board_touch, 0, sizeof(_mp_board_touch));
	_mp_board_touch.yd = _mp_board_touch_pin("pc0");
	_mp_board_touch.yu = _mp_board_touch_pin("pc1");
	_mp_board_touch.xl = _mp_board_touch_pin("pc2");
	_mp_board_touch.xr = _mp_board_touch_pin("pc3");
	_mp_board_touch.orientation = MP_RESISTIVE_LANDSCAPE;
	_mp_board_touch.columns = 240;
	_mp_board_touch.rows = 320;
	_mp_board_touch.flipY = 1;
	_mp_board_touch.xMin = 250;
	_mp_board_touch.xMax = 3850;
	_mp_board_touch.yMin = 250;
	_mp_board_touch.yMax = 3850;
	_mp_board_touch.model = "resistive";
	_mp_board_touch.serial = "stm32-lcd";
	_mp_board_touch.period = 20;
	_mp_board_touch.idle = 100;
	_mp_board_touch.doze = 50;

	if(!_mp_board_touch.yd || !_mp_board_touch.yu || !_mp_board_touch.xl || !_mp_board_touch.xr) {
		mp_printk("STM32-LCD touch: PC0 to PC3 are not free");
		if(_mp_board_touch.yd)
			mp_gpio_release(_mp_board_touch.yd);
		if(_mp_board_touch.yu)
			mp_gpio_release(_mp_board_touch.yu);
		if(_mp_board_touch.xl)
			mp_gpio_release(_mp_board_touch.xl);
		if(_mp_board_touch.xr)
			mp_gpio_release(_mp_board_touch.xr);
		return(FALSE);
	}
	return(mp_resistive_init(kernel, &_mp_board_touch));
}

#endif
