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

/* Host test of the measure contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../led.c"
#include "../button.c"
#include "../measure.c"
#include "../device.c"

static int failed;
static int starts;
static int stops;
static mp_device_measure_t accel;
static mp_device_measure_t temp;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
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
	mp_device_registry_t *accelDev = self;
	mp_device_registry_t *tempDev;

	(void)kernel;
	(void)options;
	tempDev = ((mp_device_registry_t *)self) + 1;
	if(!mp_device_registry_register(driver, accelDev, MP_DEVICE_ACCELEROMETER, 0, &accel, "02", "aa", NULL))
		return(FALSE);
	if(!mp_device_registry_register(driver, tempDev, MP_DEVICE_TEMPERATURE, 0, &temp, "02", "aa", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t imuOps = {
	"sensor/smb380", "Bosch SMB380", "1.0.0",
	(unsigned int)((1u << MP_DEVICE_ACCELEROMETER) | (1u << MP_DEVICE_TEMPERATURE)),
	_load, NULL, NULL, _start, _stop
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_registry_t devs[2];
	mp_device_subscribe_t a, b;
	mp_device_measure_t *m;
	float v[3];
	unsigned long age;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(devs, 0, sizeof(devs));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	memset(&accel, 0, sizeof(accel));
	memset(&temp, 0, sizeof(temp));
	mp_device_init(&kernel);

	expect(mp_device_driver_register(&kernel, &driver, &imuOps, devs, NULL) == TRUE, "load");
	expect(strcmp(devs[0].address, "accel0.02.aa") == 0, "accel address");
	expect(strcmp(devs[1].address, "temp0.02.aa") == 0, "temp address");

	m = mp_device_measure(&devs[0], &a);
	expect(m == &accel, "subscribe returns the measure");
	expect(starts == 1 && driver.users == 1, "first subscriber starts");
	expect(mp_device_measure(&devs[1], &b) == &temp, "second measure");
	expect(driver.users == 2 && starts == 1, "same driver stays started");

	expect(mp_device_measure_read(&accel, v, NULL) == FALSE, "nothing published yet");
	mp_device_measure_publish3(&accel, 1.0f, 2.0f, 3.0f);
	expect(mp_device_measure_read(&accel, v, &age) == TRUE, "read after publish3");
	expect(v[0] == 1.0f && v[1] == 2.0f && v[2] == 3.0f, "xyz values");
	expect(age == 1, "age");

	mp_device_measure_publish(&temp, 25.5f);
	expect(mp_device_measure_read(&temp, v, NULL) == TRUE, "read temp");
	expect(v[0] == 25.5f && v[1] == 0.0f, "single value");

	expect(mp_device_measure_axes(MP_DEVICE_ACCELEROMETER) == 3, "accel 3 axes");
	expect(mp_device_measure_axes(MP_DEVICE_TEMPERATURE) == 1, "temp 1 axis");
	expect(strcmp(mp_device_measure_unit(MP_DEVICE_ACCELEROMETER), "g") == 0, "accel unit g");
	expect(strcmp(mp_device_measure_unit(MP_DEVICE_TEMPERATURE), "C") == 0, "temp unit C");
	expect(strcmp(mp_device_measure_unit(MP_DEVICE_BAROMETER), "Pa") == 0, "baro unit Pa");
	expect(strcmp(mp_device_measure_unit(MP_DEVICE_ILLUMINANCE), "lx") == 0, "lux unit lx");
	expect(strcmp(mp_device_measure_unit(MP_DEVICE_LED), "") == 0, "led no unit");
	expect(mp_device_measure_class(MP_DEVICE_TEMPERATURE) == YES, "temp is a measure");
	expect(mp_device_measure_class(MP_DEVICE_LED) == NO, "led is not a measure");
	expect(mp_device_measure_next(&kernel, NULL) == &devs[0], "first measure");
	expect(mp_device_measure_next(&kernel, &devs[0]) == &devs[1], "next measure");
	expect(mp_device_measure_next(&kernel, &devs[1]) == NULL, "end of measures");

	mp_device_subscribe_end(&a);
	expect(stops == 0 && driver.users == 1, "one measure left stays started");
	mp_device_subscribe_end(&b);
	expect(stops == 1 && driver.running == NO, "last measure stops");

	mp_device_fini(&kernel);
	expect(stops == 1, "fini leaves a stopped driver");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
