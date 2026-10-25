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

/* Host test of the button contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../led.c"
#include "../button.c"
#include "../device.c"

static int failed;
static int starts;
static int stops;
static int changes;
static int lastDown;
static mp_device_button_t button;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void _on_change(mp_device_button_t *self, int down, void *user) {
	(void)user;
	changes++;
	lastDown = down;
	self->pressed = down ? 1 : 0;
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
	button.active = 1; /* WKUP PA0 is active high */
	button.pressed = 0;
	button.on_change = NULL;
	button.user = &button;
	if(!mp_device_registry_register(driver, device, MP_DEVICE_BUTTON, 0, &button, "button", "left", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t buttonOps = {
	"button", "Button", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_BUTTON),
	_load, NULL, NULL, _start, _stop
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_registry_t buttonDev;
	mp_device_subscribe_t a, b;
	mp_device_button_t *btn;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(&buttonDev, 0, sizeof(buttonDev));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	mp_device_init(&kernel);

	expect(mp_device_driver_register(&kernel, &driver, &buttonOps, &buttonDev, NULL) == TRUE, "load");
	btn = mp_device_button(&buttonDev, &a);
	expect(btn == &button, "subscribe returns the button");
	expect(starts == 1 && driver.users == 1, "first subscriber starts");
	expect(mp_device_button(&buttonDev, &b) == &button, "shared input");
	expect(driver.users == 2, "second subscriber counts");

	button.on_change = _on_change;

	/* active high: pin level 1 means pressed */
	mp_device_button_level(&button, 1);
	expect(changes == 1 && lastDown == 1 && mp_device_button_pressed(&button) == 1, "active high press");
	mp_device_button_level(&button, 1);
	expect(changes == 1, "same level does not refire");
	mp_device_button_level(&button, 0);
	expect(changes == 2 && lastDown == 0 && mp_device_button_pressed(&button) == 0, "active high release");

	/* decoded edge directly */
	mp_device_button_edge(&button, 1);
	expect(changes == 3 && mp_device_button_pressed(&button) == 1, "edge press");
	mp_device_button_edge(&button, 1);
	expect(changes == 3, "edge same state does not refire");

	/* active low: pin level 0 means pressed */
	button.active = 0;
	button.pressed = 0;
	changes = 0;
	mp_device_button_level(&button, 0);
	expect(changes == 1 && lastDown == 1, "active low press");
	mp_device_button_level(&button, 1);
	expect(changes == 2 && lastDown == 0, "active low release");

	mp_device_subscribe_end(&a);
	expect(stops == 0 && driver.users == 1, "one subscriber left stays started");
	mp_device_subscribe_end(&b);
	expect(stops == 1 && driver.running == NO, "last subscriber stops");

	mp_device_fini(&kernel);
	expect(stops == 1, "fini leaves a stopped driver");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
