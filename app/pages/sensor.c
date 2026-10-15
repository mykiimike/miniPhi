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

#if defined(SYSINFO_SCREEN) && defined(SUPPORT_COMMON_SENSOR)

#include <string.h>

int _sysinfo_sensor_beside(mp_sensor_t *axis, mp_sensor_t *sensor) {
	int length = (int)strlen(axis->name);

	if(sensor->type != MP_SENSOR_TEMPERATURE || length <= 0)
		return(0);
	if(strncmp(sensor->name, axis->name, (size_t)length) != 0)
		return(0);
	return(sensor->name[length] == '\0' || sensor->name[length] == ' ');
}

#endif
