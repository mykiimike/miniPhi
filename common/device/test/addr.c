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

/* Host test of the device address. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"

static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void expect_str(const char *got, const char *want, const char *msg) {
	if(got && want && strcmp(got, want) == 0)
		return;
	failed++;
	fprintf(stderr, "fail: %s (got '%s')\n", msg, got ? got : "(null)");
}

int main(void) {
	char dst[MP_DEVICE_ADDRESS];
	char type[32];
	char model[MP_DEVICE_ADDRESS];
	char serial[MP_DEVICE_ADDRESS];
	int num;
	unsigned char utf8[] = { 'c', 'a', 'f', 0xc3, 0xa9, 0 };

	expect(mp_device_address(dst, sizeof dst, "video", 0, "500-0771-01", "A1B2C3D4E5F60718") == TRUE, "lepton build");
	expect_str(dst, "video0.500-0771-01.a1b2c3d4e5f60718", "lepton text");

	expect(mp_device_address(dst, sizeof dst, "CPU", 0, "STM32F407ZG", "001E003235315709") == TRUE, "cpu build");
	expect_str(dst, "cpu0.stm32f407zg.001e003235315709", "cpu text");

	expect(mp_device_address(dst, sizeof dst, "button", 0, "BUTTON", "Left") == TRUE, "button build");
	expect_str(dst, "button0.button.left", "button text");

	expect(mp_device_address(dst, sizeof dst, "temp", 0, "foo.bar", "01") == TRUE, "dot build");
	expect_str(dst, "temp0.foo-bar.01", "dot text");

	expect(mp_device_address(dst, sizeof dst, "temp", 1, "foo bar", "01") == TRUE, "space build");
	expect_str(dst, "temp1.foo-bar.01", "space text");

	expect(mp_device_address(dst, sizeof dst, "accel", 10, "02", "1A") == TRUE, "index build");
	expect_str(dst, "accel10.02.1a", "index text");

	expect(mp_device_address(dst, sizeof dst, "video", 0, (char *)utf8, "01") == FALSE, "utf8");
	expect_str(dst, "", "utf8 cleared");
	expect(mp_device_address(dst, sizeof dst, "video", 0, "", "01") == FALSE, "empty model");
	expect(mp_device_address(dst, sizeof dst, "video", 0, "m", "") == FALSE, "empty serial");
	expect(mp_device_address(dst, sizeof dst, "video", -1, "m", "s") == FALSE, "negative");
	expect(mp_device_address(dst, sizeof dst, "vid eo", 0, "m", "s") == FALSE, "stem space");
	expect(mp_device_address(dst, sizeof dst, "video.cam", 0, "m", "s") == FALSE, "stem dot");
	expect(mp_device_address(dst, 8, "video", 0, "model", "serial") == FALSE, "short cap");
	expect_str(dst, "", "short cleared");

	expect(mp_device_address_split("VIDEO0.FOO BAR.AB", type, sizeof type, &num, model, sizeof model, serial, sizeof serial) == TRUE, "split");
	expect_str(type, "video", "split type");
	expect(num == 0, "split num");
	expect_str(model, "foo-bar", "split model");
	expect_str(serial, "ab", "split serial");

	expect(mp_device_address_split("accel10.02.1a", type, sizeof type, &num, model, sizeof model, serial, sizeof serial) == TRUE, "split 10");
	expect(num == 10, "num 10");
	expect(mp_device_address_split("video00.a.b", type, sizeof type, &num, model, sizeof model, serial, sizeof serial) == FALSE, "leading zero");
	expect(num == -1, "num cleared");
	expect(mp_device_address_split("video0.foo.bar.01", type, sizeof type, &num, model, sizeof model, serial, sizeof serial) == FALSE, "four fields");
	expect(mp_device_address_split("video.a.b", type, sizeof type, &num, model, sizeof model, serial, sizeof serial) == FALSE, "no index");

	expect(mp_device_address(dst, sizeof dst, "temp", 0, "Foo.Bar", "01") == TRUE, "round build");
	expect(mp_device_address_canon(model, sizeof model, dst) == TRUE, "canon");
	expect_str(model, dst, "round trip");
	expect(mp_device_address_canon(dst, sizeof dst, "CPU0.STM32F407ZG.001E") == TRUE, "canon cpu");
	expect_str(dst, "cpu0.stm32f407zg.001e", "canon cpu text");

	expect(mp_device_address_field(dst, sizeof dst, "A B.C") == TRUE, "field");
	expect_str(dst, "a-b-c", "field text");
	expect(mp_device_address_field(dst, sizeof dst, "caf\x7f") == FALSE, "del");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
