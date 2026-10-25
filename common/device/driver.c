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
 * Drivers loaded on the kernel. Each one owns the devices it will register.
 * The caller keeps the mp_device_driver_t. Register loads it, unregister unloads it.
 */

static int _mp_device_driver_count(mp_kernel_t *kernel) {
	mp_list_item_t *item;
	int n = 0;

	if(!kernel)
		return(0);
	for(item = kernel->drivers.first; item; item = item->next)
		n++;
	return(n);
}

static mp_device_driver_t *_mp_device_driver_named(mp_kernel_t *kernel, const char *name) {
	mp_list_item_t *item;

	if(!kernel || !name)
		return(NULL);
	for(item = kernel->drivers.first; item; item = item->next) {
		mp_device_driver_t *driver = item->user;

		if(driver->name && strcmp(driver->name, name) == 0)
			return(driver);
	}
	return(NULL);
}

static int _mp_device_driver_next_index(mp_kernel_t *kernel, mp_device_driver_t *self, int kind) {
	mp_list_item_t *item;
	int max = -1;
	int i;

	for(item = kernel->drivers.first; item; item = item->next) {
		mp_device_driver_t *driver = item->user;

		if(driver == self)
			continue;
		if(driver->index0[kind] > max)
			max = driver->index0[kind];
		for(i = 0; i < MP_DEVICE_DRIVER_EXTRA; i++) {
			if(driver->extra[i].used == NO || driver->extra[i].class != kind)
				continue;
			if(driver->extra[i].index > max)
				max = driver->extra[i].index;
		}
	}
	if(self) {
		if(self->index0[kind] > max)
			max = self->index0[kind];
		for(i = 0; i < MP_DEVICE_DRIVER_EXTRA; i++) {
			if(self->extra[i].used == NO || self->extra[i].class != kind)
				continue;
			if(self->extra[i].index > max)
				max = self->extra[i].index;
		}
	}
	return(max + 1);
}

int mp_device_driver_index(mp_device_driver_t *driver, mp_device_class_t class, int channel) {
	int i;

	if(!driver || !driver->kernel || channel < 0 || channel > 255)
		return(-1);
	if(class < 0 || class >= MP_DEVICE_CLASS_COUNT)
		return(-1);
	if((driver->classes & (1u << class)) == 0)
		return(-1);
	if(channel == 0)
		return(driver->index0[class]);
	for(i = 0; i < MP_DEVICE_DRIVER_EXTRA; i++) {
		if(driver->extra[i].used == NO || driver->extra[i].class != class)
			continue;
		if(driver->extra[i].channel == channel)
			return(driver->extra[i].index);
	}
	for(i = 0; i < MP_DEVICE_DRIVER_EXTRA; i++) {
		if(driver->extra[i].used == YES)
			continue;
		driver->extra[i].used = YES;
		driver->extra[i].class = (unsigned char)class;
		driver->extra[i].channel = (unsigned char)channel;
		driver->extra[i].index = (short)_mp_device_driver_next_index(driver->kernel, driver, class);
		return(driver->extra[i].index);
	}
	return(-1);
}

/** Printable ASCII, non-empty. A slash, a dot and a space stay as written. */
static mp_ret_t _mp_device_driver_text(const char *text) {
	const unsigned char *p;

	if(!text || !text[0])
		return(FALSE);
	for(p = (const unsigned char *)text; *p; p++)
		if(*p < 0x20 || *p >= 0x80)
			return(FALSE);
	return(TRUE);
}

static void _mp_device_driver_detach(mp_device_driver_t *driver) {
	mp_kernel_t *kernel = driver->kernel;

	if(driver->loaded == YES && driver->ops && driver->ops->unload)
		driver->ops->unload(driver->self);
	mp_list_remove(&kernel->drivers, &driver->item);
	memset(driver, 0, sizeof(*driver));
}

void mp_device_driver_init(mp_kernel_t *kernel) {
	if(!kernel)
		return;
	mp_list_init(&kernel->drivers);
}

void mp_device_driver_fini(mp_kernel_t *kernel) {
	if(!kernel)
		return;

	while(kernel->drivers.first) {
		mp_device_driver_t *driver = kernel->drivers.first->user;

		if(driver->devices.first)
			mp_printk("Driver %s still has devices at fini", driver->name);
		mp_list_init(&driver->devices);
		_mp_device_driver_detach(driver);
	}
}

mp_ret_t mp_device_driver_register(mp_kernel_t *kernel, mp_device_driver_t *driver, const mp_device_driver_ops_t *ops, void *self, mp_options_t *options) {
	int i;

	if(!kernel || !driver || !ops || !self)
		return(FALSE);
	if(driver->kernel == kernel)
		return(TRUE);
	if(driver->kernel)
		return(FALSE);
	if(_mp_device_driver_text(ops->name) == FALSE)
		return(FALSE);
	if(_mp_device_driver_text(ops->description) == FALSE)
		return(FALSE);
	if(_mp_device_driver_text(ops->version) == FALSE)
		return(FALSE);
	if(_mp_device_driver_count(kernel) >= MP_DEVICE_DRIVER_MAX)
		return(FALSE);

	memset(driver, 0, sizeof(*driver));
	driver->kernel = kernel;
	driver->ops = ops;
	driver->self = self;
	driver->options = options;
	driver->name = ops->name;
	driver->description = ops->description;
	driver->version = ops->version;
	driver->classes = ops->classes;
	mp_list_init(&driver->devices);
	for(i = 0; i < MP_DEVICE_CLASS_COUNT; i++)
		driver->index0[i] = -1;
	for(i = 0; i < MP_DEVICE_CLASS_COUNT; i++) {
		if((driver->classes & (1u << i)) == 0)
			continue;
		driver->index0[i] = (short)_mp_device_driver_next_index(kernel, driver, i);
	}

	if(ops->load) {
		if(ops->load(kernel, driver, self, options) == FALSE) {
			memset(driver, 0, sizeof(*driver));
			return(FALSE);
		}
		driver->loaded = YES;
	}

	mp_list_add_last(&kernel->drivers, &driver->item, driver);
	return(TRUE);
}

mp_ret_t mp_device_driver_unregister(mp_kernel_t *kernel, mp_device_driver_t *driver) {
	if(!kernel || !driver || driver->kernel != kernel)
		return(FALSE);
	if(driver->devices.first) {
		mp_printk("Driver %s still has devices", driver->name);
		return(FALSE);
	}
	_mp_device_driver_detach(driver);
	return(TRUE);
}

int mp_device_driver_count(mp_kernel_t *kernel) {
	return(_mp_device_driver_count(kernel));
}

mp_device_driver_t *mp_device_driver_at(mp_kernel_t *kernel, int index) {
	mp_list_item_t *item;

	if(!kernel || index < 0)
		return(NULL);
	for(item = kernel->drivers.first; item; item = item->next) {
		if(index-- == 0)
			return(item->user);
	}
	return(NULL);
}

mp_device_driver_t *mp_device_driver_named(mp_kernel_t *kernel, const char *name) {
	return(_mp_device_driver_named(kernel, name));
}

#endif
