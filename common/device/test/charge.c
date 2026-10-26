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

/* Host test of the charger contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../measure.c"
#include "../charge.c"
#include "../device.c"

static int failed;
static int starts;
static int stops;
static mp_device_charge_t face;

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
	mp_device_registry_t *device = self;

	(void)kernel;
	(void)options;
	memset(&face, 0, sizeof(face));
	if(!mp_device_registry_register(driver, device, MP_DEVICE_CHARGER, 0, &face, "mcp73833", "stat", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t chargeOps = {
	"charger/mcp73833", "MCP73833", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_CHARGER),
	_load, NULL, NULL, _start, _stop
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_registry_t dev;
	mp_device_subscribe_t a, b;
	mp_device_charge_t *charge;
	mp_device_charge_state_t state;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(&dev, 0, sizeof(dev));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	mp_device_init(&kernel);

	expect(mp_device_measure_class(MP_DEVICE_CHARGER) == NO, "charger is not a measure");
	expect(strcmp(mp_device_charge_text(MP_DEVICE_CHARGE_CHARGING), "charging") == 0, "charging");
	expect(strcmp(mp_device_charge_text(MP_DEVICE_CHARGE_DONE), "done") == 0, "done");
	expect(strcmp(mp_device_charge_text(MP_DEVICE_CHARGE_WAITING), "waiting") == 0, "waiting");
	expect(mp_device_charge_text((mp_device_charge_state_t)9)[0] == 0, "unknown state has no text");

	expect(mp_device_driver_register(&kernel, &driver, &chargeOps, &dev, NULL) == TRUE, "load");
	expect(strcmp(dev.address, "charge0.mcp73833.stat") == 0, "address");

	charge = mp_device_charge(&dev, &a);
	expect(charge == &face, "subscribe returns the charger");
	expect(starts == 1 && driver.users == 1, "first subscriber starts");
	expect(mp_device_charge_read(charge, &state) == FALSE, "nothing published yet");

	mp_device_charge_publish(charge, MP_DEVICE_CHARGE_CHARGING);
	expect(mp_device_charge_read(charge, &state) == TRUE, "read");
	expect(state == MP_DEVICE_CHARGE_CHARGING, "charging");
	mp_device_charge_publish(charge, MP_DEVICE_CHARGE_DONE);
	expect(mp_device_charge_read(charge, &state) == TRUE && state == MP_DEVICE_CHARGE_DONE, "done replaces charging");

	memset(&face, 0, sizeof(face));
	mp_device_charge_sample(&face, MP_DEVICE_CHARGE_CHARGING);
	expect(mp_device_charge_read(&face, &state) == FALSE, "one sample stays unpublished");
	mp_device_charge_sample(&face, MP_DEVICE_CHARGE_CHARGING);
	expect(mp_device_charge_read(&face, &state) == TRUE && state == MP_DEVICE_CHARGE_CHARGING, "the same sample twice is the state");
	mp_device_charge_sample(&face, (mp_device_charge_state_t)9);
	mp_device_charge_sample(&face, MP_DEVICE_CHARGE_DONE);
	expect(mp_device_charge_read(&face, &state) == TRUE && state == MP_DEVICE_CHARGE_CHARGING, "one done and anything else stay on charging");
	mp_device_charge_sample(&face, MP_DEVICE_CHARGE_DONE);
	expect(mp_device_charge_read(&face, &state) == TRUE && state == MP_DEVICE_CHARGE_DONE, "done held across two samples replaces charging");
	mp_device_charge_sample(&face, MP_DEVICE_CHARGE_WAITING);
	expect(mp_device_charge_read(&face, &state) == TRUE && state == MP_DEVICE_CHARGE_DONE, "one waiting stays on done");
	mp_device_charge_sample(&face, MP_DEVICE_CHARGE_WAITING);
	expect(mp_device_charge_read(&face, &state) == TRUE && state == MP_DEVICE_CHARGE_WAITING, "waiting held across two samples replaces done");

	expect(mp_device_charge(&dev, &b) == &face, "shared");
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
