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

#include "../include/pio.h"

/**
@defgroup mpArchRP2350Pixel Pixel

@ingroup mpArchRP2350

The Waveshare RP2350-LCD-1.47 wires one WS2812 to GPIO22. PIO0 state
machine 0 shifts the frame. The wire order is green, red, blue. After
the last bit the program stalls with the pin low, which is the latch.

boards/waveshare/rp2350-lcd-147/_phi.js reserves pa22. Drop that
reservation and add no devices[] entry. This file registers the pixel
itself, and start() claims pa22 with mp_gpio_text_handle. A pin that
is already taken is left alone.

@{
*/

#define _PIN 22

/*
 * pico-sdk ws2812 program (side-set 1). T1 = T2 = T3 = 5 cycles, so the
 * delay field is 4. One bit is 15 cycles. At a 12 MHz PIO clock that is
 * 1.25 us: T0H 0.417 us, T1H 0.833 us, T0L 0.833 us, T1L 0.417 us.
 *
 *  0: out x, 1   side 0 [4]
 *  1: jmp !x, 3  side 1 [4]
 *  2: jmp 0      side 1 [4]
 *  3: nop        side 0 [4]
 */
#define _PIO_OUT  0x6421
#define _PIO_JNZ  0x1423
#define _PIO_JMP  0x1400
#define _PIO_NOP  0xa442

/* SET PINDIRS, 1 and JMP 0, executed through SM0_INSTR while disabled. */
#define _PIO_SET_OUT 0xe081
#define _PIO_JMP0    0x0000

/* 12 MHz PIO clock. Divider is clk_sys / 12 MHz, in 1/256ths. */
#define _PIO_HZ 12000000UL

/* 24 bits at 1.25 us is 30 us. The stalled OUT then holds the line low. */
#define _SHIFT_US 40UL
#define _LATCH_US 80UL
#define _PULL_US  200UL

#ifdef SUPPORT_COMMON_DEVICE

static struct {
	mp_gpio_port_t *pin;
	int ready;
	int busy;
	mp_device_driver_t binding;
	mp_device_registry_t device;
	mp_device_pixel_t face;
	mp_device_pixel_channel_t channel[MP_DEVICE_PIXEL_CHANNELS];
} _pixel;

static int _pio_reset(void) {
	unsigned int spins = 100000;

	RESETS_RESET &= ~RESETS_PIO0;
	while(spins-- && !(RESETS_RESET_DONE & RESETS_PIO0))
		;
	if(!(RESETS_RESET_DONE & RESETS_PIO0))
		return(0);
	return(1);
}

/* 16.8 divider. Integer 0 in the register means 65536, so fraction stays 0. */
static void _pio_div(void) {
	unsigned long clk = mp_clock_get_speed();
	uint64_t div256;
	unsigned long integ;
	unsigned long frac;
	uint32_t ctrl;

	if(clk < _PIO_HZ)
		clk = _PIO_HZ;
	div256 = ((uint64_t)clk << 8) / _PIO_HZ;
	integ = (unsigned long)(div256 >> 8);
	frac = (unsigned long)(div256 & 0xffU);
	if(integ == 0UL) {
		integ = 1UL;
		frac = 0UL;
	}
	if(integ > 65535UL) {
		integ = 0UL;
		frac = 0UL;
	}
	PIO0_SM0_CLKDIV = (uint32_t)((integ << PIO_SM_CLKDIV_INT_LSB) | (frac << PIO_SM_CLKDIV_FRAC_LSB));
	ctrl = PIO0_CTRL;
	if(ctrl & PIO_CTRL_SM0_ENABLE)
		PIO0_CTRL = PIO_CTRL_SM0_ENABLE | PIO_CTRL_SM0_CLKDIV_RESTART;
}

void mp_pio_retime(void) {
	if(_pixel.ready == 0)
		return;
	_pio_div();
}

static void _us(unsigned long us) {
	unsigned long start = mp_clock_us();
	unsigned long guard = us * 400UL;

	while((mp_clock_us() - start) < us && guard--)
		__asm__ volatile("nop");
}

static void _commit(mp_device_pixel_t *face) {
	unsigned char red, green, blue;
	unsigned long word;
	unsigned long start;
	unsigned long guard;
	int index;

	(void)face;
	if(_pixel.ready == 0)
		return;
	/* A previous word is still queued. Dropping this frame beats a full FIFO. */
	if(_pixel.busy || (PIO0_FSTAT & PIO_FSTAT_TXEMPTY0) == 0)
		return;

	_pixel.busy = 1;
	mp_pio_retime();
	for(index = 0; index < _pixel.face.count && index < 1; index++) {
		if(mp_device_pixel_color(&_pixel.face, index, &red, &green, &blue) == FALSE)
			break;
		/* Top 24 bits, MSB first. Autopull at 24 drops the low padding. */
		word = ((unsigned long)green << 24) | ((unsigned long)red << 16) | ((unsigned long)blue << 8);
		PIO0_TXF0 = (uint32_t)word;
		/*
		 * TXEMPTY falls while the word sits in the FIFO and rises when the
		 * stalled OUT pulls it. The 24 bits then take 30 us, and the next
		 * OUT stalls with the pin low. That low time is the latch.
		 */
		start = mp_clock_us();
		guard = _PULL_US * 400UL;
		while((PIO0_FSTAT & PIO_FSTAT_TXEMPTY0) == 0 && (mp_clock_us() - start) < _PULL_US && guard--)
			;
		_us(_SHIFT_US + _LATCH_US);
	}
	_pixel.busy = 0;
}

static void _exec(uint32_t instr) {
	unsigned int spins = 10000;

	PIO0_SM0_INSTR = instr;
	while(spins-- && (PIO0_SM0_EXECCTRL & PIO_SM_EXECCTRL_EXEC_STALLED))
		;
}

static void _load(void) {
	PIO0_INSTR_MEM(0) = _PIO_OUT;
	PIO0_INSTR_MEM(1) = _PIO_JNZ;
	PIO0_INSTR_MEM(2) = _PIO_JMP;
	PIO0_INSTR_MEM(3) = _PIO_NOP;
	PIO0_SM0_EXECCTRL = 3UL << PIO_SM_EXECCTRL_WRAP_TOP_LSB;
	PIO0_SM0_SHIFTCTRL = (24UL << PIO_SM_SHIFTCTRL_PULL_THRESH_LSB)
		| PIO_SM_SHIFTCTRL_IN_SHIFTDIR
		| PIO_SM_SHIFTCTRL_AUTOPULL;
	PIO0_SM0_PINCTRL = (1UL << PIO_SM_PINCTRL_SIDESET_COUNT_LSB)
		| (1UL << PIO_SM_PINCTRL_SET_COUNT_LSB)
		| ((unsigned long)_PIN << PIO_SM_PINCTRL_SIDESET_BASE_LSB)
		| ((unsigned long)_PIN << PIO_SM_PINCTRL_SET_BASE_LSB);
	_pio_div();
	/* Empty OSR: the first OUT autopulls the frame instead of shifting zeros. */
	PIO0_CTRL = PIO_CTRL_SM0_RESTART;
	_exec(_PIO_SET_OUT);
	_exec(_PIO_JMP0);
	PIO0_CTRL = PIO_CTRL_SM0_ENABLE | PIO_CTRL_SM0_CLKDIV_RESTART;
}

static void _start(void *self) {
	(void)self;
	_pixel.pin = mp_gpio_text_handle("pa22", "WS2812");
	if(!_pixel.pin)
		return;
	if(!_pio_reset()) {
		mp_gpio_release(_pixel.pin);
		_pixel.pin = NULL;
		return;
	}
	mp_gpio_af(_pixel.pin, IO_GPIO_FUNCSEL_PIO0);
	mp_gpio_mode(_pixel.pin, MP_GPIO_MODE_AF);
	_load();
	_pixel.ready = 1;
	mp_printk("WS2812 on pa22");
}

static void _stop(void *self) {
	(void)self;
	_pixel.ready = 0;
	PIO0_CTRL = 0;
	if(_pixel.pin) {
		mp_gpio_release(_pixel.pin);
		_pixel.pin = NULL;
	}
	RESETS_RESET |= RESETS_PIO0;
}

static const mp_device_driver_ops_t _ops = {
	"arch/rp2350", "WS2812", "1.0.0",
	(unsigned int)(1u << MP_DEVICE_PIXEL),
	NULL, NULL, NULL, _start, _stop
};

void mp_rp2350_pixel(mp_kernel_t *kernel) {
	memset(&_pixel, 0, sizeof(_pixel));
	if(mp_device_driver_register(kernel, &_pixel.binding, &_ops, &_pixel, NULL) == FALSE)
		return;
	_pixel.face.commit = _commit;
	_pixel.face.channel = _pixel.channel;
	_pixel.face.count = 1;
	_pixel.face.user = &_pixel;
	if(!mp_device_registry_register(&_pixel.binding, &_pixel.device, MP_DEVICE_PIXEL, 0, &_pixel.face, "ws2812", "pa22", NULL)) {
		mp_device_driver_unregister(kernel, &_pixel.binding);
		return;
	}
	_pixel.device.label = "WS2812";
}

void mp_rp2350_pixel_fini(mp_kernel_t *kernel) {
	mp_device_registry_unregister(&_pixel.device);
	mp_device_driver_unregister(kernel, &_pixel.binding);
}

#else

void mp_pio_retime(void) {
}

void mp_rp2350_pixel(mp_kernel_t *kernel) {
	(void)kernel;
}

void mp_rp2350_pixel_fini(mp_kernel_t *kernel) {
	(void)kernel;
}

#endif

/**@}*/
