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

/* Host test of the XPT2046 conversion. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

typedef struct mp_gpio_port_s mp_gpio_port_t;

struct mp_gpio_port_s {
	int level;
	int port;
	int pin;
};

typedef enum {
	MP_GPIO_INPUT = 1,
	MP_GPIO_OUTPUT = 2,
} mp_gpio_direction_t;

typedef enum {
	MP_GPIO_MODE_INPUT = 1,
	MP_GPIO_MODE_OUTPUT = 3,
} mp_gpio_mode_t;

#define MP_HOST_TASK
#define SUPPORT_COMMON_DEVICE
#define SUPPORT_DRV_TOUCH_XPT2046

#include <mp.h>
#include "../xpt2046.h"

static int failed;
static mp_gpio_port_t pin_cs;
static unsigned char tx[64];
static int cs_at[64];
static int ntx;
static unsigned char reply[32];
static int nreply;
static int ireply;
static int touch_n, touch_x, touch_y, touch_down;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

const char *mp_machine_serial(void) {
	return("host");
}

char *mp_options_get(mp_options_t *options, char *key) {
	(void)options;
	(void)key;
	return(NULL);
}

mp_ret_t mp_options_cmp(char *a, char *b) {
	if(!a || !b)
		return(FALSE);
	return(strcmp(a, b) == 0 ? TRUE : FALSE);
}

mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who) {
	(void)text;
	(void)who;
	return(NULL);
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	(void)port;
	return(TRUE);
}

void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode) {
	(void)port;
	(void)mode;
}

void mp_gpio_set(mp_gpio_port_t *port) {
	if(port)
		port->level = 1;
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	if(port)
		port->level = 0;
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	(void)port;
	return(NO);
}

mp_bool_t mp_gpio_interrupt_set(mp_gpio_port_t *pin, void (*cb)(void *), void *user, char *name) {
	(void)pin;
	(void)cb;
	(void)user;
	(void)name;
	return(YES);
}

void mp_gpio_interrupt_disable(mp_gpio_port_t *pin) { (void)pin; }
void mp_gpio_interrupt_enable(mp_gpio_port_t *pin) { (void)pin; }
void mp_gpio_interrupt_hi2lo(mp_gpio_port_t *pin) { (void)pin; }
void mp_gpio_interrupt_unset(mp_gpio_port_t *pin) { (void)pin; }

unsigned char mp_spi_xfer(mp_spi_bus_t *bus, unsigned char byte) {
	(void)bus;
	if(ntx < (int)sizeof(tx)) {
		tx[ntx] = byte;
		cs_at[ntx] = pin_cs.level;
		ntx++;
	}
	if(byte == 0 && ireply < nreply)
		return(reply[ireply++]);
	return(0);
}

mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_bus_t *bus, mp_options_t *options, char *who) {
	(void)kernel;
	(void)bus;
	(void)options;
	(void)who;
	return(TRUE);
}

void mp_spi_close(mp_spi_bus_t *bus) {
	(void)bus;
}

void mp_device_input_touch(mp_device_input_t *input, int x, int y, int down) {
	input->x = x;
	input->y = y;
	touch_x = x;
	touch_y = y;
	touch_down = down;
	touch_n++;
}

int mp_device_input_init(mp_kernel_t *kernel, mp_device_input_t *input, int caps, const char *who) {
	(void)kernel;
	(void)input;
	(void)caps;
	(void)who;
	return(1);
}

void mp_device_input_bounds(mp_device_input_t *input, int width, int height) {
	(void)input;
	(void)width;
	(void)height;
}

void mp_device_input_fini(mp_device_input_t *input) {
	(void)input;
}

void mp_device_identify(void *self, const char *label, const char *model, const char *serial, const char *version) {
	(void)self;
	(void)label;
	(void)model;
	(void)serial;
	(void)version;
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
	(void)device;
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

void mp_task_wake(mp_task_t *task) {
	(void)task;
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

#include "../xpt2046.c"

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "xpt2046: %s\n", msg);
}

/* 12-bit result sits in bits 14:3 of the 16 clocks that follow the command. */
static void put12(int value) {
	unsigned int raw = ((unsigned int)value << 3) & 0xFFF8u;

	reply[nreply++] = (unsigned char)(raw >> 8);
	reply[nreply++] = (unsigned char)raw;
}

static void script_sample(int z, int x, int y) {
	nreply = 0;
	ireply = 0;
	ntx = 0;
	put12(z);
	put12(x);
	put12(x);
	put12(y);
	put12(y);
}

int main(void) {
	mp_drv_xpt2046_t touch;
	mp_task_t task;
	int x, y;

	/* span-1 divides the window: 240 counts across 2400 raw steps. */
	expect(_xpt_scale(200, 200, 2600, 241) == 0, "scale lo");
	expect(_xpt_scale(2600, 200, 2600, 241) == 240, "scale hi");
	expect(_xpt_scale(1400, 200, 2600, 241) == 120, "scale mid");
	expect(_xpt_scale(0, 200, 2600, 241) == 0, "scale clamp");
	expect(_xpt_scale(1400, 2600, 200, 241) == 240 - 120, "scale mirror");
	expect(_xpt_scale(10, 5, 5, 10) == 0, "scale empty");

	memset(&touch, 0, sizeof(touch));
	touch.columns = 241;
	touch.rows = 321;
	touch.orientation = _XPT_LANDSCAPE;
	_xpt_map(&touch, 120, 160, &x, &y);
	expect(x == 160 && y == 241 - 1 - 120, "landscape map");
	touch.orientation = _XPT_PORTRAIT;
	_xpt_map(&touch, 120, 160, &x, &y);
	expect(x == 120 && y == 160, "portrait map");
	touch.orientation = _XPT_PORTRAIT_FLIPPED;
	_xpt_map(&touch, 120, 160, &x, &y);
	expect(x == 241 - 1 - 120 && y == 321 - 1 - 160, "portrait flip");
	touch.orientation = _XPT_LANDSCAPE_FLIPPED;
	_xpt_map(&touch, 120, 160, &x, &y);
	expect(x == 321 - 1 - 160 && y == 120, "landscape flip");

	touch.cs = &pin_cs;
	pin_cs.level = 1;
	ntx = 0;
	nreply = 0;
	put12(1400);
	expect(_xpt_command(&touch, 0xD0) == 1400, "decode 12");
	/* start, A=101, 12-bit, differential, power down: 0xD0 */
	expect(ntx == 3 && tx[0] == 0xD0 && tx[1] == 0 && tx[2] == 0, "X clocks");
	expect(cs_at[0] == 0 && cs_at[2] == 0 && pin_cs.level == 1, "CS");

	ntx = 0;
	nreply = 0;
	ireply = 0;
	put12(1000);
	put12(1000 + 400);
	expect(_xpt_pair(&touch, 0x90) < 0, "spread");
	expect(tx[0] == 0x90 && tx[3] == 0x90, "Y twice");

	memset(&touch, 0, sizeof(touch));
	touch.cs = &pin_cs;
	touch.columns = 241;
	touch.rows = 321;
	touch.xMin = 200;
	touch.xMax = 2600;
	touch.yMin = 100;
	touch.yMax = 3300;
	touch.zMin = 100;
	touch.orientation = _XPT_LANDSCAPE;
	memset(&task, 0, sizeof(task));
	task.user = &touch;
	task.signal = MP_TASK_SIG_OK;
	script_sample(500, 1400, 1700);
	_xpt_task(&task);
	expect(touch_n == 0 && touch.seen == 1, "first sample waits");
	expect(tx[0] == 0xB0, "Z1 first");
	script_sample(500, 1400, 1700);
	_xpt_task(&task);
	expect(touch_n == 1 && touch_down == 1, "press");
	expect(touch_x == 160 && touch_y == 120, "press at");

	touch.down = 0;
	touch.seen = 0;
	touch.tracked = 0;
	touch_n = 0;
	script_sample(500, 0, 1700);
	_xpt_task(&task);
	script_sample(500, 0, 1700);
	_xpt_task(&task);
	expect(touch_n == 0, "outside");

	touch.seen = 1;
	touch.px = 0;
	touch.py = 0;
	script_sample(50, 1400, 1700);
	_xpt_task(&task);
	expect(touch_n == 0, "z below");
	return(failed ? 1 : 0);
}
