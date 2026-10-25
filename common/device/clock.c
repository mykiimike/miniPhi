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
 * Clock contract. The driver owns an mp_device_time_t and registers it as the
 * instance. A subscriber starts the driver; the driver publishes civil time
 * into the struct. This is not the kernel tick counter.
 */

mp_device_time_t *mp_device_clock(mp_device_registry_t *device, mp_device_subscribe_t *sub) {
	return((mp_device_time_t *)mp_device_subscribe(device, sub));
}

void mp_device_clock_publish(mp_device_time_t *clock, const mp_device_time_t *time) {
	if(!clock || !time)
		return;
	MP_INTERRUPT_SAFE_BEGIN
	clock->year = time->year;
	clock->month = time->month;
	clock->day = time->day;
	clock->wday = time->wday;
	clock->hour = time->hour;
	clock->minute = time->minute;
	clock->second = time->second;
	clock->running = time->running;
	clock->valid = YES;
	clock->stamp = mp_clock_ticks();
	MP_INTERRUPT_SAFE_END
}

mp_ret_t mp_device_clock_read(mp_device_time_t *clock, mp_device_time_t *out) {
	mp_device_time_t copy;

	if(!clock || !out)
		return(FALSE);
	MP_INTERRUPT_SAFE_BEGIN
	copy = *clock;
	MP_INTERRUPT_SAFE_END
	if(copy.valid != YES)
		return(FALSE);
	out->year = copy.year;
	out->month = copy.month;
	out->day = copy.day;
	out->wday = copy.wday;
	out->hour = copy.hour;
	out->minute = copy.minute;
	out->second = copy.second;
	out->running = copy.running;
	out->valid = YES;
	out->stamp = copy.stamp;
	out->set = copy.set;
	out->user = copy.user;
	return(TRUE);
}

mp_ret_t mp_device_clock_set(mp_device_time_t *clock, const mp_device_time_t *time) {
	if(!clock || !clock->set || !time)
		return(FALSE);
	return(clock->set(clock, time));
}

#endif
