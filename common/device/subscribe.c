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
 * Subscribers. The caller owns each mp_device_subscribe_t.
 * Subscribe links it and returns the instance. End unlinks it.
 * The first subscriber of a driver starts it, the last stops it.
 */

static mp_bool_t _mp_device_subscribe_output(mp_device_class_t class) {
	if(class == MP_DEVICE_DISPLAY || class == MP_DEVICE_LED)
		return(YES);
	return(NO);
}

static void _mp_device_subscribe_acquire(mp_device_driver_t *driver) {
	driver->users++;
	if(driver->running == YES)
		return;
	driver->running = YES;
	if(driver->ops && driver->ops->start)
		driver->ops->start(driver->self);
}

static void _mp_device_subscribe_release(mp_device_driver_t *driver) {
	if(driver->users <= 0)
		return;
	driver->users--;
	if(driver->users > 0 || driver->running == NO)
		return;
	driver->running = NO;
	if(driver->ops && driver->ops->stop)
		driver->ops->stop(driver->self);
}

static void _mp_device_subscribe_stop(mp_device_driver_t *driver) {
	if(driver->users > 0 || driver->running == NO)
		return;
	driver->running = NO;
	if(driver->ops && driver->ops->stop)
		driver->ops->stop(driver->self);
}

void *mp_device_subscribe(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	if(!device || !device->driver || device->removed == YES || !sub)
		return(NULL);
	if(sub->device == device)
		return(device->instance);
	if(sub->device)
		return(NULL);
	if(_mp_device_subscribe_output(device->class) == YES && device->subs.first)
		return(NULL);
	sub->device = device;
	sub->item.next = NULL;
	sub->item.prev = NULL;
	mp_list_add_last(&device->subs, &sub->item, sub);
	_mp_device_subscribe_acquire(device->driver);
	return(device->instance);
}

void mp_device_subscribe_end(mp_device_subscribe_t *sub) {
	mp_device_registry_t *device;
	mp_device_driver_t *driver;

	if(!sub || !sub->device)
		return;
	device = sub->device;
	driver = device->driver;
	mp_list_remove(&device->subs, &sub->item);
	sub->device = NULL;
	if(driver)
		_mp_device_subscribe_release(driver);
}

void mp_device_subscribe_notify(mp_device_registry_t *device) {
	mp_device_driver_t *driver;
	mp_list_item_t *item;
	mp_list_item_t *next;

	if(!device || !device->driver)
		return;
	driver = device->driver;
	item = device->subs.first;
	mp_list_init(&device->subs);
	while(item) {
		mp_device_subscribe_t *sub = item->user;

		next = item->next;
		item->next = NULL;
		item->prev = NULL;
		sub->device = NULL;
		if(driver->users > 0)
			driver->users--;
		if(sub->on_gone)
			sub->on_gone(device, sub->user);
		item = next;
	}
	_mp_device_subscribe_stop(driver);
}

#endif
