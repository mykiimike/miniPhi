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
 * LED contract. The driver fills an mp_device_led_t and registers it as the instance.
 * An app subscribes with mp_device_led and drives the LED through mp_device_led_set.
 */

mp_device_led_t *mp_device_led(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_led_t *)mp_device_subscribe(device, sub));
}

void mp_device_led_set(mp_device_led_t *led, int on) {
	if(led && led->set)
		led->set(led, on);
}

void mp_device_led_turn(mp_device_led_t *led) {
	if(led && led->turn)
		led->turn(led);
}

int mp_device_led_state(mp_device_led_t *led) {
	return(led ? led->state : 0);
}

#endif
