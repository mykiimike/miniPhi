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

/**
@defgroup mpArchESP32C6Phy PHY

@ingroup mpArchESP32

One MP_DEVICE_PHY for the on-chip radio. Registering it does not power
the modem. open does. tune accepts a 20 MHz channel only: the 2 MHz
filter path is not written. hold is the contract's lock on this face.

@{
*/

#if defined(SUPPORT_ESP32_RF) && defined(SUPPORT_COMMON_DEVICE)

#include "rf_priv.h"

/* Wi-Fi channels 1 through 14. Width is the 20 MHz channel this silicon programs. */
#define _HZ_MIN  2412000000UL
#define _HZ_MAX  2484000000UL
#define _WIDTH   20000000UL

static struct {
	mp_kernel_t *kernel;
	mp_device_driver_t binding;
	mp_device_registry_t device;
	mp_device_phy_t face;
	mp_bool_t live;
} _phy;

static mp_ret_t _open(mp_device_phy_t *phy) {
	(void)phy;
	return(mp_arch_rf_open(_phy.kernel));
}

static void _close(mp_device_phy_t *phy) {
	(void)phy;
	mp_arch_rf_close();
}

static mp_bool_t _ready(mp_device_phy_t *phy) {
	(void)phy;
	return(mp_arch_rf_ready() == TRUE ? YES : NO);
}

/** The contract owns the lock. This hook has nothing to program yet. */
static mp_ret_t _hold(mp_device_phy_t *phy) {
	(void)phy;
	return(TRUE);
}

static void _release(mp_device_phy_t *phy) {
	(void)phy;
}

/**
 * Program one 20 MHz carrier. Any other width is refused.
 * hz is a whole megahertz so phy_chan_set can take it as a frequency.
 */
static mp_ret_t _tune(mp_device_phy_t *phy, unsigned long hz, unsigned long width) {
	(void)phy;
	if(width != _WIDTH)
		return(FALSE);
	if((hz % 1000000UL) != 0)
		return(FALSE);
	phy_chan_set((uint32_t)(hz / 1000000UL), 0);
	return(TRUE);
}

static const mp_device_driver_ops_t _ops = {
	"arch/c6/rf", "ESP32-C6 radio", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_PHY),
	NULL, NULL, NULL, NULL, NULL
};

void mp_c6_phy(mp_kernel_t *kernel) {
	const char *serial;

	memset(&_phy, 0, sizeof(_phy));
	_phy.kernel = kernel;
	if(mp_device_driver_register(kernel, &_phy.binding, &_ops, &_phy, NULL) == FALSE)
		return;
	_phy.face.kind = MP_DEVICE_PHY_RADIO;
	_phy.face.open = _open;
	_phy.face.close = _close;
	_phy.face.ready = _ready;
	_phy.face.hold = _hold;
	_phy.face.release = _release;
	_phy.face.tune = _tune;
	_phy.face.hz_min = _HZ_MIN;
	_phy.face.hz_max = _HZ_MAX;
	_phy.face.width_min = _WIDTH;
	_phy.face.width_max = _WIDTH;
	_phy.face.user = &_phy;
	serial = mp_machine_serial();
	if(!serial || !serial[0])
		serial = "c6";
	if(!mp_device_registry_register(&_phy.binding, &_phy.device, MP_DEVICE_PHY, 0, &_phy.face, "esp32c6", serial, NULL)) {
		mp_device_driver_unregister(kernel, &_phy.binding);
		return;
	}
	_phy.device.label = "radio";
	_phy.live = YES;
}

mp_device_phy_t *mp_c6_phy_face(void) {
	if(_phy.live != YES)
		return(NULL);
	return(&_phy.face);
}

/**
 * One Wi-Fi carrier. level 8 is a low entry of the 0..31 gain table.
 * A zero argument stops it.
 */
void mp_c6_rf_tone(int on) {
	if(on)
		phy_tx_tone(1, 0, 8);
	else
		phy_tx_tone(0, 0, 0);
}

int mp_c6_rf_cca(void) {
	return(phy_get_cca());
}

int mp_c6_rf_rssi(void) {
	return(phy_get_rssi());
}

void mp_c6_phy_fini(mp_kernel_t *kernel) {
	_phy.live = NO;
	if(_phy.face.opened == YES)
		mp_device_phy_close(&_phy.face);
	mp_device_registry_unregister(&_phy.device);
	mp_device_driver_unregister(kernel, &_phy.binding);
}

#else

void mp_c6_phy(mp_kernel_t *kernel) {
	(void)kernel;
}

void mp_c6_phy_fini(mp_kernel_t *kernel) {
	(void)kernel;
}

#endif

/**@}*/
