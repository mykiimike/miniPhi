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

#ifndef _HAVE_MP_DRV_LCD_ST7735_H
	#define _HAVE_MP_DRV_LCD_ST7735_H

	/**
	 * Sitronix ST7735S, 160x80, RGB565, on SPI.
	 * The glass is 80x160 in GRAM, with column offset 24 and row offset 0.
	 * MADCTL with MV presents that as 160x80 and moves the column offset onto
	 * the rows. The driver opens a shared mp_spi_bus_t on SCK and MOSI. DC and
	 * reset are GPIO. Chip select is tied on the module, so there is no CS pin,
	 * and there is no backlight pin. Rows stream straight to the panel.
	 *
	 * options: sck, mosi, dc, reset, orientation (native, flip, inverted).
	 */

	#define MP_DRV_ST7735_WIDTH 160
	#define MP_DRV_ST7735_HEIGHT 80
	/** GRAM column where the 80-wide glass starts */
	#define MP_DRV_ST7735_OFFSET_X 24
	/** GRAM row where the 160-tall glass starts */
	#define MP_DRV_ST7735_OFFSET_Y 0

	/** MX|MV: landscape, the 160x80 face */
	#define MP_DRV_ST7735_MADCTL_NATIVE 0x60
	/** MY|MV: the other landscape */
	#define MP_DRV_ST7735_MADCTL_FLIP 0xA0
	/** MX|MY|MV: landscape, both axes mirrored */
	#define MP_DRV_ST7735_MADCTL_INVERTED 0xE0

	#include <spi/spi.h>

	typedef struct mp_drv_st7735_s mp_drv_st7735_t;

	struct mp_drv_st7735_s {
		mp_kernel_t *kernel;
		/** the shared SPI bus that owns SCK and MOSI */
		mp_spi_bus_t bus;
		mp_gpio_port_t *dc;
		mp_gpio_port_t *reset;
		unsigned char madctl;
		unsigned char on;
#ifdef SUPPORT_COMMON_PUI
		/** 16-bit face: rows stream to the glass, RGB565 low byte first in memory */
		mp_pui_display_t pui;
#endif
	};

	mp_ret_t mp_drv_st7735_init(mp_kernel_t *kernel, mp_drv_st7735_t *lcd, mp_options_t *options, char *who);
	mp_ret_t mp_drv_st7735_fini(mp_drv_st7735_t *lcd);
	mp_ret_t mp_drv_st7735_power(mp_drv_st7735_t *lcd, int on);

#ifdef SUPPORT_COMMON_DEVICE
	/** The app opens this display. Init paints a red frame. With PUI the face is an mp_pui_display_t at bpp 16. */
	extern const mp_device_ops_t mp_drv_st7735_device;
#endif

#endif
