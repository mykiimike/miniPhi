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

#ifndef _HAVE_MP_DRV_LCD_ST7789_H
	#define _HAVE_MP_DRV_LCD_ST7789_H

	/**
	 * Sitronix ST7789, RGB565, on SPI. The controller RAM is 240x320.
	 * glass names the panel in that RAM, and each device keeps its own.
	 * 172x320, the default, is centered: the short axis starts 34 pixels
	 * in, and that IPS glass needs color inversion. 240x320 fills the RAM,
	 * BGR, inversion off. Landscape swaps the axes and moves the offset
	 * onto the rows. The driver opens a shared mp_spi_bus_t on SCK and
	 * MOSI. DC, reset, chip select and backlight are GPIO. Chip select is
	 * active low. Backlight is active high.
	 *
	 * options: sck, mosi, dc, cs, reset, backlight, glass (172x320,
	 * 240x320), orientation (landscape, portrait, landscape-flipped,
	 * portrait-flipped).
	 */

	#define MP_DRV_ST7789_GRAM 240
	#define MP_DRV_ST7789_SHORT 172
	#define MP_DRV_ST7789_LONG 320
	/** (240 - 172) / 2, where the 172-wide glass starts in the controller RAM */
	#define MP_DRV_ST7789_OFFSET 34

	#include <spi/spi.h>

	typedef struct mp_drv_st7789_s mp_drv_st7789_t;

	struct mp_drv_st7789_s {
		mp_kernel_t *kernel;
		mp_spi_bus_t bus;
		mp_gpio_port_t *dc;
		mp_gpio_port_t *cs;
		mp_gpio_port_t *reset;
		mp_gpio_port_t *backlight;
		int width;
		int height;
		int xoff;
		int yoff;
		unsigned char madctl;
		/** 16-bit pixel format this glass accepts */
		unsigned char colmod;
		/** the 172-wide IPS glass is inverted; the full 240x320 glass is not */
		unsigned char invert;
		unsigned char on;
		/** RP2350 GPIO numbers, driven through SIO */
		int n_dc, n_cs, n_sck, n_mosi, n_rst, n_bl;
#ifdef SUPPORT_COMMON_PUI
		/** 16-bit face: rows stream to the glass, RGB565 low byte first in memory */
		mp_pui_display_t pui;
#endif
#ifdef SUPPORT_COMMON_TUI
		mp_tui_display_t tui;
#endif
	};

	mp_ret_t mp_drv_st7789_init(mp_kernel_t *kernel, mp_drv_st7789_t *lcd, mp_options_t *options, char *who);
	mp_ret_t mp_drv_st7789_fini(mp_drv_st7789_t *lcd);
	mp_ret_t mp_drv_st7789_power(mp_drv_st7789_t *lcd, int on);
	void mp_drv_st7789_backlight(mp_drv_st7789_t *lcd, int percent);

#ifdef SUPPORT_COMMON_DEVICE
	extern const mp_device_ops_t mp_drv_st7789_device;
#endif

#endif
