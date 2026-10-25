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

/* Host test of driver register and unregister. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../driver.c"

static int failed;
static int loads;
static int unloads;
static int loadFails;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static mp_ret_t _load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	(void)kernel;
	(void)options;
	expect(driver->self == self, "load sees self");
	expect(driver->name != NULL, "load sees name");
	loads++;
	if(loadFails)
		return(FALSE);
	return(TRUE);
}

static void _unload(void *self) {
	(void)self;
	unloads++;
}

static const mp_device_driver_ops_t ops = {
	"sensor/ina219", "TI INA219 current monitor", "1.0.0", 0, _load, _unload, NULL, NULL, NULL
};

static const mp_device_driver_ops_t broken = {
	"sensor/ina219", "", "1.0.0", 0, _load, _unload, NULL, NULL, NULL
};

static const mp_device_driver_ops_t utf8 = {
	"sensor/caf\xc3\xa9", "text", "1.0.0", 0, _load, _unload, NULL, NULL, NULL
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t a, b, extra[MP_DEVICE_DRIVER_MAX];
	mp_list_item_t device;
	char self;
	int i;

	memset(&kernel, 0, sizeof(kernel));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	mp_device_driver_init(&kernel);

	expect(mp_device_driver_register(NULL, &a, &ops, &self, NULL) == FALSE, "null kernel");
	expect(mp_device_driver_register(&kernel, &a, &ops, NULL, NULL) == FALSE, "null self");
	expect(mp_device_driver_register(&kernel, &a, &broken, &self, NULL) == FALSE, "empty description");
	expect(mp_device_driver_register(&kernel, &a, &utf8, &self, NULL) == FALSE, "utf8 name");
	expect(mp_device_driver_count(&kernel) == 0, "still empty");

	loadFails = 1;
	expect(mp_device_driver_register(&kernel, &a, &ops, &self, NULL) == FALSE, "load fails");
	expect(mp_device_driver_count(&kernel) == 0, "not kept");
	expect(a.kernel == NULL, "cleared after failed load");
	loadFails = 0;
	loads = 0;

	expect(mp_device_driver_register(&kernel, &a, &ops, &self, NULL) == TRUE, "register");
	expect(loads == 1, "loaded once");
	expect(a.loaded == YES, "loaded flag");
	expect(strcmp(a.name, "sensor/ina219") == 0, "index keeps the slash");
	expect(strcmp(a.description, "TI INA219 current monitor") == 0, "description");
	expect(strcmp(a.version, "1.0.0") == 0, "driver version");
	expect(mp_device_driver_register(&kernel, &a, &ops, &self, NULL) == TRUE, "again");
	expect(loads == 1, "no second load");
	expect(mp_device_driver_count(&kernel) == 1, "one driver");
	expect(mp_device_driver_at(&kernel, 0) == &a, "at 0");
	expect(mp_device_driver_named(&kernel, "sensor/ina219") == &a, "named");
	expect(mp_device_driver_named(&kernel, "missing") == NULL, "missing");

	expect(mp_device_driver_register(&kernel, &b, &ops, &self, NULL) == TRUE, "same driver twice");
	expect(mp_device_driver_count(&kernel) == 2, "two loads");
	expect(mp_device_driver_unregister(&kernel, &b) == TRUE, "drop the second");

	memset(&device, 0, sizeof(device));
	mp_list_add_last(&a.devices, &device, &device);
	expect(mp_device_driver_unregister(&kernel, &a) == FALSE, "devices block unregister");
	expect(mp_device_driver_count(&kernel) == 1, "still registered");
	mp_list_remove(&a.devices, &device);

	expect(mp_device_driver_unregister(&kernel, &a) == TRUE, "unregister");
	expect(unloads == 2, "unloaded");
	expect(mp_device_driver_unregister(&kernel, &a) == FALSE, "second unregister");
	expect(mp_device_driver_count(&kernel) == 0, "empty");

	for(i = 0; i < MP_DEVICE_DRIVER_MAX; i++)
		memset(&extra[i], 0, sizeof(extra[i]));
	{
		static char selves[MP_DEVICE_DRIVER_MAX];

		for(i = 0; i < MP_DEVICE_DRIVER_MAX; i++)
			expect(mp_device_driver_register(&kernel, &extra[i], &ops, &selves[i], NULL) == TRUE, "fill");
	}
	memset(&b, 0, sizeof(b));
	expect(mp_device_driver_register(&kernel, &b, &ops, &self, NULL) == FALSE, "full");
	expect(mp_device_driver_count(&kernel) == MP_DEVICE_DRIVER_MAX, "at max");

	mp_device_driver_fini(&kernel);
	expect(mp_device_driver_count(&kernel) == 0, "fini drops them");
	expect(unloads == 2 + MP_DEVICE_DRIVER_MAX, "fini unloads");
	memset(&a, 0, sizeof(a));
	expect(mp_device_driver_register(&kernel, &a, &ops, &self, NULL) == TRUE, "after fini");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
