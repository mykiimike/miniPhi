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

/* Host test of the pixel contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../measure.c"
#include "../pixel.c"
#include "../device.c"

static int failed;
static int starts;
static int stops;
static int commits;
static unsigned char frame[2][3];
static mp_device_pixel_t face;
static mp_device_pixel_channel_t lamps[2 * MP_DEVICE_PIXEL_CHANNELS];

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void _commit(mp_device_pixel_t *pixel) {
	int index;

	commits++;
	for(index = 0; index < 2; index++)
		mp_device_pixel_color(pixel, index, &frame[index][0], &frame[index][1], &frame[index][2]);
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
	memset(lamps, 0, sizeof(lamps));
	face.commit = _commit;
	face.channel = lamps;
	face.count = 2;
	face.user = &face;
	if(!mp_device_registry_register(driver, device, MP_DEVICE_PIXEL, 0, &face, "ws2812", "board", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t pixelOps = {
	"pixel", "Pixel", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_PIXEL),
	_load, NULL, NULL, _start, _stop
};

static int _color(int index, int red, int green, int blue) {
	return(frame[index][0] == red && frame[index][1] == green && frame[index][2] == blue);
}

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_registry_t dev;
	mp_device_subscribe_t a, b;
	mp_device_pixel_t *pixel;
	unsigned char red, green, blue;
	int sent;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(&dev, 0, sizeof(dev));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	mp_device_init(&kernel);

	expect(mp_device_measure_class(MP_DEVICE_PIXEL) == NO, "pixel is not a measure");
	expect(mp_device_pixel_channel_state(NULL, 0, MP_DEVICE_PIXEL_RED) == 0, "null state");
	expect(mp_device_pixel_color(NULL, 0, &red, &green, &blue) == FALSE, "null color");
	mp_device_pixel_set(NULL, 0, 1, 2, 3);
	mp_device_pixel_channel_set(NULL, 0, MP_DEVICE_PIXEL_RED, 1);
	mp_device_pixel_channel_turn(NULL, 0, MP_DEVICE_PIXEL_RED);

	expect(mp_device_driver_register(&kernel, &driver, &pixelOps, &dev, NULL) == TRUE, "load");
	expect(strcmp(dev.address, "pixel0.ws2812.board") == 0, "address");
	pixel = mp_device_pixel(&dev, &a);
	expect(pixel == &face, "subscribe returns the pixel");
	expect(starts == 1 && driver.users == 1, "first subscriber starts");
	expect(mp_device_pixel(&dev, &a) == &face, "same subscriber");
	expect(driver.users == 1, "same subscriber counts once");
	expect(mp_device_pixel(&dev, &b) == NULL, "pixel is exclusive");
	expect(driver.users == 1, "refused does not count");

	mp_device_pixel_channel_set(pixel, 0, MP_DEVICE_PIXEL_RED, 1);
	expect(commits == 1 && _color(0, 255, 0, 0) && _color(1, 0, 0, 0), "red on leaves the others off");
	expect(mp_device_pixel_channel_state(pixel, 0, MP_DEVICE_PIXEL_RED) == 1, "red state");
	expect(mp_device_pixel_channel_state(pixel, 0, MP_DEVICE_PIXEL_GREEN) == 0, "green stays off");
	expect(mp_device_pixel_channel_state(pixel, 0, MP_DEVICE_PIXEL_BLUE) == 0, "blue stays off");

	mp_device_pixel_channel_turn(pixel, 0, MP_DEVICE_PIXEL_RED);
	expect(commits == 2 && _color(0, 0, 0, 0), "turn red off");
	expect(mp_device_pixel_channel_state(pixel, 0, MP_DEVICE_PIXEL_RED) == 0, "red is off");
	mp_device_pixel_channel_turn(pixel, 0, MP_DEVICE_PIXEL_RED);
	expect(commits == 3 && _color(0, 255, 0, 0), "turn red restores 255");

	mp_device_pixel_set(pixel, 0, 128, 0, 40);
	expect(commits == 4 && _color(0, 128, 0, 40), "color writes the three bytes");
	expect(lamps[MP_DEVICE_PIXEL_RED].level == 128, "red remembers 128");
	mp_device_pixel_channel_turn(pixel, 0, MP_DEVICE_PIXEL_GREEN);
	expect(commits == 5 && _color(0, 128, 255, 40), "green on leaves red and blue");
	mp_device_pixel_channel_set(pixel, 0, MP_DEVICE_PIXEL_RED, 0);
	expect(commits == 6 && _color(0, 0, 255, 40), "red off leaves green");
	expect(lamps[MP_DEVICE_PIXEL_RED].level == 128, "red keeps its shade");
	mp_device_pixel_channel_turn(pixel, 0, MP_DEVICE_PIXEL_RED);
	expect(commits == 7 && _color(0, 128, 255, 40), "red comes back at 128");

	mp_device_pixel_set(pixel, 1, 1, 2, 3);
	expect(commits == 8 && _color(0, 128, 255, 40) && _color(1, 1, 2, 3), "the other pixel is its own frame");

	sent = commits;
	mp_device_pixel_set(pixel, 2, 9, 9, 9);
	mp_device_pixel_channel_set(pixel, 0, 3, 1);
	mp_device_pixel_channel_turn(pixel, -1, MP_DEVICE_PIXEL_BLUE);
	expect(commits == sent && _color(0, 128, 255, 40), "a bad index sends nothing");

	face.commit = NULL;
	mp_device_pixel_channel_set(pixel, 0, MP_DEVICE_PIXEL_BLUE, 0);
	expect(commits == sent, "no commit callback sends nothing");
	expect(mp_device_pixel_color(pixel, 0, &red, &green, &blue) == TRUE, "color");
	expect(red == 128 && green == 255 && blue == 0, "the byte still changes");
	face.commit = _commit;

	mp_device_pixel_set(pixel, 0, 300, -4, 255);
	expect(_color(0, 255, 0, 255), "a component is clamped");
	expect(lamps[MP_DEVICE_PIXEL_GREEN].level == 255, "green keeps the shade it had");
	mp_device_pixel_channel_turn(pixel, 0, MP_DEVICE_PIXEL_GREEN);
	expect(_color(0, 255, 255, 255), "green comes back full");

	mp_device_pixel_level(pixel, 0, 16);
	expect(_color(0, 16, 16, 16) && _color(1, 1, 2, 3), "level dims the lit pixel");
	expect(lamps[MP_DEVICE_PIXEL_BLUE].level == 16, "blue remembers 16");
	mp_device_pixel_channel_set(pixel, 0, MP_DEVICE_PIXEL_GREEN, 0);
	sent = commits;
	mp_device_pixel_channel_level(pixel, 0, MP_DEVICE_PIXEL_GREEN, 40);
	expect(commits == sent && _color(0, 16, 0, 16), "a dark channel keeps the new shade off the frame");
	expect(lamps[MP_DEVICE_PIXEL_GREEN].level == 40, "green shade");
	mp_device_pixel_channel_turn(pixel, 0, MP_DEVICE_PIXEL_GREEN);
	expect(_color(0, 16, 40, 16), "green comes back at 40");
	sent = commits;
	mp_device_pixel_level(pixel, 0, 0);
	mp_device_pixel_level(NULL, 0, 8);
	mp_device_pixel_level(pixel, 3, 8);
	mp_device_pixel_channel_level(pixel, 0, 9, 8);
	expect(commits == sent && _color(0, 16, 40, 16), "a missing level sends nothing");
	mp_device_pixel_channel_level(pixel, 0, MP_DEVICE_PIXEL_RED, 300);
	expect(_color(0, 255, 40, 16), "a channel level clamps");

	mp_device_subscribe_end(&a);
	expect(stops == 1 && driver.users == 0 && driver.running == NO, "last subscriber stops");
	expect(mp_device_pixel(&dev, &a) == &face, "resubscribe");
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
