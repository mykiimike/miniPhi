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

#ifndef _HAVE_MP_DRV_LCD_SSD1306_H
	#define _HAVE_MP_DRV_LCD_SSD1306_H

	/**
	 * Solomon SSD1306, 128x64, on I2C.
	 * The driver opens a shared mp_i2c_bus_t on the pins the board names, so
	 * other devices on the same bus (the PCF8563, a sensor) borrow it instead
	 * of grabbing SDA and SCL a second time. Address 0x3C, or 0x3D when the
	 * board says so. Reset is optional: the expansion module ties it, so the
	 * board leaves it out.
	 *
	 * The glass is 8 pages of 128 columns. Bit 0 of a column is the top pixel
	 * of that page, page 0 is the top of the glass, column 0 is the left.
	 * mp_drv_ssd1306_point draws into that image. mp_drv_ssd1306_flush sends
	 * the pages that changed. The internal charge pump stays on.
	 *
	 * options: sda, clk, address (0x3C when missing), contrast (207 when missing), reset.
	 */

	#define MP_DRV_SSD1306_WIDTH 128
	#define MP_DRV_SSD1306_HEIGHT 64
	#define MP_DRV_SSD1306_PAGES 8

	#include <i2c/i2c.h>

	typedef struct mp_drv_ssd1306_s mp_drv_ssd1306_t;

	struct mp_drv_ssd1306_s {
		mp_kernel_t *kernel;
		/** the shared I2C bus that owns SDA and SCL */
		mp_i2c_bus_t bus;
		mp_gpio_port_t *reset;
		/** pages in order, each page a row of columns; bit 0 is the top pixel */
		unsigned char gram[MP_DRV_SSD1306_PAGES * MP_DRV_SSD1306_WIDTH];
		unsigned char dirty;
		unsigned char address;
		unsigned char contrast;
		unsigned char on;
#ifdef SUPPORT_COMMON_PUI
		/** 1-bit face over gram: rows top to bottom, MSB first in each byte */
		mp_pui_display_t pui;
#endif
	};

	mp_ret_t mp_drv_ssd1306_init(mp_kernel_t *kernel, mp_drv_ssd1306_t *lcd, mp_options_t *options, char *who);
	mp_ret_t mp_drv_ssd1306_fini(mp_drv_ssd1306_t *lcd);

	/** on is anything but 0. A point outside the glass is ignored. */
	void mp_drv_ssd1306_point(mp_drv_ssd1306_t *lcd, int x, int y, int on);
	void mp_drv_ssd1306_clear(mp_drv_ssd1306_t *lcd);
	/** Send the pages point() or clear() changed. TRUE when the panel is off: the image waits. */
	mp_ret_t mp_drv_ssd1306_flush(mp_drv_ssd1306_t *lcd);
	mp_ret_t mp_drv_ssd1306_power(mp_drv_ssd1306_t *lcd, int on);
	mp_ret_t mp_drv_ssd1306_contrast(mp_drv_ssd1306_t *lcd, int contrast);

	/** The I2C bus the panel owns; other devices on the same bus borrow it. */
	mp_i2c_bus_t *mp_drv_ssd1306_i2c(mp_drv_ssd1306_t *lcd);

#ifdef SUPPORT_COMMON_DEVICE
	/** The app opens this display and leaves the picture painted at init. With PUI the face is an mp_pui_display_t over this gram. */
	extern const mp_device_ops_t mp_drv_ssd1306_device;
#endif

#endif
