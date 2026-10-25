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
 * One directory per home page, and its sub-pages inside it. index.c is the list
 * screen.c calls; a new page is an index.c in its directory, two lines in
 * index.c and its source in _phi.js. sensor.c is shared by the pages.
 */

#ifndef _MINIPHI_APP_SYSINFO_PAGES_H
#define _MINIPHI_APP_SYSINFO_PAGES_H

#include "../sysinfo.h"

#ifdef SYSINFO_SCREEN

#include <stdio.h>

/** a temperature published by the same driver as a 3-axis measure */
#ifdef SUPPORT_COMMON_DEVICE
int _sysinfo_measure_beside(mp_device_registry_t *axis, mp_device_registry_t *other);
#endif

void _sysinfo_accel_open(sysinfo_t *sysinfo);
void _sysinfo_accel_refresh(sysinfo_t *sysinfo);
void _sysinfo_camera_open(sysinfo_t *sysinfo);
void _sysinfo_camera_refresh(sysinfo_t *sysinfo);
/** 0 when the System page could not be registered */
int _sysinfo_system_open(sysinfo_t *sysinfo);
void _sysinfo_system_refresh(sysinfo_t *sysinfo);
void _sysinfo_tasks_open(sysinfo_t *sysinfo);
void _sysinfo_tasks_refresh(sysinfo_t *sysinfo);
void _sysinfo_devices_open(sysinfo_t *sysinfo);
void _sysinfo_devices_refresh(sysinfo_t *sysinfo);
#ifdef SUPPORT_COMMON_USB
void _sysinfo_usb_open(sysinfo_t *sysinfo);
void _sysinfo_usb_refresh(sysinfo_t *sysinfo);
/** wires a USB link change to wake the screen, like a touch */
void _sysinfo_usb_watch(sysinfo_t *sysinfo);
#endif
void _sysinfo_crypto_open(sysinfo_t *sysinfo);
void _sysinfo_crypto_refresh(sysinfo_t *sysinfo);
void _sysinfo_power_open(sysinfo_t *sysinfo);
void _sysinfo_power_refresh(sysinfo_t *sysinfo);

/** home entries first, then the sub-pages of System; 0 when System is missing */
int _sysinfo_pages_open(sysinfo_t *sysinfo);
/** the live figures of whichever page is on screen */
void _sysinfo_pages_refresh(sysinfo_t *sysinfo);

#endif

#endif
