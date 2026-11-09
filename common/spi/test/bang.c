/* Host test of the SPI bit-bang. Not linked into the firmware. */

#include <stdio.h>
#include <string.h>

#define SUPPORT_COMMON_SPI

#include <mp.h>

typedef struct mp_gpio_port_s mp_gpio_port_t;

struct mp_gpio_port_s {
	int level;
};

typedef enum {
	MP_GPIO_INPUT = 1,
	MP_GPIO_OUTPUT = 2,
} mp_gpio_direction_t;

typedef enum {
	MP_GPIO_MODE_INPUT = 1,
	MP_GPIO_MODE_INPUT_PULL = 2,
	MP_GPIO_MODE_OUTPUT = 3,
	MP_GPIO_MODE_OUTPUT_OD = 4,
} mp_gpio_mode_t;

static mp_gpio_port_t sclk_pin;
static mp_gpio_port_t mosi_pin;
static int mosi_bit[16];
static int mosi_n;
static int rise;
static int fall;
static int order_ok;
static unsigned long micros;

unsigned long mp_clock_us(void) {
	return(micros += 5);
}

void test_printk(const char *fmt, ...) {
	(void)fmt;
}

char *mp_options_get(mp_options_t *options, char *key) {
	(void)options;
	(void)key;
	return(NULL);
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

mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction) {
	(void)port;
	(void)direction;
	return(TRUE);
}

void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode) {
	(void)port;
	(void)mode;
}

void mp_gpio_set(mp_gpio_port_t *port) {
	port->level = 1;
	if(port == &mosi_pin) {
		if(mosi_n < 16)
			mosi_bit[mosi_n++] = 1;
	}
	if(port == &sclk_pin) {
		rise++;
		if(mosi_n != rise)
			order_ok = 0;
	}
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	port->level = 0;
	if(port == &mosi_pin) {
		if(mosi_n < 16)
			mosi_bit[mosi_n++] = 0;
	}
	if(port == &sclk_pin)
		fall++;
}

#include "../spi.h"
#include "../spi.c"

static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static void reset_log(void) {
	memset(mosi_bit, 0, sizeof(mosi_bit));
	mosi_n = 0;
	rise = 0;
	fall = 0;
	order_ok = 1;
	sclk_pin.level = 0;
	mosi_pin.level = 0;
}

/* 0xA5 is 10100101, MSB first. SCLK rises after MOSI and ends low. */
static void check_a5(const char *msg) {
	static const int want[8] = { 1, 0, 1, 0, 0, 1, 0, 1 };

	expect(mosi_n == 8 && memcmp(mosi_bit, want, sizeof(want)) == 0, msg);
	expect(rise == 8 && fall == 8 && order_ok && sclk_pin.level == 0, "clock");
}

int main(void) {
	mp_spi_bus_t bus;
	unsigned char byte;

	memset(&bus, 0, sizeof(bus));
	bus.sclk = &sclk_pin;
	bus.mosi = &mosi_pin;
	bus.half_us = 5;

	reset_log();
	mp_spi_write_byte(&bus, 0xA5);
	check_a5("0xA5 msb first");

	reset_log();
	byte = 0xA5;
	expect(mp_spi_tx(&bus, &byte, 1) == 1, "tx");
	check_a5("tx clocks the same byte");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
