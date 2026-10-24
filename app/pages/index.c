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

/*
 * The page list. Home order is the order of the _open calls before System.
 */

#include "index.h"

#ifdef SYSINFO_SCREEN

int _sysinfo_pages_open(sysinfo_t *sysinfo) {
	_sysinfo_accel_open(sysinfo);
	_sysinfo_camera_open(sysinfo);
	_sysinfo_crypto_open(sysinfo);
	_sysinfo_power_open(sysinfo);
	if(_sysinfo_system_open(sysinfo) == 0)
		return(0);
	_sysinfo_tasks_open(sysinfo);
	_sysinfo_devices_open(sysinfo);
#ifdef SUPPORT_COMMON_USB
	_sysinfo_usb_open(sysinfo);
#endif
	return(1);
}

void _sysinfo_pages_refresh(sysinfo_t *sysinfo) {
	_sysinfo_accel_refresh(sysinfo);
	_sysinfo_camera_refresh(sysinfo);
	_sysinfo_crypto_refresh(sysinfo);
	_sysinfo_power_refresh(sysinfo);
	_sysinfo_tasks_refresh(sysinfo);
	_sysinfo_devices_refresh(sysinfo);
#ifdef SUPPORT_COMMON_USB
	_sysinfo_usb_refresh(sysinfo);
#endif
	_sysinfo_system_refresh(sysinfo);
}

#endif
