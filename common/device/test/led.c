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

/* Host test of the LED contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../led.c"
#include "../device.c"

static int failed;
static int starts;
static int stops;
static mp_device_led_t led;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void _led_set(mp_device_led_t *self, int on) {
	self->state = on ? 1 : 0;
}

static void _led_turn(mp_device_led_t *self) {
	self->state = self->state ? 0 : 1;
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
	led.set = _led_set;
	led.turn = _led_turn;
	led.state = 0;
	led.user = &led;
	if(!mp_device_registry_register(driver, device, MP_DEVICE_LED, 0, &led, "led", "heart", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t ledOps = {
	"led", "LED", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_LED),
	_load, NULL, NULL, _start, _stop
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_registry_t ledDev;
	mp_device_subscribe_t a, b;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(&ledDev, 0, sizeof(ledDev));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	mp_device_init(&kernel);

	expect(mp_device_driver_register(&kernel, &driver, &ledOps, &ledDev, NULL) == TRUE, "load");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_LED, 0) == &ledDev, "led registered");

	expect(mp_device_led(&ledDev, &a) == &led, "subscribe returns the led");
	expect(starts == 1 && driver.users == 1, "first subscriber starts");
	expect(mp_device_led(&ledDev, &a) == &led, "same subscriber");
	expect(driver.users == 1, "same subscriber counts once");
	expect(mp_device_led(&ledDev, &b) == NULL, "led is exclusive");
	expect(driver.users == 1, "refused does not count");

	mp_device_led_set(&led, 1);
	expect(led.state == 1 && mp_device_led_state(&led) == 1, "on");
	mp_device_led_set(&led, 0);
	expect(led.state == 0, "off");
	mp_device_led_turn(&led);
	expect(led.state == 1, "turn toggles on");
	mp_device_led_turn(&led);
	expect(led.state == 0, "turn toggles off");

	mp_device_subscribe_end(&a);
	expect(stops == 1 && driver.users == 0 && driver.running == NO, "last subscriber stops");
	expect(mp_device_led(&ledDev, &a) == &led, "resubscribe");
	expect(starts == 2, "start again");

	mp_device_fini(&kernel);
	expect(stops == 2, "fini stops");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
