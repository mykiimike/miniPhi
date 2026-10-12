/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2015  Michael VERGOZ                                      *
 * Copyright (C) 2015  VERMAN                                              *
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

#ifdef SUPPORT_COMMON_SENSOR
#ifndef _HAVE_MP_COMMON_SENSOR_H
	#define _HAVE_MP_COMMON_SENSOR_H

	#define MP_COMMON_SENSOR_NAME_SIZE 19

	typedef struct mp_sensor_handler_s mp_sensor_handler_t;
	typedef struct mp_sensor_s mp_sensor_t;

	typedef enum {
		MP_SENSOR_TEMPERATURE,
		MP_SENSOR_BAROMETER,
		MP_SENSOR_ALTIMETER,
		MP_SENSOR_3AXIS,
		MP_SENSOR_VOLTAGE,
		MP_SENSOR_CURRENT,
	} mp_sensor_type_t;

	typedef struct {
		float result;
	} mp_sensor_temperature_t;

	typedef struct {
		float result;
	} mp_sensor_voltage_t;

	typedef struct {
		float result;
	} mp_sensor_current_t;

	typedef struct {
		float x;
		float y;
		float z;
	} mp_sensor_3axis_t;

	typedef struct {
		float result;
	} mp_sensor_barometer_t;

	typedef struct {
#define MP_SENSOR_ALTIMETER_FEET  0
#define MP_SENSOR_ALTIMETER_METER 1
#define MP_SENSOR_ALTIMETER_FMC   0.30480

		/** convert to feet or meter */
		char conversion;

		/** altimeter result */
		float result;
	} mp_sensor_altimeter_t;

	struct mp_sensor_s {
		unsigned short id;

		char idName[MP_COMMON_SENSOR_NAME_SIZE];

		mp_sensor_type_t type;

		union {
			mp_sensor_temperature_t temperature;
			mp_sensor_barometer_t barometer;
			mp_sensor_altimeter_t altimeter;
			mp_sensor_voltage_t voltage;
			mp_sensor_current_t current;
			mp_sensor_3axis_t axis3;
		};

		mp_list_item_t item;
	};



	struct mp_sensor_handler_s {
		int lastId;
		mp_list_t list;
	};

	void mp_sensor_init(mp_kernel_t *kernel);
	void mp_sensor_flush(mp_kernel_t *kernel);
	mp_sensor_t *mp_sensor_register(mp_kernel_t *kernel, mp_sensor_type_t type, char *name);
	mp_ret_t mp_sensor_unregister(mp_kernel_t *kernel, mp_sensor_t *sensor);
	mp_sensor_t *mp_sensor_search(mp_kernel_t *kernel, char *key);

	#define mp_sensor_fini mp_sensor_flush

#endif
#endif

