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

/* Host test of device subscribe. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../device.c"

static int failed;
static int imuStarts;
static int imuStops;
static int panelStarts;
static int panelStops;
static int gone;
static int stopsDuringGone;
static int refusedDuringGone;
static int tryAgain;
static char goneAddress[MP_DEVICE_ADDRESS];
static char imuInst;
static char tempInst;
static char ledInst;
static char dispInst;
static mp_device_registry_t accelDev;
static mp_device_registry_t tempDev;
static mp_device_registry_t ledDev;
static mp_device_registry_t dispDev;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void _imu_start(void *self) {
	(void)self;
	imuStarts++;
}

static void _imu_stop(void *self) {
	(void)self;
	imuStops++;
}

static void _panel_start(void *self) {
	(void)self;
	panelStarts++;
}

static void _panel_stop(void *self) {
	(void)self;
	panelStops++;
}

static void _gone(mp_device_registry_t *device, void *user) {
	(void)user;
	gone++;
	stopsDuringGone = imuStops;
	strncpy(goneAddress, device->address, sizeof(goneAddress) - 1);
	goneAddress[sizeof(goneAddress) - 1] = 0;
	if(tryAgain && mp_device_subscribe(device, user) == NULL)
		refusedDuringGone++;
}

static int imuMode;

static void _imu_scan(mp_device_driver_t *driver, void *self) {
	(void)self;
	if(imuMode == 1)
		return;
	mp_device_registry_register(driver, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "aa", NULL);
	mp_device_registry_register(driver, &tempDev, MP_DEVICE_TEMPERATURE, 0, &tempInst, "02", "aa", NULL);
}

static const mp_device_driver_ops_t imuOps = {
	"sensor/smb380", "Bosch SMB380", "1.0.0",
	(unsigned int)((1u << MP_DEVICE_ACCELEROMETER) | (1u << MP_DEVICE_TEMPERATURE)),
	NULL, NULL, _imu_scan, _imu_start, _imu_stop
};

static const mp_device_driver_ops_t panelOps = {
	"panel", "Panel", "1.0.0",
	(unsigned int)((1u << MP_DEVICE_LED) | (1u << MP_DEVICE_DISPLAY)),
	NULL, NULL, NULL, _panel_start, _panel_stop
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t imu, panel;
	mp_device_subscribe_t a, b, c, ledSub, dispSub;
	char self;
	void *got;

	memset(&kernel, 0, sizeof(kernel));
	memset(&imu, 0, sizeof(imu));
	memset(&panel, 0, sizeof(panel));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	memset(&c, 0, sizeof(c));
	memset(&ledSub, 0, sizeof(ledSub));
	memset(&dispSub, 0, sizeof(dispSub));
	a.on_gone = _gone;
	a.user = &a;
	mp_device_init(&kernel);

	expect(mp_device_driver_register(&kernel, &imu, &imuOps, &self, NULL) == TRUE, "imu");
	expect(mp_device_driver_register(&kernel, &panel, &panelOps, &self, NULL) == TRUE, "panel");
	expect(mp_device_registry_register(&imu, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "aa", NULL) != NULL, "accel");
	expect(mp_device_registry_register(&imu, &tempDev, MP_DEVICE_TEMPERATURE, 0, &tempInst, "02", "aa", NULL) != NULL, "temp");
	expect(mp_device_registry_register(&panel, &ledDev, MP_DEVICE_LED, 0, &ledInst, "led", "heart", NULL) != NULL, "led");
	expect(mp_device_registry_register(&panel, &dispDev, MP_DEVICE_DISPLAY, 0, &dispInst, "lcd", "1", NULL) != NULL, "display");

	expect(mp_device_subscribe(NULL, &a) == NULL, "no device");
	expect(mp_device_subscribe(&accelDev, NULL) == NULL, "no subscriber");

	got = mp_device_subscribe(&accelDev, &a);
	expect(got == &imuInst, "accel instance");
	expect(imuStarts == 1 && imu.users == 1 && imu.running == YES, "first subscriber starts");
	expect(mp_device_subscribe(&accelDev, &a) == &imuInst, "same subscriber");
	expect(imuStarts == 1 && imu.users == 1, "same subscriber counts once");

	expect(mp_device_subscribe(&tempDev, &b) == &tempInst, "temp instance");
	expect(imuStarts == 1 && imu.users == 2, "second device stays started");
	expect(mp_device_subscribe(&accelDev, &c) == &imuInst, "shared input");
	expect(imu.users == 3 && imuStarts == 1, "shared input counts");

	mp_device_subscribe_end(&c);
	expect(imu.users == 2 && imuStops == 0, "one reader left does not stop");
	mp_device_subscribe_end(&a);
	mp_device_subscribe_end(&b);
	expect(imu.users == 0 && imuStops == 1 && imu.running == NO, "last subscriber stops");
	mp_device_subscribe_end(&a);
	expect(imuStops == 1, "end again does nothing");

	expect(mp_device_subscribe(&ledDev, &ledSub) == &ledInst, "led instance");
	expect(panelStarts == 1 && panel.users == 1, "led starts the panel");
	expect(mp_device_subscribe(&ledDev, &ledSub) == &ledInst, "same led subscriber");
	expect(panel.users == 1, "same led subscriber counts once");
	expect(mp_device_subscribe(&ledDev, &dispSub) == NULL, "led is exclusive");
	expect(panel.users == 1, "refused led does not count");
	expect(mp_device_subscribe(&dispDev, &dispSub) == &dispInst, "display beside the led");
	expect(panelStarts == 1 && panel.users == 2, "display shares the panel start");
	expect(mp_device_subscribe(&dispDev, &a) == NULL, "display is exclusive");
	mp_device_subscribe_end(&ledSub);
	expect(panelStops == 0 && panel.users == 1, "display keeps the panel started");
	mp_device_subscribe_end(&dispSub);
	expect(panelStops == 1 && panel.running == NO, "last panel subscriber stops");

	expect(mp_device_subscribe(&accelDev, &a) == &imuInst, "subscribe again");
	expect(imuStarts == 2, "start again");
	expect(mp_device_subscribe(&tempDev, &a) == NULL, "struct already holds accel");
	expect(imu.users == 1, "refused move does not count");
	expect(mp_device_registry_register(&imu, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "aa", "1.2") == &accelDev, "version");
	expect(gone == 0 && a.device == &accelDev && imuStops == 1, "version keeps the subscriber");

	imuMode = 0;
	mp_device_registry_register(&imu, &accelDev, MP_DEVICE_ACCELEROMETER, 0, &imuInst, "02", "bb", NULL);
	expect(gone == 1, "new serial tells the subscriber");
	expect(strcmp(goneAddress, "accel0.02.aa") == 0, "old address during on_gone");
	expect(stopsDuringGone == 1, "stop waits until on_gone returns");
	expect(imuStops == 2 && imu.running == NO, "identity change stops");
	expect(a.device == NULL, "subscriber unlinked");
	expect(strcmp(accelDev.address, "accel0.02.bb") == 0, "new address");
	expect(mp_device_subscribe(&accelDev, &a) == &imuInst, "resubscribe");
	expect(imuStarts == 3 && imu.users == 1, "resubscribe starts");

	imuMode = 1;
	tryAgain = 1;
	refusedDuringGone = 0;
	mp_device_registry_scan(&kernel, MP_DEVICE_ACCELEROMETER);
	expect(gone == 2, "missing chip tells the subscriber");
	expect(stopsDuringGone == 2, "removal stops after on_gone");
	expect(refusedDuringGone == 1, "removal refuses a new subscribe");
	expect(imuStops == 3 && imu.users == 0, "removal releases the driver");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_ACCELEROMETER, 0) == NULL, "accel removed");
	expect(a.device == NULL, "subscriber clear");

	mp_device_fini(&kernel);
	expect(panelStops == 1, "fini leaves a stopped panel");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
