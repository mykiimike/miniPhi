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
 * PHY contract. The driver fills an mp_device_phy_t and registers it as the instance.
 * opened and held live here, so two subscribers share one medium and only one holds it.
 */

mp_device_phy_t *mp_device_phy(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_phy_t *)mp_device_subscribe(device, sub));
}

mp_ret_t mp_device_phy_open(mp_device_phy_t *phy) {
	if(!phy || !phy->open)
		return(FALSE);
	if(phy->opened == YES)
		return(TRUE);
	if(phy->open(phy) != TRUE)
		return(FALSE);
	phy->opened = YES;
	return(TRUE);
}

void mp_device_phy_close(mp_device_phy_t *phy) {
	if(!phy || phy->opened != YES)
		return;
	if(phy->held == YES) {
		if(phy->release)
			phy->release(phy);
		phy->held = NO;
	}
	if(phy->close)
		phy->close(phy);
	phy->hz = 0;
	phy->width = 0;
	phy->opened = NO;
}

mp_bool_t mp_device_phy_ready(mp_device_phy_t *phy) {
	if(!phy || !phy->ready)
		return(NO);
	return(phy->ready(phy));
}

mp_ret_t mp_device_phy_hold(mp_device_phy_t *phy) {
	if(!phy || phy->opened != YES || !phy->hold)
		return(FALSE);
	if(phy->held == YES)
		return(FALSE);
	if(phy->hold(phy) != TRUE)
		return(FALSE);
	phy->held = YES;
	return(TRUE);
}

void mp_device_phy_release(mp_device_phy_t *phy) {
	if(!phy || phy->held != YES)
		return;
	if(phy->release)
		phy->release(phy);
	phy->held = NO;
}

mp_ret_t mp_device_phy_tune(mp_device_phy_t *phy, unsigned long hz, unsigned long width) {
	if(!phy || phy->held != YES || !phy->tune)
		return(FALSE);
	if(hz < phy->hz_min || hz > phy->hz_max)
		return(FALSE);
	if(width < phy->width_min || width > phy->width_max)
		return(FALSE);
	if(phy->tune(phy, hz, width) != TRUE)
		return(FALSE);
	phy->hz = hz;
	phy->width = width;
	return(TRUE);
}

unsigned long mp_device_phy_hz(mp_device_phy_t *phy) {
	return(phy ? phy->hz : 0);
}

unsigned long mp_device_phy_width(mp_device_phy_t *phy) {
	return(phy ? phy->width : 0);
}

mp_ret_t mp_device_phy_link(mp_device_phy_t *phy, unsigned long bps, int duplex) {
	if(!phy || !phy->link)
		return(FALSE);
	return(phy->link(phy, bps, duplex));
}

#endif
