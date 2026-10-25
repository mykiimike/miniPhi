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
 * Measure contract. The driver owns an mp_device_measure_t, registers it as the device
 * instance, and publishes into it. A reader takes a coherent copy.
 * This is the home of what common/sensor used to do, without its own registry:
 * the device address and class carry the name and the type.
 */

static const char *_mp_device_measure_unit[MP_DEVICE_CLASS_COUNT] = {
	"", "", "g", "", "", "C", "dps", "gauss", "V", "A", "", "",
	"Pa", "m", "W", "%RH", "lx", "deg", ""
};

typedef char _mp_device_measure_unit_ok[(sizeof(_mp_device_measure_unit) / sizeof(_mp_device_measure_unit[0]) == MP_DEVICE_CLASS_COUNT) ? 1 : -1];

mp_device_measure_t *mp_device_measure(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_measure_t *)mp_device_subscribe(device, sub));
}

void mp_device_measure_publish(mp_device_measure_t *measure, float value) {
	unsigned long now;

	if(!measure)
		return;
	now = mp_clock_ticks();
	MP_INTERRUPT_SAFE_BEGIN
	measure->value[0] = value;
	measure->value[1] = 0;
	measure->value[2] = 0;
	measure->stamp = now;
	measure->valid = YES;
	MP_INTERRUPT_SAFE_END
}

void mp_device_measure_publish3(mp_device_measure_t *measure, float x, float y, float z) {
	unsigned long now;

	if(!measure)
		return;
	now = mp_clock_ticks();
	MP_INTERRUPT_SAFE_BEGIN
	measure->value[0] = x;
	measure->value[1] = y;
	measure->value[2] = z;
	measure->stamp = now;
	measure->valid = YES;
	MP_INTERRUPT_SAFE_END
}

mp_ret_t mp_device_measure_read(mp_device_measure_t *measure, float value[3], unsigned long *age) {
	unsigned long stamp;
	mp_bool_t valid;

	if(!measure || !value)
		return(FALSE);
	MP_INTERRUPT_SAFE_BEGIN
	value[0] = measure->value[0];
	value[1] = measure->value[1];
	value[2] = measure->value[2];
	stamp = measure->stamp;
	valid = measure->valid;
	MP_INTERRUPT_SAFE_END
	if(valid != YES)
		return(FALSE);
	if(age)
		*age = mp_clock_ticks() - stamp;
	return(TRUE);
}

int mp_device_measure_axes(mp_device_class_t class) {
	if(class == MP_DEVICE_ACCELEROMETER || class == MP_DEVICE_GYROSCOPE ||
		class == MP_DEVICE_MAGNETOMETER || class == MP_DEVICE_ORIENTATION)
		return(3);
	return(1);
}

const char *mp_device_measure_unit(mp_device_class_t class) {
	if((unsigned int)class >= MP_DEVICE_CLASS_COUNT)
		return("");
	return(_mp_device_measure_unit[class]);
}

mp_bool_t mp_device_measure_class(mp_device_class_t class) {
	if(class == MP_DEVICE_ACCELEROMETER || class == MP_DEVICE_TEMPERATURE ||
		class == MP_DEVICE_GYROSCOPE || class == MP_DEVICE_MAGNETOMETER ||
		class == MP_DEVICE_VOLTAGE || class == MP_DEVICE_CURRENT ||
		class == MP_DEVICE_BAROMETER || class == MP_DEVICE_ALTIMETER ||
		class == MP_DEVICE_POWER || class == MP_DEVICE_HUMIDITY ||
		class == MP_DEVICE_ILLUMINANCE || class == MP_DEVICE_ORIENTATION)
		return(YES);
	return(NO);
}

mp_device_registry_t *mp_device_measure_next(mp_kernel_t *kernel, mp_device_registry_t *after) {
	mp_list_item_t *item;

	if(!kernel)
		return(NULL);
	item = after ? after->item.next : kernel->devices.first;
	for(; item; item = item->next) {
		mp_device_registry_t *device = item->user;

		if(mp_device_measure_class(device->class) == YES)
			return(device);
	}
	return(NULL);
}

#endif
