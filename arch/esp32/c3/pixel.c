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

#include <mp.h>

/**
@defgroup mpArchESP32C3Pixel Pixel

@ingroup mpArchESP32

The Waveshare ESP32-C3-Zero wires one WS2812 to GPIO10. RMT channel 0
shifts the frame. The wire order is green, red, blue. The line stays low
after the last bit, which is the latch.

@{
*/

#define _RMT            0x60016000UL
#define _RMT_MEM        0x60016400UL
#define _RMT_CONF       (_RMT + 0x10UL)
#define _RMT_INT_RAW    (_RMT + 0x38UL)
#define _RMT_INT_CLR    (_RMT + 0x44UL)
#define _RMT_SYS        (_RMT + 0x68UL)
#define _OUT_SEL        (C3_GPIO + 0x554UL)

/* matrix signal of RMT channel 0. OEN_SEL (bit 9) keeps the GPIO enable. */
#define _RMT_SIG        51UL
#define _PIN            10

/* 40 MHz tick from the crystal, divider 1. 0.40 / 0.85 us and 0.80 / 0.45 us. */
#define _T0H            16UL
#define _T0L            34UL
#define _T1H            32UL
#define _T1L            18UL

#ifdef SUPPORT_COMMON_DEVICE

static struct {
	mp_gpio_port_t *pin;
	int ready;
	mp_device_driver_t binding;
	mp_device_registry_t device;
	mp_device_pixel_t face;
	mp_device_pixel_channel_t channel[MP_DEVICE_PIXEL_CHANNELS];
} _pixel;

static void _gap(void) {
	unsigned long hz = mp_clock_get_speed();
	unsigned long left;

	if(hz < 1000000UL)
		hz = C3_CPU_MEDIUM_HZ;
	/* a few cycles a turn, so the low time stays past the 50 us latch */
	left = hz / 20000UL;
	while(left--)
		__asm__ volatile ("nop");
}

static unsigned long _item(unsigned long high, unsigned long low) {
	return((high & 0x7fffUL) | (1UL << 15) | ((low & 0x7fffUL) << 16));
}

static void _byte(volatile unsigned long **slot, unsigned char value) {
	int bit;

	for(bit = 7; bit >= 0; bit--) {
		if(value & (1U << bit))
			*(*slot)++ = _item(_T1H, _T1L);
		else
			*(*slot)++ = _item(_T0H, _T0L);
	}
}

static void _commit(mp_device_pixel_t *face) {
	volatile unsigned long *slot = (volatile unsigned long *)_RMT_MEM;
	unsigned long conf;
	unsigned long spin;
	unsigned char red, green, blue;
	int index;

	(void)face;
	if(_pixel.ready == 0)
		return;

	for(index = 0; index < _pixel.face.count && index < 1; index++) {
		if(mp_device_pixel_color(&_pixel.face, index, &red, &green, &blue) == FALSE)
			break;
		_byte(&slot, green);
		_byte(&slot, red);
		_byte(&slot, blue);
	}
	*slot = 0;

	/* idle low, divider 1, one block. The start bits are write-triggered. */
	conf = (1UL << 6) | (1UL << 8) | (1UL << 16);
	c3_write(_RMT_INT_CLR, 1UL);
	c3_write(_RMT_CONF, conf | (1UL << 1));
	c3_write(_RMT_CONF, conf | (1UL << 24));
	c3_write(_RMT_CONF, conf | 1UL);

	spin = 100000;
	while((c3_read(_RMT_INT_RAW) & 1UL) == 0 && spin)
		spin--;
	if(!spin)
		c3_write(_RMT_CONF, conf | (1UL << 7) | (1UL << 24));
	c3_write(_RMT_INT_CLR, 1UL);
	_gap();
}

static void _start(void *self) {
	(void)self;
	_pixel.pin = mp_gpio_text_handle("pa10", "WS2812");
	if(!_pixel.pin)
		return;
	mp_gpio_direction(_pixel.pin, MP_GPIO_OUTPUT);

	c3_set(C3_SYSTEM + 0x10UL, 1UL << 9);
	c3_set(C3_SYSTEM + 0x18UL, 1UL << 9);
	c3_clr(C3_SYSTEM + 0x18UL, 1UL << 9);

	/* crystal, no fraction, memory powered, direct access, clock forced on */
	c3_write(_RMT_SYS, (1UL << 31) | (1UL << 26) | (3UL << 24) | (1UL << 18) | (1UL << 3) | 1UL);
	/* idle low, divider 1, one memory block */
	c3_write(_RMT_CONF, (1UL << 6) | (1UL << 8) | (1UL << 16));
	c3_write(_RMT_CONF, c3_read(_RMT_CONF) | (1UL << 24));
	c3_write(_OUT_SEL + _PIN * 4UL, _RMT_SIG | (1UL << 9));
	_pixel.ready = 1;
	mp_printk("WS2812 on pa10");
}

static void _stop(void *self) {
	(void)self;
	_pixel.ready = 0;
	if(!_pixel.pin)
		return;
	c3_write(_OUT_SEL + _PIN * 4UL, 0x80UL | (1UL << 9));
	mp_gpio_unset(_pixel.pin);
	mp_gpio_release(_pixel.pin);
	_pixel.pin = NULL;
	c3_clr(C3_SYSTEM + 0x10UL, 1UL << 9);
}

static const mp_device_driver_ops_t _ops = {
	"arch/c3", "WS2812", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_PIXEL),
	NULL, NULL, NULL, _start, _stop
};

void mp_c3_pixel(mp_kernel_t *kernel) {
	memset(&_pixel, 0, sizeof(_pixel));
	if(mp_device_driver_register(kernel, &_pixel.binding, &_ops, &_pixel, NULL) == FALSE)
		return;
	_pixel.face.commit = _commit;
	_pixel.face.channel = _pixel.channel;
	_pixel.face.count = 1;
	_pixel.face.user = &_pixel;
	if(!mp_device_registry_register(&_pixel.binding, &_pixel.device, MP_DEVICE_PIXEL, 0, &_pixel.face, "ws2812", "pa10", NULL)) {
		mp_device_driver_unregister(kernel, &_pixel.binding);
		return;
	}
	_pixel.device.label = "WS2812";
}

void mp_c3_pixel_fini(mp_kernel_t *kernel) {
	mp_device_registry_unregister(&_pixel.device);
	mp_device_driver_unregister(kernel, &_pixel.binding);
}

#else

void mp_c3_pixel(mp_kernel_t *kernel) {
	(void)kernel;
}

void mp_c3_pixel_fini(mp_kernel_t *kernel) {
	(void)kernel;
}

#endif

/**@}*/
