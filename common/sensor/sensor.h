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

	typedef struct mp_sensor_handler_s mp_sensor_handler_t;
	typedef struct mp_sensor_s mp_sensor_t;

	/** what a sensor measures, and the unit it publishes in */
	typedef enum {
		MP_SENSOR_TEMPERATURE,		/**< degree Celsius */
		MP_SENSOR_BAROMETER,		/**< pascal */
		MP_SENSOR_ALTIMETER,		/**< metre */
		MP_SENSOR_ACCELEROMETER,	/**< g, x y z */
		MP_SENSOR_GYROSCOPE,		/**< degree per second, x y z */
		MP_SENSOR_MAGNETOMETER,		/**< gauss, x y z */
		MP_SENSOR_VOLTAGE,			/**< volt */
		MP_SENSOR_CURRENT,			/**< ampere */
		MP_SENSOR_TYPES
	} mp_sensor_type_t;

	/** a mp_sensor_find() type that matches every sensor */
	#define MP_SENSOR_ANY MP_SENSOR_TYPES

	struct mp_sensor_s {
		/** kept, not copied: it must outlive the sensor */
		const char *name;

		mp_sensor_type_t type;

		/** YES once a value was published */
		mp_bool_t valid;

		/** clock ticks of the last publication */
		unsigned long stamp;

		/** value[0], or x y z; read them with mp_sensor_read() */
		float value[3];

		mp_list_item_t item;
	};

	struct mp_sensor_handler_s {
		mp_list_t list;
	};

	void mp_sensor_init(mp_kernel_t *kernel);
	void mp_sensor_fini(mp_kernel_t *kernel);

	mp_sensor_t *mp_sensor_register(mp_kernel_t *kernel, mp_sensor_type_t type, const char *name);
	mp_ret_t mp_sensor_unregister(mp_kernel_t *kernel, mp_sensor_t **sensor);

	void mp_sensor_publish(mp_sensor_t *sensor, float value);
	void mp_sensor_publish3(mp_sensor_t *sensor, float x, float y, float z);
	mp_ret_t mp_sensor_read(mp_sensor_t *sensor, float value[3], unsigned long *age);

	mp_sensor_t *mp_sensor_next(mp_kernel_t *kernel, mp_sensor_t *after);
	mp_sensor_t *mp_sensor_find(mp_kernel_t *kernel, mp_sensor_type_t type, const char *name);

	int mp_sensor_axes(mp_sensor_type_t type);
	const char *mp_sensor_unit(mp_sensor_type_t type);

#endif
#endif
