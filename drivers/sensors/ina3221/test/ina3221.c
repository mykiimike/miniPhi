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

/* Host test of the INA3221 register map and LSB. Not linked into the firmware. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct mp_gpio_port_s mp_gpio_port_t;

#define MP_HOST_TASK
#define SUPPORT_DRV_INA3221

#include <mp.h>
#include "../ina3221.h"

static int failed;
static unsigned char rd_reg[8];
static int nread;
static unsigned char wr_reg;
static unsigned char wr[2];
static int nwrite;
static unsigned char rd_buf[2];

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

char *mp_options_get(mp_options_t *options, char *key) {
	(void)options;
	(void)key;
	return(NULL);
}

mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_bus_t *bus, mp_options_t *options, char *who) {
	(void)kernel;
	(void)bus;
	(void)options;
	(void)who;
	return(TRUE);
}

void mp_i2c_close(mp_i2c_bus_t *bus) {
	(void)bus;
}

int mp_i2c_write_reg(mp_i2c_bus_t *bus, unsigned char address, unsigned char reg, const unsigned char *data, int len) {
	(void)bus;
	(void)address;
	wr_reg = reg;
	if(len >= 1)
		wr[0] = data[0];
	if(len >= 2)
		wr[1] = data[1];
	nwrite++;
	return(1);
}

int mp_i2c_read_reg(mp_i2c_bus_t *bus, unsigned char address, unsigned char reg, unsigned char *buf, int len) {
	(void)bus;
	(void)address;
	if(nread < (int)sizeof(rd_reg))
		rd_reg[nread++] = reg;
	if(len >= 2) {
		buf[0] = rd_buf[0];
		buf[1] = rd_buf[1];
	}
	return(1);
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

void mp_task_delay(mp_task_t *task, unsigned long delay) {
	(void)task;
	(void)delay;
}

#include "../ina3221.c"

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "ina3221: %s\n", msg);
}

static int near(float got, float exp, float slop) {
	float d = got - exp;

	if(d < 0)
		d = -d;
	return(d <= slop);
}

int main(void) {
	mp_drv_ina3221_t meter;
	mp_task_t task;
	unsigned short raw, cfg;
	float shunt, bus;

	expect(_ina3221_address(NULL) == 0x40, "addr default");
	expect(_ina3221_address("0x41") == 0x41, "addr 0x41");
	expect(_ina3221_address("42") == 0x42, "addr 42");
	expect(_ina3221_address("0x43") == 0x43, "addr 0x43");
	expect(_ina3221_address("0x44") == 0x40, "addr range");
	expect(_ina3221_address("zz") == 0x40, "addr junk");

	expect(_ina3221_channels(NULL) == 0x07, "ch all");
	expect(_ina3221_channels("") == 0x07, "ch empty");
	expect(_ina3221_channels("13") == 0x05, "ch 1 and 3");
	expect(_ina3221_channels("2") == 0x02, "ch 2");
	expect(_ina3221_channels("9") == 0x07, "ch none");

	expect(_ina3221_shunt(NULL) == 50, "shunt default");
	expect(_ina3221_shunt("0") == 50, "shunt 0");
	expect(_ina3221_shunt("100") == 100, "shunt 100");
	expect(_ina3221_shunt("10001") == 50, "shunt huge");
	expect(_ina3221_shunt("12x") == 50, "shunt junk");

	/* bits 15:3, 40 µV. 1000 counts is 40 mV. 0xFFF8 is -1 count. */
	shunt = _ina3221_shunt_v((unsigned short)(1000u << 3));
	expect(near(shunt, 0.04f, 0.00004f), "shunt 40 mV");
	shunt = _ina3221_shunt_v(0xFFF8);
	expect(near(shunt, -0.00004f, 0.000001f), "shunt -1");
	/* bus bits 15:3, 8 mV. 2500 counts is 20 V. */
	bus = _ina3221_bus_v((unsigned short)(2500u << 3));
	expect(near(bus, 20.0f, 0.01f), "bus 20 V");

	memset(&meter, 0, sizeof(meter));
	meter.mask = 0x07;
	/* AVG=16 is 010, 1.1 ms is 100, continuous shunt+bus is 111. */
	cfg = _ina3221_config(&meter, 1);
	expect(cfg == (unsigned short)((1u << 14) | (1u << 13) | (1u << 12) | (2u << 9) | (4u << 6) | (4u << 3) | 7u), "cfg run");
	meter.mask = 0x01;
	cfg = _ina3221_config(&meter, 0);
	expect(cfg == (unsigned short)((1u << 14) | (2u << 9) | (4u << 6) | (4u << 3)), "cfg down");

	rd_buf[0] = 0x12;
	rd_buf[1] = 0x34;
	expect(_ina3221_read(&meter, 0x01, &raw) == 1, "read");
	expect(raw == 0x1234, "big endian");

	meter.mask = 0x07;
	meter.count = 3;
	meter.hw[0] = 0;
	meter.hw[1] = 1;
	meter.hw[2] = 2;
	meter.ampsPerVolt = 1000.0f / 50.0f;
	nread = 0;
	_ina3221_sample(&meter);
	expect(nread == 6, "six regs");
	expect(rd_reg[0] == 0x01 && rd_reg[1] == 0x02, "ch1");
	expect(rd_reg[2] == 0x03 && rd_reg[3] == 0x04, "ch2");
	expect(rd_reg[4] == 0x05 && rd_reg[5] == 0x06, "ch3");

	memset(&task, 0, sizeof(task));
	task.user = &meter;
	task.signal = MP_TASK_SIG_OK;
	meter.watched = YES;
	meter.running = NO;
	meter.mask = 0x07;
	nwrite = 0;
	_ina3221_task(&task);
	expect(nwrite == 1 && wr_reg == 0x00, "config reg");
	expect(wr[0] == 0x75 && wr[1] == 0x27, "config bytes");
	expect(meter.running == YES, "running");
	(void)_ina3221_serial;
	return(failed ? 1 : 0);
}
