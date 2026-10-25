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

/* Host test of the clock contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../measure.c"
#include "../clock.c"
#include "../device.c"

static int failed;
static int starts;
static int stops;
static int sets;
static mp_device_time_t face;
static mp_device_time_t asked;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static mp_ret_t _clock_set(mp_device_time_t *self, const mp_device_time_t *time) {
	(void)self;
	asked = *time;
	sets++;
	return(TRUE);
}

static void _start(void *self) {
	(void)self;
	starts++;
}

static void _stop(void *self) {
	(void)self;
	stops++;
}

static mp_ret_t _load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	mp_device_registry_t *device = self;

	(void)kernel;
	(void)options;
	memset(&face, 0, sizeof(face));
	face.set = _clock_set;
	face.user = &face;
	if(!mp_device_registry_register(driver, device, MP_DEVICE_CLOCK, 0, &face, "ds1307", "rtc", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t clockOps = {
	"rtc/ds1307", "DS1307", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_CLOCK),
	_load, NULL, NULL, _start, _stop
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_registry_t dev;
	mp_device_subscribe_t a, b;
	mp_device_time_t *clock;
	mp_device_time_t sample;
	mp_device_time_t next;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(&dev, 0, sizeof(dev));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	memset(&sample, 0, sizeof(sample));
	memset(&next, 0, sizeof(next));
	mp_device_init(&kernel);

	expect(mp_device_measure_class(MP_DEVICE_CLOCK) == NO, "clock is not a measure");
	expect(mp_device_driver_register(&kernel, &driver, &clockOps, &dev, NULL) == TRUE, "load");
	expect(strcmp(dev.address, "clock0.ds1307.rtc") == 0, "address");

	clock = mp_device_clock(&dev, &a);
	expect(clock == &face, "subscribe returns the clock");
	expect(starts == 1 && driver.users == 1, "first subscriber starts");
	expect(mp_device_clock_read(clock, &sample) == FALSE, "nothing published yet");

	sample.year = 2026;
	sample.month = 10;
	sample.day = 25;
	sample.wday = 1;
	sample.hour = 23;
	sample.minute = 4;
	sample.second = 5;
	sample.running = YES;
	mp_device_clock_publish(clock, &sample);
	expect(mp_device_clock_read(clock, &next) == TRUE, "read");
	expect(next.year == 2026 && next.month == 10 && next.day == 25, "date");
	expect(next.hour == 23 && next.minute == 4 && next.second == 5, "time");
	expect(next.running == YES && next.valid == YES, "running");
	expect(clock->set == _clock_set, "publish keeps the driver set");

	next.minute = 5;
	expect(mp_device_clock_set(clock, &next) == TRUE, "set");
	expect(sets == 1 && asked.minute == 5, "set reached the driver");

	expect(mp_device_clock(&dev, &b) == &face, "shared");
	expect(driver.users == 2, "second subscriber counts");
	mp_device_subscribe_end(&a);
	expect(stops == 0 && driver.users == 1, "one subscriber left stays started");
	mp_device_subscribe_end(&b);
	expect(stops == 1 && driver.running == NO, "last subscriber stops");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	return(0);
}
