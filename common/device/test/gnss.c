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

/* Host test of the GNSS contract. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_DEVICE
#include "../addr.c"
#include "../driver.c"
#include "../registry.c"
#include "../subscribe.c"
#include "../measure.c"
#include "../gnss.c"
#include "../device.c"

static int failed;
static int starts;
static int stops;
static mp_device_gnss_t face;

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
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
	memset(&face, 0, sizeof(face));
	face.user = &face;
	if(!mp_device_registry_register(driver, device, MP_DEVICE_GNSS, 0, &face, "venus638", "uart", NULL))
		return(FALSE);
	return(TRUE);
}

static const mp_device_driver_ops_t gnssOps = {
	"gnss/venus638", "VENUS638", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_GNSS),
	_load, NULL, NULL, _start, _stop
};

int main(void) {
	mp_kernel_t kernel;
	mp_device_driver_t driver;
	mp_device_registry_t dev;
	mp_device_subscribe_t a, b;
	mp_device_gnss_t *gnss;
	mp_device_gnss_t sample;
	mp_device_gnss_t next;

	memset(&kernel, 0, sizeof(kernel));
	memset(&driver, 0, sizeof(driver));
	memset(&dev, 0, sizeof(dev));
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	memset(&sample, 0, sizeof(sample));
	memset(&next, 0, sizeof(next));
	mp_device_init(&kernel);

	expect(mp_device_measure_class(MP_DEVICE_GNSS) == NO, "gnss is not a measure");
	expect(mp_device_driver_register(&kernel, &driver, &gnssOps, &dev, NULL) == TRUE, "load");
	expect(strcmp(dev.address, "gnss0.venus638.uart") == 0, "address");

	gnss = mp_device_gnss(&dev, &a);
	expect(gnss == &face, "subscribe returns the receiver");
	expect(starts == 1 && driver.users == 1, "first subscriber starts");
	expect(mp_device_gnss_read(gnss, &sample) == FALSE, "nothing published yet");

	memset(&sample, 0, sizeof(sample));
	sample.quality = 1;
	sample.satellites = 7;
	sample.latitude = 488566000L;
	sample.longitude = 23522000L;
	sample.altitude = 35000L;
	sample.speed = 1200UL;
	sample.heading = 9000;
	sample.hour = 12;
	sample.minute = 34;
	sample.second = 56;
	mp_device_gnss_publish(gnss, &sample);
	expect(mp_device_gnss_read(gnss, &next) == TRUE, "read");
	expect(next.quality == 1 && next.satellites == 7 && next.system == 0, "quality");
	expect(mp_device_gnss_system("GPRMC") == MP_DEVICE_GNSS_SYSTEM_GPS, "GP");
	expect(mp_device_gnss_system("GLGGA") == MP_DEVICE_GNSS_SYSTEM_GLONASS, "GL");
	expect(mp_device_gnss_system("GNGGA") == MP_DEVICE_GNSS_SYSTEM_MIXED, "GN");
	expect(mp_device_gnss_system("BDGGA") == MP_DEVICE_GNSS_SYSTEM_BEIDOU, "BD");
	expect(strcmp(mp_device_gnss_system_text(MP_DEVICE_GNSS_SYSTEM_GPS), "GPS") == 0, "GPS text");
	expect(mp_device_gnss_system_text(MP_DEVICE_GNSS_SYSTEM_NONE)[0] == 0, "no system");
	sample.system = MP_DEVICE_GNSS_SYSTEM_GPS;
	sample.viewed = 1;
	sample.sat[0].id = 12;
	sample.sat[0].elevation = 40;
	sample.sat[0].azimuth = 180;
	sample.sat[0].snr = 36;
	sample.sat[0].used = YES;
	mp_device_gnss_publish(gnss, &sample);
	expect(mp_device_gnss_read(gnss, &next) == TRUE && next.system == MP_DEVICE_GNSS_SYSTEM_GPS, "system");
	expect(next.viewed == 1 && next.sat[0].id == 12 && next.sat[0].used == YES, "satellite");
	expect(next.latitude == 488566000L && next.longitude == 23522000L, "position");
	expect(next.altitude == 35000L && next.speed == 1200UL && next.heading == 9000, "motion");
	expect(next.hour == 12 && next.minute == 34 && next.second == 56, "utc");
	expect(next.fixed == YES && next.valid == YES, "fix");
	expect(gnss->user == &face && next.user == &face, "publish keeps user");

	sample.quality = 0;
	mp_device_gnss_publish(gnss, &sample);
	expect(mp_device_gnss_read(gnss, &next) == TRUE, "read after no fix");
	expect(next.quality == 0 && next.fixed == NO && next.valid == YES, "quality 0 is not a fix");

	expect(mp_device_gnss(&dev, &b) == &face, "shared");
	expect(driver.users == 2, "second subscriber counts");
	mp_device_subscribe_end(&a);
	expect(stops == 0 && driver.users == 1, "one subscriber left stays started");
	mp_device_subscribe_end(&b);
	expect(stops == 1 && driver.running == NO, "last subscriber stops");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	return(0);
}
