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
 * GNSS contract. The driver owns an mp_device_gnss_t and registers it as the
 * instance. A subscriber starts the driver; the driver publishes each fix
 * into the struct.
 */

mp_device_gnss_t *mp_device_gnss(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_gnss_t *)mp_device_subscribe(device, sub));
}

void mp_device_gnss_publish(mp_device_gnss_t *gnss, const mp_device_gnss_t *fix) {
	void *user;

	if(!gnss || !fix)
		return;
	user = gnss->user;
	MP_INTERRUPT_SAFE_BEGIN
	*gnss = *fix;
	gnss->user = user;
	gnss->fixed = fix->quality != 0 ? YES : NO;
	gnss->valid = YES;
	gnss->stamp = mp_clock_ticks();
	MP_INTERRUPT_SAFE_END
}

mp_ret_t mp_device_gnss_read(mp_device_gnss_t *gnss, mp_device_gnss_t *out) {
	mp_device_gnss_t copy;

	if(!gnss || !out)
		return(FALSE);
	MP_INTERRUPT_SAFE_BEGIN
	copy = *gnss;
	MP_INTERRUPT_SAFE_END
	if(copy.valid != YES)
		return(FALSE);
	*out = copy;
	return(TRUE);
}

unsigned char mp_device_gnss_system(const char *name) {
	char a, b;

	if(!name || !name[0] || !name[1])
		return(MP_DEVICE_GNSS_SYSTEM_NONE);
	a = name[0];
	b = name[1];
	if(a >= 'a' && a <= 'z')
		a = (char)(a - 'a' + 'A');
	if(b >= 'a' && b <= 'z')
		b = (char)(b - 'a' + 'A');
	if(a == 'G' && b == 'P')
		return(MP_DEVICE_GNSS_SYSTEM_GPS);
	if(a == 'G' && b == 'L')
		return(MP_DEVICE_GNSS_SYSTEM_GLONASS);
	if(a == 'G' && b == 'A')
		return(MP_DEVICE_GNSS_SYSTEM_GALILEO);
	if((a == 'G' && b == 'B') || (a == 'B' && b == 'D'))
		return(MP_DEVICE_GNSS_SYSTEM_BEIDOU);
	if(a == 'G' && b == 'Q')
		return(MP_DEVICE_GNSS_SYSTEM_QZSS);
	if(a == 'G' && b == 'I')
		return(MP_DEVICE_GNSS_SYSTEM_NAVIC);
	if(a == 'G' && b == 'N')
		return(MP_DEVICE_GNSS_SYSTEM_MIXED);
	return(MP_DEVICE_GNSS_SYSTEM_NONE);
}

const char *mp_device_gnss_system_text(unsigned char system) {
	switch(system) {
		case MP_DEVICE_GNSS_SYSTEM_GPS: return("GPS");
		case MP_DEVICE_GNSS_SYSTEM_GLONASS: return("GLONASS");
		case MP_DEVICE_GNSS_SYSTEM_GALILEO: return("Galileo");
		case MP_DEVICE_GNSS_SYSTEM_BEIDOU: return("BeiDou");
		case MP_DEVICE_GNSS_SYSTEM_QZSS: return("QZSS");
		case MP_DEVICE_GNSS_SYSTEM_NAVIC: return("NavIC");
		case MP_DEVICE_GNSS_SYSTEM_MIXED: return("GNSS");
	}
	return("");
}

#endif
