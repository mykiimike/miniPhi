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

/* Host test of the PHY contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../measure.c"
#include "../phy.c"
#include "../device.c"

static int failed;
static int opens;
static int closes;
static int holds;
static int releases;
static int tunes;
static int links;
static int holdRefuse;
static mp_device_phy_t radio;
static mp_device_phy_t wire;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static mp_ret_t _open(mp_device_phy_t *phy) {
	(void)phy;
	opens++;
	return(TRUE);
}

static void _close(mp_device_phy_t *phy) {
	(void)phy;
	closes++;
}

static mp_bool_t _ready(mp_device_phy_t *phy) {
	(void)phy;
	return(YES);
}

static mp_ret_t _hold(mp_device_phy_t *phy) {
	(void)phy;
	holds++;
	if(holdRefuse)
		return(FALSE);
	return(TRUE);
}

static void _release(mp_device_phy_t *phy) {
	(void)phy;
	releases++;
}

static mp_ret_t _tune(mp_device_phy_t *phy, unsigned long hz, unsigned long width) {
	(void)phy;
	(void)hz;
	(void)width;
	tunes++;
	return(TRUE);
}

static mp_ret_t _link(mp_device_phy_t *phy, unsigned long bps, int duplex) {
	(void)phy;
	(void)bps;
	(void)duplex;
	links++;
	return(TRUE);
}

static void _fill(mp_device_phy_t *phy, mp_device_phy_kind_t kind) {
	memset(phy, 0, sizeof(*phy));
	phy->kind = kind;
	phy->open = _open;
	phy->close = _close;
	phy->ready = _ready;
	phy->hold = _hold;
	phy->release = _release;
	phy->user = phy;
	if(kind == MP_DEVICE_PHY_RADIO) {
		phy->tune = _tune;
		phy->hz_min = 2412000000UL;
		phy->hz_max = 2484000000UL;
		phy->width_min = 20000000UL;
		phy->width_max = 20000000UL;
	} else
		phy->link = _link;
}

static mp_ret_t _load(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	mp_device_registry_t *device = self;

	(void)kernel;
	(void)options;
	_fill(&radio, MP_DEVICE_PHY_RADIO);
	if(!mp_device_registry_register(driver, device, MP_DEVICE_PHY, 0, &radio, "radio", "one", NULL))
		return(FALSE);
	device->label = "radio";
	return(TRUE);
}

static const mp_device_driver_ops_t radioOps = {
	"phy/radio", "Radio", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_PHY),
	_load, NULL, NULL, NULL, NULL
};

static mp_ret_t _load_wire(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options) {
	mp_device_registry_t *device = self;

	(void)kernel;
	(void)options;
	_fill(&wire, MP_DEVICE_PHY_ETHERNET);
	if(!mp_device_registry_register(driver, device, MP_DEVICE_PHY, 0, &wire, "eth", "one", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t wireOps = {
	"phy/eth", "Ethernet", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_PHY),
	_load_wire, NULL, NULL, NULL, NULL
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t radioDriver;
	mp_device_driver_t wireDriver;
	mp_device_registry_t radioDev;
	mp_device_registry_t wireDev;
	mp_device_subscribe_t a, b, w;
	mp_device_phy_t *phy;
	mp_device_phy_t bare;

	memset(&kernel, 0, sizeof(kernel));
	memset(&radioDriver, 0, sizeof(radioDriver));
	memset(&wireDriver, 0, sizeof(wireDriver));
	memset(&radioDev, 0, sizeof(radioDev));
	memset(&wireDev, 0, sizeof(wireDev));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	memset(&w, 0, sizeof(w));
	mp_device_init(&kernel);

	expect(mp_device_driver_register(&kernel, &radioDriver, &radioOps, &radioDev, NULL) == TRUE, "load radio");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_PHY, 0) == &radioDev, "phy registered");
	expect(strcmp(radioDev.address, "phy0.radio.one") == 0, "phy address");
	expect(mp_device_measure_class(MP_DEVICE_PHY) == NO, "phy is not a measure");
	expect(strcmp(mp_device_measure_unit(MP_DEVICE_PHY), "") == 0, "phy has no unit");

	phy = mp_device_phy(&radioDev, &a);
	expect(phy == &radio, "subscribe returns the phy");
	expect(mp_device_phy(&radioDev, &b) == &radio, "phy is shared");
	expect(radioDriver.users == 2, "two subscribers");

	expect(mp_device_phy_open(NULL) == FALSE, "null open");
	expect(mp_device_phy_tune(phy, 2437000000UL, 20000000UL) == FALSE, "tune before hold");
	expect(tunes == 0, "tune was not called");
	expect(mp_device_phy_hold(phy) == FALSE, "hold before open");
	expect(mp_device_phy_open(phy) == TRUE, "open");
	expect(opens == 1, "open once");
	expect(mp_device_phy_open(phy) == TRUE && opens == 1, "open stays up");
	expect(mp_device_phy_ready(phy) == YES, "ready");

	expect(mp_device_phy_hold(phy) == TRUE, "hold");
	expect(holds == 1 && phy->held == YES, "held");
	expect(mp_device_phy_hold(phy) == FALSE && holds == 1, "second hold refused");
	expect(mp_device_phy_tune(phy, 2400000000UL, 20000000UL) == FALSE && tunes == 0, "below the band");
	expect(mp_device_phy_tune(phy, 2437000000UL, 2000000UL) == FALSE && tunes == 0, "width refused");
	expect(mp_device_phy_tune(phy, 2437000000UL, 20000000UL) == TRUE, "tune");
	expect(tunes == 1, "tune once");
	expect(mp_device_phy_hz(phy) == 2437000000UL, "stored hz");
	expect(mp_device_phy_width(phy) == 20000000UL, "stored width");
	expect(mp_device_phy_link(phy, 100000000UL, 1) == FALSE && links == 0, "radio has no link");

	mp_device_phy_release(phy);
	expect(releases == 1 && phy->held == NO, "released");
	mp_device_phy_release(phy);
	expect(releases == 1, "release without a hold");
	expect(mp_device_phy_tune(phy, 2437000000UL, 20000000UL) == FALSE && tunes == 1, "tune after release");

	holdRefuse = 1;
	expect(mp_device_phy_hold(phy) == FALSE && phy->held == NO, "driver refuses hold");
	holdRefuse = 0;

	mp_device_phy_close(phy);
	expect(closes == 1 && phy->opened == NO, "closed");
	expect(mp_device_phy_hz(phy) == 0 && mp_device_phy_width(phy) == 0, "tune cleared");
	mp_device_phy_close(phy);
	expect(closes == 1, "close while down");
	expect(mp_device_phy_open(phy) == TRUE && opens == 2, "reopen");

	expect(mp_device_phy_hold(phy) == TRUE, "hold again");
	mp_device_phy_close(phy);
	expect(releases == 2 && closes == 2 && phy->held == NO, "close releases");

	memset(&bare, 0, sizeof(bare));
	bare.hz_min = 1;
	bare.hz_max = 2;
	bare.width_min = 1;
	bare.width_max = 2;
	expect(mp_device_phy_open(&bare) == FALSE, "missing open");
	bare.opened = YES;
	expect(mp_device_phy_hold(&bare) == FALSE, "missing hold");
	bare.held = YES;
	expect(mp_device_phy_tune(&bare, 1, 1) == FALSE, "missing tune");
	expect(mp_device_phy_ready(&bare) == NO, "missing ready");
	expect(mp_device_phy_link(&bare, 1, 1) == FALSE, "missing link");

	expect(mp_device_driver_register(&kernel, &wireDriver, &wireOps, &wireDev, NULL) == TRUE, "load ethernet");
	expect(mp_device_registry_find(&kernel, MP_DEVICE_PHY, 1) == &wireDev, "second phy");
	phy = mp_device_phy(&wireDev, &w);
	expect(phy == &wire && phy->kind == MP_DEVICE_PHY_ETHERNET, "ethernet kind");
	expect(mp_device_phy_link(phy, 100000000UL, 1) == TRUE && links == 1, "link");
	expect(mp_device_phy_open(phy) == TRUE, "ethernet open");
	expect(mp_device_phy_hold(phy) == TRUE, "ethernet hold");
	expect(mp_device_phy_tune(phy, 2437000000UL, 20000000UL) == FALSE, "ethernet has no tune");

	mp_device_fini(&kernel);
	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
