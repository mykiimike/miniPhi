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
 * Registered devices. The caller owns each mp_device_registry_t.
 * Register links it on the kernel and on the driver. A scan unlinks what was not registered again.
 */

static const char *_mp_device_registry_stem[MP_DEVICE_CLASS_COUNT] = {
	"disp", "touch", "accel", "led", "video",
	"temp", "gyro", "mag", "volt", "curr", "cpu", "button",
	"baro", "alt", "power", "hum", "lux", "orient", "clock"
};

typedef char _mp_device_registry_stem_ok[(sizeof(_mp_device_registry_stem) / sizeof(_mp_device_registry_stem[0]) == MP_DEVICE_CLASS_COUNT) ? 1 : -1];

static void _mp_device_registry_drop(mp_device_registry_t *device) {
	mp_device_driver_t *driver = device->driver;
	mp_kernel_t *kernel = driver->kernel;

	device->removed = YES;
	mp_device_subscribe_notify(device);
	mp_list_remove(&kernel->devices, &device->item);
	mp_list_remove(&driver->devices, &device->byDriver);
	memset(device, 0, sizeof(*device));
}

void mp_device_registry_init(mp_kernel_t *kernel) {
	if(!kernel)
		return;
	mp_list_init(&kernel->devices);
}

void mp_device_registry_fini(mp_kernel_t *kernel) {
	if(!kernel)
		return;
	while(kernel->devices.first)
		_mp_device_registry_drop(kernel->devices.first->user);
}

void mp_device_registry_unregister(mp_device_registry_t *device) {
	if(!device || !device->driver)
		return;
	_mp_device_registry_drop(device);
}

mp_device_registry_t *mp_device_registry_register(mp_device_driver_t *driver, mp_device_registry_t *device, mp_device_class_t class, int channel, void *instance, const char *model, const char *serial, const char *version) {
	char built[MP_DEVICE_ADDRESS];
	const char *ver = NULL;
	const unsigned char *p;
	mp_list_item_t *item;
	int index;

	if(!driver || !driver->kernel || !device || !instance)
		return(NULL);
	if(class < 0 || class >= MP_DEVICE_CLASS_COUNT)
		return(NULL);
	if(device->driver && device->driver != driver)
		return(NULL);
	if(device->driver == driver && (device->class != class || device->channel != channel))
		return(NULL);
	if(version && version[0]) {
		for(p = (const unsigned char *)version; *p; p++)
			if(*p < 0x20 || *p >= 0x80)
				return(NULL);
		ver = version;
	}
	if(device->driver != driver) {
		for(item = driver->devices.first; item; item = item->next) {
			mp_device_registry_t *other = item->user;

			if(other->class == class && other->channel == channel)
				return(NULL);
		}
	}
	index = mp_device_driver_index(driver, class, channel);
	if(index < 0)
		return(NULL);
	if(mp_device_address(built, (int)sizeof(built), _mp_device_registry_stem[class], index, model, serial) == FALSE)
		return(NULL);

	if(device->driver == driver) {
		if(strcmp(device->address, built) != 0) {
			mp_device_subscribe_notify(device);
			memcpy(device->address, built, sizeof(built));
		}
		device->instance = instance;
		device->version = ver;
		device->index = (short)index;
		device->stale = NO;
		/* label and commands belong to the driver; a refresh must not clear them */
		return(device);
	}

	memset(device, 0, sizeof(*device));
	device->driver = driver;
	device->class = class;
	device->channel = (unsigned char)channel;
	device->index = (short)index;
	device->instance = instance;
	device->version = ver;
	memcpy(device->address, built, sizeof(built));
	mp_list_init(&device->subs);
	mp_list_add_last(&driver->kernel->devices, &device->item, device);
	mp_list_add_last(&driver->devices, &device->byDriver, device);
	return(device);
}

void mp_device_registry_scan(mp_kernel_t *kernel, mp_device_class_t class) {
	mp_list_item_t *item;
	mp_list_item_t *next;

	if(!kernel)
		return;
	if(class != MP_DEVICE_ANY && (class < 0 || class >= MP_DEVICE_CLASS_COUNT))
		return;
	for(item = kernel->drivers.first; item; item = next) {
		mp_device_driver_t *driver = item->user;
		mp_list_item_t *held;

		next = item->next;

		if(class != MP_DEVICE_ANY && (driver->classes & (1u << class)) == 0)
			continue;
		if(!driver->ops || !driver->ops->scan)
			continue;
		for(held = driver->devices.first; held; held = held->next) {
			mp_device_registry_t *device = held->user;

			device->stale = YES;
		}
		driver->ops->scan(driver, driver->self);
		held = driver->devices.first;
		while(held) {
			mp_device_registry_t *device = held->user;
			mp_list_item_t *after = held->next;

			if(device->stale == YES) {
				_mp_device_registry_drop(device);
			}
			held = after;
		}
	}
}

int mp_device_registry_count(mp_kernel_t *kernel) {
	mp_list_item_t *item;
	int n = 0;

	if(!kernel)
		return(0);
	for(item = kernel->devices.first; item; item = item->next)
		n++;
	return(n);
}

mp_device_registry_t *mp_device_registry_at(mp_kernel_t *kernel, int index) {
	mp_list_item_t *item;

	if(!kernel || index < 0)
		return(NULL);
	for(item = kernel->devices.first; item; item = item->next) {
		if(index-- == 0)
			return(item->user);
	}
	return(NULL);
}

mp_device_registry_t *mp_device_registry_address(mp_kernel_t *kernel, const char *address) {
	char built[MP_DEVICE_ADDRESS];
	mp_list_item_t *item;

	if(!kernel || !address)
		return(NULL);
	if(mp_device_address_canon(built, (int)sizeof(built), address) == FALSE)
		return(NULL);
	for(item = kernel->devices.first; item; item = item->next) {
		mp_device_registry_t *device = item->user;

		if(strcmp(device->address, built) == 0)
			return(device);
	}
	return(NULL);
}

mp_device_registry_t *mp_device_registry_find(mp_kernel_t *kernel, mp_device_class_t class, int index) {
	mp_list_item_t *item;

	if(!kernel)
		return(NULL);
	for(item = kernel->devices.first; item; item = item->next) {
		mp_device_registry_t *device = item->user;

		if(device->class == class && device->index == index)
			return(device);
	}
	return(NULL);
}

#endif
