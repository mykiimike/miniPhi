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

void mp_sensor_init(mp_kernel_t *kernel) {
	mp_sensor_handler_t *sensorHdl;
	sensorHdl = &kernel->sensors;

	sensorHdl->lastId = 0;
	mp_list_init(&sensorHdl->list);
}



void mp_sensor_flush(mp_kernel_t *kernel) {
	mp_sensor_handler_t *sensorHdl;
	mp_sensor_t *cur;
	mp_sensor_t *next;

	sensorHdl = &kernel->sensors;

	if(sensorHdl->list.first) {
		/* free all sensors */
		cur = sensorHdl->list.first->user;
		while(cur) {
			next = cur->item.next != NULL ? cur->item.next->user : NULL;
			mp_list_remove(&sensorHdl->list, &cur->item);
			mp_mem_free(kernel, cur);
			cur = next;
		}
	}

}

mp_sensor_t *mp_sensor_register(mp_kernel_t *kernel, mp_sensor_type_t type, char *name) {
	mp_sensor_handler_t *sensorHdl;
	int size = strlen(name);
	mp_sensor_t *sensor;

	sensorHdl = &kernel->sensors;

	if(size >= MP_COMMON_SENSOR_NAME_SIZE) {
		mp_printk("Common sensor register: name is too long %d", size);
		return(NULL);
	}

	/* allocate sensor */
	sensor = mp_mem_alloc(kernel, sizeof(sensor));
	if(!sensor) {
		mp_printk("Common sensor register: can not allocate sensor");
		return(NULL);
	}

	memset(sensor, 0, sizeof(*sensor));

	sensor->type = type;
	memcpy(sensor->idName, name, size);

	/* add */
	mp_list_add_last(&sensorHdl->list, &sensor->item, sensor);

	mp_printk("Registering sensor type #%d : %s", type, name);

	return(sensor);

}

mp_ret_t mp_sensor_unregister(mp_kernel_t *kernel, mp_sensor_t *sensor) {
	mp_sensor_handler_t *sensorHdl;

	mp_printk("Unregistering sensor type #%d : %s", sensor->type, sensor->idName);

	sensorHdl = &kernel->sensors;

	mp_list_remove(&sensorHdl->list, &sensor->item);

	return(TRUE);
}


mp_sensor_t *mp_sensor_search(mp_kernel_t *kernel, char *key) {
	return(NULL);
}

#endif
