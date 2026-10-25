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

#include <mp.h>

#ifdef SUPPORT_COMMON_SENSOR

/**
@defgroup mpCommonSensor Sensors

@ingroup mpCommon

@brief Registry of the measures the board makes

A driver registers one sensor per measure, with a type that fixes its
unit (see mp_sensor_type_t), and publishes into it with
mp_sensor_publish() or mp_sensor_publish3(), from a task or an interrupt.
A reader walks them with mp_sensor_next() or mp_sensor_find() and takes
a coherent copy with mp_sensor_read(), which also says whether anything
was published and how long ago.

Each sensor takes one @ref mpCommonMem chunk, given back by
mp_sensor_unregister(), which also clears the owner's pointer.
Registration and lookups run from tasks; publishing is interrupt safe
and ignores a NULL sensor, so a driver may publish into a sensor whose
registration failed.

<strong>@ref SUPPORT_COMMON_SENSOR must be defined to use this module</strong>
@{
*/

static const char *_mp_sensor_units[MP_SENSOR_TYPES] = {
	"C", "Pa", "m", "g", "dps", "gauss", "V", "A", "W", "%RH", "lx", "deg"
};

void mp_sensor_init(mp_kernel_t *kernel) {
	mp_list_init(&kernel->sensors.list);
}

/**
 * @brief Give back every sensor still registered
 *
 * Their owners keep dangling pointers: unregister them first.
 */
void mp_sensor_fini(mp_kernel_t *kernel) {
	mp_sensor_t *sensor;

	while(kernel->sensors.list.first) {
		sensor = kernel->sensors.list.first->user;
		mp_printk("Sensor %s still registered at fini", sensor->name);
		mp_sensor_unregister(kernel, &sensor);
	}
}

/**
 * @brief Add a sensor
 *
 * @param[in] kernel Kernel
 * @param[in] type What it measures
 * @param[in] name Label, kept as given: a literal or a string that outlives the sensor
 * @return The sensor, NULL on a bad type or name or when the pool is empty
 */
mp_sensor_t *mp_sensor_register(mp_kernel_t *kernel, mp_sensor_type_t type, const char *name) {
	mp_sensor_t *sensor;

	if(!kernel || !name || (unsigned int)type >= MP_SENSOR_TYPES)
		return(NULL);

	sensor = mp_mem_alloc(kernel, sizeof(*sensor));
	if(!sensor) {
		mp_printk("Sensor %s: can not allocate", name);
		return(NULL);
	}

	sensor->name = name;
	sensor->type = type;
	sensor->valid = NO;
	mp_list_add_last(&kernel->sensors.list, &sensor->item, sensor);

	return(sensor);
}

/**
 * @brief Remove a sensor and give its chunk back
 *
 * @param[in] kernel Kernel
 * @param[in,out] sensor The owner's pointer, set to NULL; NULL or a pointer to NULL does nothing
 * @return FALSE when there was no sensor, or it is not registered
 */
mp_ret_t mp_sensor_unregister(mp_kernel_t *kernel, mp_sensor_t **sensor) {
	mp_list_item_t *item;

	if(!kernel || !sensor || !*sensor)
		return(FALSE);

	for(item = kernel->sensors.list.first; item; item = item->next) {
		if(item->user == *sensor)
			break;
	}
	if(!item) {
		mp_printk("Sensor: unregistering one that is not registered");
		*sensor = NULL;
		return(FALSE);
	}

	mp_list_remove(&kernel->sensors.list, &(*sensor)->item);
	mp_mem_free(kernel, *sensor);
	*sensor = NULL;
	return(TRUE);
}

/**
 * @brief Publish a single value; interrupt safe, NULL ignored
 */
void mp_sensor_publish(mp_sensor_t *sensor, float value) {
	unsigned long now;

	if(!sensor)
		return;
	now = mp_clock_ticks();
	MP_INTERRUPT_SAFE_BEGIN
	sensor->value[0] = value;
	sensor->stamp = now;
	sensor->valid = YES;
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Publish x, y and z together; interrupt safe, NULL ignored
 */
void mp_sensor_publish3(mp_sensor_t *sensor, float x, float y, float z) {
	unsigned long now;

	if(!sensor)
		return;
	now = mp_clock_ticks();
	MP_INTERRUPT_SAFE_BEGIN
	sensor->value[0] = x;
	sensor->value[1] = y;
	sensor->value[2] = z;
	sensor->stamp = now;
	sensor->valid = YES;
	MP_INTERRUPT_SAFE_END
}

/**
 * @brief Copy the last publication
 *
 * @param[in] sensor Sensor
 * @param[out] value mp_sensor_axes() values
 * @param[out] age Clock ticks since it was published; NULL when not wanted
 * @return FALSE when nothing was published yet
 */
mp_ret_t mp_sensor_read(mp_sensor_t *sensor, float value[3], unsigned long *age) {
	unsigned long stamp;
	mp_bool_t valid;
	int i, axes;

	if(!sensor || !value)
		return(FALSE);

	axes = mp_sensor_axes(sensor->type);
	MP_INTERRUPT_SAFE_BEGIN
	for(i = 0; i < axes; i++)
		value[i] = sensor->value[i];
	stamp = sensor->stamp;
	valid = sensor->valid;
	MP_INTERRUPT_SAFE_END

	if(valid != YES)
		return(FALSE);
	if(age)
		*age = mp_clock_ticks() - stamp;
	return(TRUE);
}

/**
 * @brief Walk the sensors
 *
 * @param[in] after The previous one, NULL for the first
 * @return The next sensor, NULL after the last
 */
mp_sensor_t *mp_sensor_next(mp_kernel_t *kernel, mp_sensor_t *after) {
	mp_list_item_t *item;

	if(!kernel)
		return(NULL);
	item = after ? after->item.next : kernel->sensors.list.first;
	return(item ? item->user : NULL);
}

/**
 * @brief First sensor of a type, and of a name
 *
 * @param[in] type The type, or MP_SENSOR_ANY
 * @param[in] name The exact name, or NULL for any
 */
mp_sensor_t *mp_sensor_find(mp_kernel_t *kernel, mp_sensor_type_t type, const char *name) {
	mp_sensor_t *sensor = NULL;

	while((sensor = mp_sensor_next(kernel, sensor)) != NULL) {
		if(type != MP_SENSOR_ANY && sensor->type != type)
			continue;
		if(name && strcmp(sensor->name, name) != 0)
			continue;
		return(sensor);
	}
	return(NULL);
}

/**
 * @return 3 for the accelerometer, the gyroscope, the magnetometer and the orientation, 1 otherwise
 */
int mp_sensor_axes(mp_sensor_type_t type) {
	return(type == MP_SENSOR_ACCELEROMETER || type == MP_SENSOR_GYROSCOPE || type == MP_SENSOR_MAGNETOMETER || type == MP_SENSOR_ORIENTATION ? 3 : 1);
}

/**
 * @return The unit a type publishes in, "" for an unknown type
 */
const char *mp_sensor_unit(mp_sensor_type_t type) {
	return((unsigned int)type < MP_SENSOR_TYPES ? _mp_sensor_units[type] : "");
}

/**@}*/

#endif
