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

#include <pinout.h>
#include <gen/devices.h>

int mp_device_count(void) {
	return(MP_DEVICE_COUNT);
}

mp_device_t *mp_device_at(int index) {
	if(index < 0 || index >= MP_DEVICE_COUNT)
		return(NULL);
	return(&_mp_devices[index]);
}

mp_device_t *mp_device_find(mp_device_class_t type, int rank) {
	int index;

	for(index = 0; index < MP_DEVICE_COUNT; index++) {
		if(_mp_devices[index].driver->type != type)
			continue;
		if(rank-- == 0)
			return(&_mp_devices[index]);
	}
	return(NULL);
}

mp_device_t *mp_device_named(const char *name) {
	int index;

	if(!name)
		return(NULL);
	for(index = 0; index < MP_DEVICE_COUNT; index++)
		if(strcmp(_mp_devices[index].name, name) == 0)
			return(&_mp_devices[index]);
	return(NULL);
}

static const char *_mp_device_text(const char *text) {
	if(!text || !text[0])
		return(NULL);
	return(text);
}

mp_ret_t mp_device_open(mp_kernel_t *kernel, mp_device_t *device) {
	if(!device)
		return(FALSE);
	if(device->opened == YES)
		return(TRUE);
	device->model = NULL;
	device->serial = NULL;
	device->version = NULL;
	if(device->driver->open(kernel, device->self, device->options, device->name) == FALSE)
		return(FALSE);
	device->opened = YES;
	return(TRUE);
}

void mp_device_close(mp_device_t *device) {
	if(!device || device->opened == NO)
		return;
	device->driver->close(device->self);
	device->opened = NO;
}

void *mp_device_face(mp_device_t *device) {
	if(!device || device->opened == NO || !device->driver->face)
		return(NULL);
	return(device->driver->face(device->self));
}

void mp_device_set(mp_device_t *device, int value) {
	if(!device || device->opened == NO || !device->driver->set)
		return;
	device->driver->set(device->self, value);
}

void mp_device_identify(void *self, const char *model, const char *serial, const char *version) {
	int index;

	if(!self)
		return;
	for(index = 0; index < MP_DEVICE_COUNT; index++) {
		if(_mp_devices[index].self != self)
			continue;
		_mp_devices[index].model = _mp_device_text(model);
		_mp_devices[index].serial = _mp_device_text(serial);
		_mp_devices[index].version = _mp_device_text(version);
		return;
	}
}

#endif
