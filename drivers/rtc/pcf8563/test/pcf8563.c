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

/* Host test of the PCF8563 BCD time. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

typedef struct mp_gpio_port_s mp_gpio_port_t;

#define MP_HOST_TASK
#define SUPPORT_COMMON_DEVICE
#define SUPPORT_DRV_RTC_PCF8563

#include <mp.h>
#include "../pcf8563.h"

static int failed;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

char *mp_options_get(mp_options_t *options, char *key) {
	(void)options;
	(void)key;
	return(NULL);
}

int mp_i2c_read_reg(mp_i2c_bus_t *bus, unsigned char address, unsigned char reg, unsigned char *buf, int len) {
	(void)bus;
	(void)address;
	(void)reg;
	(void)buf;
	(void)len;
	return(0);
}

void mp_device_clock_publish(mp_device_time_t *clock, const mp_device_time_t *time) {
	(void)clock;
	(void)time;
}

mp_ret_t mp_device_driver_register(mp_kernel_t *kernel, mp_device_driver_t *driver, const mp_device_driver_ops_t *ops, void *self, mp_options_t *options) {
	(void)kernel;
	(void)driver;
	(void)ops;
	(void)self;
	(void)options;
	return(TRUE);
}

mp_ret_t mp_device_driver_unregister(mp_kernel_t *kernel, mp_device_driver_t *driver) {
	(void)kernel;
	(void)driver;
	return(TRUE);
}

mp_device_registry_t *mp_device_registry_register(mp_device_driver_t *driver, mp_device_registry_t *device, mp_device_class_t class, int channel, void *instance, const char *model, const char *serial, const char *version) {
	(void)driver;
	(void)class;
	(void)channel;
	(void)instance;
	(void)model;
	(void)serial;
	(void)version;
	return(device);
}

void mp_device_registry_unregister(mp_device_registry_t *device) {
	(void)device;
}

void mp_task_delay(mp_task_t *task, unsigned long delay) {
	(void)task;
	(void)delay;
}

mp_task_t *mp_task_create(mp_task_handler_t *hdl, char *name, mp_task_wakeup_t wakeup, void *user, unsigned long delay) {
	(void)hdl;
	(void)name;
	(void)wakeup;
	(void)user;
	(void)delay;
	return(NULL);
}

mp_ret_t mp_task_destroy(mp_task_t *task) {
	(void)task;
	return(TRUE);
}

#include "../pcf8563.c"

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "pcf8563: %s\n", msg);
}

int main(void) {
	mp_device_time_t time;
	/* 21 Nov 2026 13:37:45, weekday 6, VL clear. BCD, century bit clear. */
	unsigned char regs[7] = { 0x45, 0x37, 0x13, 0x21, 0x06, 0x11, 0x26 };

	expect(_pcf8563_address(NULL) == 0x51, "addr default");
	expect(_pcf8563_address("0x51") == 0x51, "addr 51");
	expect(_pcf8563_address("0") == 0x51, "addr 0");
	expect(_pcf8563_address("80") == 0x51, "addr wide");
	expect(_pcf8563_from_bcd(0x45) == 45, "bcd 45");
	expect(_pcf8563_from_bcd(0x0A) < 0, "bcd nibble");

	memset(&time, 0, sizeof(time));
	expect(_pcf8563_decode(regs, &time) == 1, "decode");
	expect(time.year == 2026 && time.month == 11 && time.day == 21, "date");
	expect(time.hour == 13 && time.minute == 37 && time.second == 45, "clock");
	expect(time.wday == 6 && time.running == YES, "running");

	regs[0] = 0xC5;
	expect(_pcf8563_decode(regs, &time) == 1, "VL decode");
	expect(time.second == 45 && time.running == NO, "VL");

	regs[0] = 0x45;
	regs[5] = 0x91;
	expect(_pcf8563_decode(regs, &time) == 1, "century");
	expect(time.month == 11 && time.year == 2026, "century ignored");

	regs[1] = 0x0A;
	expect(_pcf8563_decode(regs, &time) == 0, "bad minute");
	regs[1] = 0x37;
	regs[2] = 0x24;
	expect(_pcf8563_decode(regs, &time) == 0, "hour 24");
	regs[2] = 0x13;
	regs[3] = 0x00;
	expect(_pcf8563_decode(regs, &time) == 0, "day 0");
	return(failed ? 1 : 0);
}
