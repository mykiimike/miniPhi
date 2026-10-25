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

#ifdef SUPPORT_COMMON_DEVICE

/**
 * Button contract. The instance is an mp_device_button_t.
 * active tells the polarity: 0 active low (pressed reads 0), 1 active high (WKUP PA0).
 * common/button reports a raw pin level with mp_device_button_level. A producer that
 * already decoded the edge calls mp_device_button_edge. The contract keeps pressed and
 * fires on_change on each edge. Press patterns come later, on top of this.
 */

mp_device_button_t *mp_device_button(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_button_t *)mp_device_subscribe(device, sub));
}

void mp_device_button_level(mp_device_button_t *button, int level) {
	int down;

	if(!button)
		return;
	down = button->active ? (level ? 1 : 0) : (level ? 0 : 1);
	mp_device_button_edge(button, down);
}

void mp_device_button_edge(mp_device_button_t *button, int down) {
	if(!button)
		return;
	down = down ? 1 : 0;
	if(button->pressed == down)
		return;
	button->pressed = down;
	if(button->on_change)
		button->on_change(button, down, button->user);
}

int mp_device_button_pressed(mp_device_button_t *button) {
	return(button ? button->pressed : 0);
}

#endif
