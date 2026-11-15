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

#ifdef SUPPORT_DRV_TOUCH_XPT2046

#ifndef _HAVE_MP_DRV_TOUCH_XPT2046_H
	#define _HAVE_MP_DRV_TOUCH_XPT2046_H

	/**
	 * XPTEK XPT2046, an ADS7846-compatible 12-bit controller on the four
	 * plates of a resistive sheet. Those wires stop at the chip. The driver
	 * opens its own SPI bus (SCK, MOSI, MISO) and does not share the LCD
	 * bus. Chip select is active low, owned here, and idle high. PENIRQ is
	 * valid only while chip select is high; it is not read during a
	 * conversion. Without the irq option a press is Z1 alone.
	 *
	 * options: sck, mosi, miso, cs, irq (optional, active low), orientation
	 * (landscape, portrait, landscape-flipped, portrait-flipped; landscape
	 * swaps the axes), columns, rows (panel axes before orientation, 240
	 * and 320), xmin, xmax, ymin, ymax, zmin. The raw window defaults to
	 * 200..3900, a starting range, not a calibration of one glass; the
	 * board overrides it. zmin defaults to 200. model defaults to
	 * "xpt2046". serial replaces the MCU id when the option is set.
	 */

	#include <spi/spi.h>

	typedef struct mp_drv_xpt2046_s mp_drv_xpt2046_t;

	struct mp_drv_xpt2046_s {
		mp_kernel_t *kernel;
		mp_device_input_t input;
		mp_spi_bus_t bus;
		/** active low, not part of the SPI bus, idle high */
		mp_gpio_port_t *cs;
		/** active low input. NULL when the board did not wire PENIRQ. */
		mp_gpio_port_t *irq;

		mp_device_driver_t binding;
		mp_device_registry_t registry;

		int orientation;
		/** panel columns and rows, before orientation turns them into the display */
		int columns;
		int rows;
		/** raw 12-bit window */
		int xMin, xMax, yMin, yMax;
		int zMin;

		const char *model;
		const char *serial;

		int down;
		int seen;
		int miss;
		int px, py;
		int fx, fy;
		int tracked;

		/** sample, idle and doze delays, in ticks */
		unsigned long period;
		unsigned long idle;
		unsigned long doze;
		mp_bool_t armed;
	};

	mp_ret_t mp_drv_xpt2046_init(mp_kernel_t *kernel, mp_drv_xpt2046_t *touch, mp_options_t *options, char *who);
	mp_ret_t mp_drv_xpt2046_fini(mp_drv_xpt2046_t *touch);

#ifdef SUPPORT_COMMON_DEVICE
	extern const mp_device_ops_t mp_drv_xpt2046_device;
#endif

#endif

#endif
