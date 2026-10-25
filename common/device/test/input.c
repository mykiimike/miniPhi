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

/* Host test of the input engine, now folded into device. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../input.c"
#include "../device.c"

static int failed;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

/* drain the pending events; return the mask of types seen */
static unsigned int drain(mp_device_input_t *input) {
	mp_device_input_event_t ev;
	unsigned int seen = 0;

	while(mp_device_input_read(input, &ev) == 1) {
		seen |= (1u << ev.type);
	}
	return(seen);
}

int main(void) {
	mp_kernel_t kernel;
	mp_device_input_t input;
	mp_device_input_t *walk;

	memset(&kernel, 0, sizeof(kernel));
	mp_list_init(&kernel.inputs);
	memset(&input, 0, sizeof(input));

	expect(mp_device_input_init(&kernel, &input, MP_DEVICE_INPUT_CAP_TOUCH, "panel") == 1, "init");
	expect(kernel.inputs.first != NULL, "listed on kernel");

	mp_device_input_bounds(&input, 10, 10);

	/* a tap in the corner: some motion then a down, then a click on release */
	mp_device_input_touch(&input, 5, 5, 1);
	expect((drain(&input) & (1u << MP_DEVICE_INPUT_DOWN)) != 0, "down during a press");

	mp_device_input_touch(&input, 5, 5, 0);
	expect((drain(&input) & (1u << MP_DEVICE_INPUT_CLICK)) != 0, "click on release");

	/* a drag past the threshold: a drag start, then a drop */
	mp_device_input_touch(&input, 1, 1, 1);
	drain(&input);
	mp_device_input_touch(&input, 9, 9, 1);
	expect((drain(&input) & (1u << MP_DEVICE_INPUT_DRAG_START)) != 0, "drag start");
	mp_device_input_touch(&input, 9, 9, 0);
	expect((drain(&input) & (1u << MP_DEVICE_INPUT_DROP)) != 0, "drop after drag");

	/* walk the inputs */
	walk = mp_device_input_next(&kernel, NULL, NULL);
	expect(walk == &input, "next finds the input");
	expect(mp_device_input_next(&kernel, walk, NULL) == NULL, "no second input");

	mp_device_input_fini(&input);
	expect(kernel.inputs.first == NULL, "unlisted on fini");

	{
		mp_device_driver_t driver;
		mp_device_registry_t touchDev;
		mp_device_subscribe_t sub;
		static const mp_device_driver_ops_t ops = {
			"resistive", "Resistive", "1.0.0",
			(unsigned int)(1u << MP_DEVICE_TOUCH),
			NULL, NULL, NULL, NULL, NULL
		};

		memset(&driver, 0, sizeof(driver));
		memset(&touchDev, 0, sizeof(touchDev));
		memset(&sub, 0, sizeof(sub));
		mp_device_init(&kernel);
		mp_list_init(&kernel.inputs);
		expect(mp_device_driver_register(&kernel, &driver, &ops, &touchDev, NULL) == TRUE, "driver");
		expect(mp_device_registry_register(&driver, &touchDev, MP_DEVICE_TOUCH, 0, &input, "resistive", "01", NULL) != NULL, "register");
		expect(mp_device_touch(&touchDev, &sub) == &input, "subscribe returns the input");
		mp_device_subscribe_end(&sub);
		mp_device_fini(&kernel);
	}

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
