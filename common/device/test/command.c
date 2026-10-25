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

/* Host test of registered-device commands. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../command.c"
#include "../device.c"

static int failed;
static int calls;
static int gone;
static int serialTwo;
static char inst;
static mp_device_registry_t dev;
static mp_device_value_t got[6];
static int have[6];

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static mp_ret_t _store(int slot, const mp_device_value_t *value) {
	if(!value || slot < 0 || slot >= 6)
		return(FALSE);
	got[slot] = *value;
	have[slot] = 1;
	calls++;
	return(TRUE);
}

static mp_ret_t _get_slot(int slot, mp_device_value_t *value) {
	if(!value || slot < 0 || slot >= 6 || !have[slot])
		return(FALSE);
	*value = got[slot];
	return(TRUE);
}

static mp_ret_t _set_ffc(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)device;
	return(_store(0, value));
}

static mp_ret_t _set_enable(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)device;
	return(_store(1, value));
}

static mp_ret_t _get_enable(mp_device_registry_t *device, mp_device_value_t *value) {
	(void)device;
	return(_get_slot(1, value));
}

static mp_ret_t _set_gain(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)device;
	return(_store(2, value));
}

static mp_ret_t _get_gain(mp_device_registry_t *device, mp_device_value_t *value) {
	(void)device;
	return(_get_slot(2, value));
}

static mp_ret_t _set_offset(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)device;
	return(_store(3, value));
}

static mp_ret_t _get_offset(mp_device_registry_t *device, mp_device_value_t *value) {
	(void)device;
	return(_get_slot(3, value));
}

static mp_ret_t _set_raw(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)device;
	return(_store(4, value));
}

static mp_ret_t _get_raw(mp_device_registry_t *device, mp_device_value_t *value) {
	(void)device;
	return(_get_slot(4, value));
}

static mp_ret_t _set_mode(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)device;
	return(_store(5, value));
}

static mp_ret_t _get_mode(mp_device_registry_t *device, mp_device_value_t *value) {
	(void)device;
	return(_get_slot(5, value));
}

static mp_ret_t _set_deny(mp_device_registry_t *device, const mp_device_value_t *value) {
	(void)device;
	(void)value;
	calls++;
	return(FALSE);
}

static const char *const _mode_choice[] = { "white-hot", "black-hot", NULL };

static const mp_device_command_t commands[] = {
	{ "ffc", "FFC", MP_DEVICE_ARG_NONE, 0, 0, NULL, NULL, _set_ffc, NULL },
	{ "enable", "Enable", MP_DEVICE_ARG_BOOL, 0, 0, NULL, NULL, _set_enable, _get_enable },
	{ "gain", "Gain", MP_DEVICE_ARG_INT, 0, 10, NULL, NULL, _set_gain, _get_gain },
	{ "offset", "Offset", MP_DEVICE_ARG_FLOAT, -1, 2, NULL, NULL, _set_offset, _get_offset },
	{ "raw", "Raw", MP_DEVICE_ARG_INT, 0, -1, NULL, NULL, _set_raw, _get_raw },
	{ "mode", "Mode", MP_DEVICE_ARG_SELECT, 0, 0, _mode_choice, NULL, _set_mode, _get_mode },
	{ "dead", "Dead", MP_DEVICE_ARG_NONE, 0, 0, NULL, NULL, NULL, NULL },
	{ "deny", "Deny", MP_DEVICE_ARG_NONE, 0, 0, NULL, NULL, _set_deny, NULL },
	{ NULL, NULL, MP_DEVICE_ARG_NONE, 0, 0, NULL, NULL, NULL, NULL }
};

static mp_ret_t _load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	mp_device_registry_t *device = self;

	(void)kernel;
	(void)options;
	if(!mp_device_registry_register(driver, device, MP_DEVICE_LED, 0, &inst, "led", "one", NULL))
		return(FALSE);
	return(TRUE);
}

static void _scan(mp_device_driver_t *driver, void *self) {
	mp_device_registry_t *device = self;

	mp_device_registry_register(driver, device, MP_DEVICE_LED, 0, &inst, "led", serialTwo ? "two" : "one", NULL);
}

static const mp_device_driver_ops_t ops = {
	"led", "LED", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_LED),
	_load, NULL, _scan, NULL, NULL
};

static void _gone(mp_device_registry_t *device, void *user) {
	(void)user;
	gone++;
	expect(device->removed == YES, "removal has begun");
	expect(mp_device_command(device, "ffc", NULL) == FALSE, "removed refuses a command");
}

static void _roundtrip(const char *name, const char *text, const char *back) {
	const mp_device_command_t *command;
	mp_device_value_t value;
	char buf[32];
	int n;

	expect(mp_device_command(&dev, name, text) == TRUE, name);
	expect(mp_device_command_get(&dev, name, &value) == TRUE, name);
	command = mp_device_command_find(&dev, name);
	n = mp_device_command_text(command, &value, buf, (int)sizeof(buf));
	expect(n == (int)strlen(back) && strcmp(buf, back) == 0, name);
}

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_subscribe_t sub;
	mp_device_value_t value;
	const mp_device_command_t *command;
	char tiny[1];
	int before;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(&dev, 0, sizeof(dev));
	memset(&sub, 0, sizeof(sub));
	mp_device_init(&kernel);

	expect(mp_device_command_count(NULL) == 0, "null count");
	expect(mp_device_command_at(NULL, 0) == NULL, "null at");
	expect(mp_device_command_find(NULL, "ffc") == NULL, "null find");
	expect(mp_device_command(NULL, "ffc", NULL) == FALSE, "null command");
	expect(mp_device_command_get(NULL, "ffc", &value) == FALSE, "null get");
	expect(mp_device_command_text(NULL, &value, tiny, 1) == 0, "null text");

	expect(mp_device_driver_register(&kernel, &driver, &ops, &dev, NULL) == TRUE, "load");
	expect(mp_device_command_count(&dev) == 0, "no table yet");
	dev.commands = commands;
	expect(mp_device_command_count(&dev) == 8, "eight commands");
	expect(mp_device_command_at(&dev, 0) == &commands[0], "first");
	expect(mp_device_command_at(&dev, 8) == NULL, "past the end");
	expect(mp_device_command_at(&dev, -1) == NULL, "negative index");
	command = mp_device_command_find(&dev, "GAIN");
	expect(command == &commands[2], "find ignores case");
	expect(mp_device_command_find(&dev, "missing") == NULL, "unknown find");

	before = calls;
	expect(mp_device_command(&dev, "ffc", NULL) == TRUE, "none null");
	expect(mp_device_command(&dev, "ffc", "") == TRUE, "none empty");
	expect(calls == before + 2 && got[0].arg == MP_DEVICE_ARG_NONE, "none stored");
	before = calls;
	expect(mp_device_command(&dev, "ffc", "1") == FALSE, "none refuses a value");
	expect(calls == before, "refused none does not call set");
	expect(mp_device_command_get(&dev, "ffc", &value) == FALSE, "none has no get");

	expect(mp_device_command_get(&dev, "enable", &value) == FALSE, "get before set");
	expect(mp_device_command(&dev, "enable", "YeS") == TRUE, "bool yes");
	expect(got[1].as.on == 1, "yes is on");
	expect(mp_device_command(&dev, "ENABLE", "off") == TRUE, "bool off");
	expect(got[1].as.on == 0, "off");
	expect(mp_device_command(&dev, "enable", "TRUE") == TRUE, "bool true");
	expect(mp_device_command(&dev, "enable", "false") == TRUE, "bool false");
	expect(mp_device_command(&dev, "enable", "on") == TRUE, "bool on");
	expect(mp_device_command(&dev, "enable", "0") == TRUE, "bool 0");
	expect(mp_device_command(&dev, "enable", "1") == TRUE, "bool 1");
	before = calls;
	expect(mp_device_command(&dev, "enable", "maybe") == FALSE, "bool refuses maybe");
	expect(mp_device_command(&dev, "enable", "2") == FALSE, "bool refuses 2");
	expect(mp_device_command(&dev, "enable", "") == FALSE, "bool refuses empty");
	expect(calls == before, "bad bool does not call set");
	_roundtrip("enable", "1", "1");
	_roundtrip("enable", "0", "0");

	expect(mp_device_command(&dev, "gain", "0") == TRUE, "gain 0");
	expect(got[2].arg == MP_DEVICE_ARG_INT && got[2].as.integer == 0, "gain zero");
	expect(mp_device_command(&dev, "gain", "+10") == TRUE, "gain 10");
	expect(got[2].as.integer == 10, "gain ten");
	expect(mp_device_command(&dev, "gain", "11") == FALSE, "gain above max");
	expect(mp_device_command(&dev, "gain", "-1") == FALSE, "gain below min");
	expect(mp_device_command(&dev, "gain", "10.0") == FALSE, "gain refuses a fraction");
	expect(mp_device_command(&dev, "gain", " 4") == FALSE, "gain refuses space");
	_roundtrip("gain", "7", "7");

	expect(mp_device_command(&dev, "offset", "0.5") == TRUE, "offset 0.5");
	expect(got[3].arg == MP_DEVICE_ARG_FLOAT && got[3].as.number == 0.5f, "offset half");
	expect(mp_device_command(&dev, "offset", "-1") == TRUE, "offset -1");
	expect(got[3].as.number == -1.0f, "offset low");
	expect(mp_device_command(&dev, "offset", "1") == TRUE, "offset 1");
	expect(mp_device_command(&dev, "offset", "2") == TRUE, "offset 2");
	expect(mp_device_command(&dev, "offset", "2.5") == FALSE, "offset above max");
	expect(mp_device_command(&dev, "offset", "-1.01") == FALSE, "offset below min");
	expect(mp_device_command(&dev, "offset", "1.2.3") == FALSE, "offset two dots");
	expect(mp_device_command(&dev, "offset", "") == FALSE, "offset empty");
	_roundtrip("offset", "1.50", "1.5");
	_roundtrip("offset", "-0.25", "-0.25");
	_roundtrip("offset", "0.05", "0.05");

	expect(mp_device_command(&dev, "raw", "100000") == TRUE, "unbounded high");
	expect(got[4].as.integer == 100000L, "raw high");
	expect(mp_device_command(&dev, "raw", "-40") == TRUE, "unbounded low");
	expect(got[4].as.integer == -40L, "raw low");
	_roundtrip("raw", "-3", "-3");

	expect(mp_device_command(&dev, "mode", "white-hot") == TRUE, "select white");
	expect(got[5].arg == MP_DEVICE_ARG_SELECT && got[5].as.index == 0, "index 0");
	expect(mp_device_command(&dev, "mode", "BLACK-HOT") == TRUE, "select black");
	expect(got[5].as.index == 1, "index 1");
	before = calls;
	expect(mp_device_command(&dev, "mode", "warm") == FALSE, "select unknown");
	expect(mp_device_command(&dev, "mode", NULL) == FALSE, "select null");
	expect(calls == before, "bad select does not call set");
	_roundtrip("mode", "Black-Hot", "black-hot");

	before = calls;
	expect(mp_device_command(&dev, "nope", NULL) == FALSE, "unknown name");
	expect(mp_device_command(&dev, "dead", NULL) == FALSE, "no set");
	expect(calls == before, "unknown and dead do not call set");
	expect(mp_device_command(&dev, "deny", NULL) == FALSE, "set returns false");
	expect(calls == before + 1, "deny called set");

	command = mp_device_command_find(&dev, "mode");
	expect(mp_device_command_get(&dev, "mode", &value) == TRUE, "mode get");
	expect(mp_device_command_text(command, &value, tiny, 1) == (int)strlen("black-hot"), "full length");
	expect(tiny[0] == 0, "short buffer terminated");

	expect(mp_device_registry_register(&driver, &dev, MP_DEVICE_LED, 0, &inst, "led", "one", "1.2") == &dev, "refresh");
	expect(dev.commands == commands, "refresh keeps commands");
	expect(mp_device_command(&dev, "ffc", NULL) == TRUE, "command after refresh");

	serialTwo = 1;
	mp_device_registry_scan(&kernel, MP_DEVICE_LED);
	expect(dev.commands == commands, "scan keeps commands");
	expect(strcmp(dev.address, "led0.led.two") == 0, "scan replaced the serial");
	expect(mp_device_command_find(&dev, "Ffc") == &commands[0], "find after scan");

	sub.on_gone = _gone;
	expect(mp_device_subscribe(&dev, &sub) == &inst, "subscribe");
	mp_device_registry_unregister(&dev);
	expect(gone == 1, "unregister tells the subscriber");
	expect(mp_device_command(&dev, "ffc", NULL) == FALSE, "cleared device");

	mp_device_fini(&kernel);

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return 1;
	}
	printf("ok\n");
	return 0;
}
