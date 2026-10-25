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

/* Host test of device register and scan. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../device.c"

static int failed;
static int gone;
static int imuScans;
static int otherScans;
static int imuMode;
static char imuInst;
static char tempInst;
static char ch1Inst;
static char otherInst;
static char ledInst;
static mp_device_registry_t accelDev;
static mp_device_registry_t tempDev;
static mp_device_registry_t ch1Dev;
static mp_device_registry_t otherDev;
static mp_device_registry_t ledDev;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void _gone(mp_device_registry_t *device, void *user) {
	(void)device;
	(void)user;
	gone++;
}

static void _imu_scan(mp_device_driver_t *driver, void *self) {
	(void)self;
	imuScans++;
	if(imuMode == 1)
		return;
	if(imuMode == 2) {
		mp_device_registry_register(driver, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "bb", "9");
		mp_device_registry_register(driver, &tempDev, MP_DEVICE_TEMPERATURE, 0, &tempInst, "02", "bb", NULL);
		return;
	}
	mp_device_registry_register(driver, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "AA", NULL);
	mp_device_registry_register(driver, &tempDev, MP_DEVICE_TEMPERATURE, 0, &tempInst, "02", "aa", NULL);
}

static int otherMode;

static void _other_scan(mp_device_driver_t *driver, void *self) {
	(void)self;
	otherScans++;
	if(otherMode == 1)
		return;
	mp_device_registry_register(driver, &otherDev, MP_DEVICE_ACCELEROMETER, 0, &otherInst, "02", "cc", NULL);
}

static mp_ret_t _led_load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	(void)kernel;
	(void)self;
	(void)options;
	if(!mp_device_registry_register(driver, &ledDev, MP_DEVICE_LED, 0, &ledInst, "LED", "heart beat", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t imuOps = {
	"sensor/smb380", "Bosch SMB380", "1.0.0",
	(unsigned int)((1u << MP_DEVICE_ACCELEROMETER) | (1u << MP_DEVICE_TEMPERATURE)),
	NULL, NULL, _imu_scan, NULL, NULL
};

static const mp_device_driver_ops_t otherOps = {
	"sensor/smb380", "Bosch SMB380", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_ACCELEROMETER),
	NULL, NULL, _other_scan, NULL, NULL
};

static const mp_device_driver_ops_t ledOps = {
	"led", "LED", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_LED),
	_led_load, NULL, NULL, NULL, NULL
};

static void _watch(mp_device_registry_t *device) {
	static mp_device_subscribe_t sub;

	memset(&sub, 0, sizeof(sub));
	sub.on_gone = _gone;
	mp_device_subscribe(device, &sub);
}

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t imu, other, led;
	mp_device_registry_t *accel;
	mp_device_registry_t *second;
	mp_device_registry_t *temp;
	mp_device_registry_t *light;
	char self;

	{
		mp_options_t named[] = {
			{ "model", "smb380" },
			{ "serial", "stm32-lcd" },
			{ NULL, NULL }
		};
		mp_options_t bare[] = {
			{ "model", "smb380" },
			{ NULL, NULL }
		};
		const char *model = NULL;
		const char *serial = NULL;

		expect(mp_device_options_identity(named, &model, &serial) == TRUE, "model and serial");
		expect(model && strcmp(model, "smb380") == 0, "model text");
		expect(serial && strcmp(serial, "stm32-lcd") == 0, "serial text");
		expect(mp_device_options_identity(bare, &model, &serial) == FALSE, "serial required");
		expect(mp_device_options_identity(NULL, &model, &serial) == FALSE, "no options");
	}
	memset(&kernel, 0, sizeof(kernel));
	memset(&imu, 0, sizeof(imu));
	memset(&other, 0, sizeof(other));
	memset(&led, 0, sizeof(led));
	mp_device_init(&kernel);

	expect(mp_device_driver_register(&kernel, &imu, &imuOps, &self, NULL) == TRUE, "imu");
	expect(mp_device_driver_register(&kernel, &other, &otherOps, &self, NULL) == TRUE, "other");
	expect(mp_device_registry_register(&imu, &accelDev, MP_DEVICE_BUTTON, 0, &imuInst, "button", "left", NULL) == NULL, "class not announced");
	expect(mp_device_registry_register(&imu, &accelDev, MP_DEVICE_ACCELEROMETER, 0, NULL, "02", "aa", NULL) == NULL, "no instance");

	second = mp_device_registry_register(&other, &otherDev, MP_DEVICE_ACCELEROMETER, 0, &otherInst, "02", "cc", NULL);
	expect(second != NULL, "second binding");
	expect(second->index == 1, "accel1 reserved");
	expect(strcmp(second->address, "accel1.02.cc") == 0, "accel1 text");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 0) == NULL, "accel0 absent");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 1) == second, "find accel1");

	accel = mp_device_registry_register(&imu, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "aa", NULL);
	expect(accel != NULL && accel->index == 0, "accel0");
	expect(strcmp(accel->address, "accel0.02.aa") == 0, "accel0 text");
	_watch(accel);
	expect(mp_device_registry_register(&imu, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "AA", "1.2") == accel, "same device");
	expect(accel->version && strcmp(accel->version, "1.2") == 0, "version");
	expect(gone == 0 && accel->subs.first != NULL, "version keeps the subscriber");

	temp = mp_device_registry_register(&imu, &tempDev, MP_DEVICE_TEMPERATURE, 0, &tempInst, "foo.bar", "aa", NULL);
	expect(temp != NULL && temp->index == 0, "temp0");
	expect(strcmp(temp->address, "temp0.foo-bar.aa") == 0, "temp text");

	expect(mp_device_registry_register(&imu, &ch1Dev, MP_DEVICE_ACCELEROMETER, 1, &ch1Inst, "02", "aa", NULL) != NULL, "channel 1");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 2) != NULL, "accel2");
	expect(strcmp(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 2)->address, "accel2.02.aa") == 0, "accel2 text");
	expect(mp_device_registry_address(&kernel, "ACCEL0.02.AA") == accel, "canon lookup");
	expect(mp_device_registry_register(&imu, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "caf\xc3\xa9", "aa", NULL) == NULL, "utf8");
	expect(strcmp(accel->address, "accel0.02.aa") == 0, "utf8 leaves it");

	imuScans = 0;
	otherScans = 0;
	mp_device_registry_scan(&kernel, MP_DEVICE_LED);
	expect(imuScans == 0 && otherScans == 0, "led scan skips the imus");

	imuMode = 0;
	mp_device_registry_scan(&kernel, MP_DEVICE_ACCELEROMETER);
	expect(imuScans == 1 && otherScans == 1, "accel scan visits both");
	expect(gone == 0, "same chip keeps the subscriber");
	expect(accel->subs.first != NULL, "subscriber still chained");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_TEMPERATURE, 0) == temp, "temp refreshed too");

	imuMode = 2;
	mp_device_registry_scan(&kernel, MP_DEVICE_ANY);
	expect(gone == 1, "new serial tells the subscriber");
	expect(accel->subs.first == NULL, "subscriber dropped");
	expect(accel->index == 0, "index stays");
	expect(strcmp(accel->address, "accel0.02.bb") == 0, "new address");
	expect(strcmp(temp->address, "temp0.02.bb") == 0, "temp serial follows");

	_watch(accel);
	imuMode = 1;
	mp_device_registry_scan(&kernel, MP_DEVICE_ACCELEROMETER);
	expect(gone == 2, "missing chip tells the subscriber");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 0) == NULL, "accel0 removed");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 1) == second, "accel1 stays");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_TEMPERATURE, 0) == NULL, "temp removed");
	expect(mp_device_registry_count(&kernel) == 1, "only the other accel");

	expect(mp_device_driver_register(&kernel, &led, &ledOps, &self, NULL) == TRUE, "led load");
	light = mp_device_registry_find(&kernel, MP_DEVICE_LED, 0);
	expect(light != NULL, "led0");
	expect(strcmp(light->address, "led0.led.heart-beat") == 0, "led text");
	imuMode = 1;
	mp_device_registry_scan(&kernel, MP_DEVICE_ANY);
	expect(mp_device_registry_find(&kernel, MP_DEVICE_LED, 0) == light, "fixed led stays");

	expect(mp_device_driver_unregister(&kernel, &other) == FALSE, "device blocks unregister");
	otherMode = 1;
	mp_device_registry_scan(&kernel, MP_DEVICE_ACCELEROMETER);
	expect(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 1) == NULL, "other accel removed");
	expect(mp_device_driver_unregister(&kernel, &other) == TRUE, "unregister after the device is gone");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_LED, 0) == light, "led still there");

	mp_device_fini(&kernel);
	expect(mp_device_registry_count(&kernel) == 0, "fini drops devices");
	expect(mp_device_driver_count(&kernel) == 0, "fini drops drivers");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
