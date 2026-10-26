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
 * Charger contract. The driver owns an mp_device_charge_t and registers it as
 * the instance. A subscriber starts the driver. The driver offers charging,
 * done or waiting; the device publishes one once that reading comes back.
 */

static const char *_mp_device_charge_text[] = {
	"charging",
	"done",
	"waiting",
};

mp_device_charge_t *mp_device_charge(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_charge_t *)mp_device_subscribe(device, sub));
}

void mp_device_charge_publish(mp_device_charge_t *charge, mp_device_charge_state_t state) {
	if(!charge)
		return;
	MP_INTERRUPT_SAFE_BEGIN
	charge->state = state;
	charge->valid = YES;
	charge->stamp = mp_clock_ticks();
	charge->pending = state;
	charge->seen = 0;
	MP_INTERRUPT_SAFE_END
}

void mp_device_charge_sample(mp_device_charge_t *charge, mp_device_charge_state_t state) {
	if(!charge)
		return;
	if(state != MP_DEVICE_CHARGE_CHARGING && state != MP_DEVICE_CHARGE_DONE && state != MP_DEVICE_CHARGE_WAITING)
		return;
	MP_INTERRUPT_SAFE_BEGIN
	if(charge->valid == YES && state == charge->state) {
		charge->pending = state;
		charge->seen = 0;
	}
	else if(state == charge->pending && charge->seen) {
		charge->state = state;
		charge->valid = YES;
		charge->stamp = mp_clock_ticks();
		charge->seen = 0;
	}
	else {
		charge->pending = state;
		charge->seen = 1;
	}
	MP_INTERRUPT_SAFE_END
}

mp_ret_t mp_device_charge_read(mp_device_charge_t *charge, mp_device_charge_state_t *state) {
	mp_device_charge_t copy;

	if(!charge || !state)
		return(FALSE);
	MP_INTERRUPT_SAFE_BEGIN
	copy = *charge;
	MP_INTERRUPT_SAFE_END
	if(copy.valid != YES)
		return(FALSE);
	*state = copy.state;
	return(TRUE);
}

const char *mp_device_charge_text(mp_device_charge_state_t state) {
	if((unsigned int)state >= sizeof(_mp_device_charge_text) / sizeof(_mp_device_charge_text[0]))
		return("");
	return(_mp_device_charge_text[state]);
}

#endif
