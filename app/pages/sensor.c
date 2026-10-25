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

#include "index.h"

#if defined(SYSINFO_SCREEN) && defined(SUPPORT_COMMON_DEVICE)

int _sysinfo_measure_beside(mp_device_registry_t *axis, mp_device_registry_t *other) {
	if(!axis || !other || !axis->driver)
		return(0);
	return(other->driver == axis->driver && other->class == MP_DEVICE_TEMPERATURE);
}

/* integer digits only: printf %f mallocs, and a failed malloc resets the chip */
void _sysinfo_format_fixed(char *dst, int cap, float value, int decimals, mp_bool_t sign) {
	int scale = 1;
	int n, whole, frac;
	int neg;

	if(!dst || cap < 2)
		return;
	if(decimals < 0)
		decimals = 0;
	if(decimals > 3)
		decimals = 3;
	for(n = 0; n < decimals; n++)
		scale *= 10;
	neg = value < 0.0f;
	if(neg)
		value = -value;
	whole = (int)value;
	frac = (int)((value - (float)whole) * (float)scale + 0.5f);
	if(frac >= scale) {
		frac -= scale;
		whole++;
	}
	if(decimals == 0)
		snprintf(dst, (size_t)cap, "%s%d", neg ? "-" : sign == YES ? " " : "", whole);
	else if(decimals == 1)
		snprintf(dst, (size_t)cap, "%s%d.%d", neg ? "-" : sign == YES ? " " : "", whole, frac);
	else if(decimals == 2)
		snprintf(dst, (size_t)cap, "%s%d.%02d", neg ? "-" : sign == YES ? " " : "", whole, frac);
	else
		snprintf(dst, (size_t)cap, "%s%d.%03d", neg ? "-" : sign == YES ? " " : "", whole, frac);
}

#endif
