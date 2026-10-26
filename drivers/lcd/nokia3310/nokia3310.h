/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

#ifndef _HAVE_MP_DRV_LCD_NOKIA3310_H
	#define _HAVE_MP_DRV_LCD_NOKIA3310_H

	/**
	 * Philips PCD8544, the 84x48 panel of the Nokia 3310, on SPI.
	 * Chip select is a GPIO held low around each command or page.
	 * With the TUI module, the struct starts with an mp_tui_display_t and
	 * the device face is that display: write() turns RGB565 into the 1-bit
	 * pages, flush() ships the pages that changed.
	 */

	typedef struct mp_drv_nokia3310_s mp_drv_nokia3310_t;

	struct mp_drv_nokia3310_s {
#ifdef SUPPORT_COMMON_TUI
		mp_tui_display_t display;
#endif
		mp_kernel_t *kernel;
		mp_spi_t spi;
		mp_gpio_port_t *dc;
		mp_gpio_port_t *reset;
		mp_gpio_port_t *cs;
		/** 6 pages of 84 columns; the low bit is the top pixel of the page */
		unsigned char gram[6 * 84];
		unsigned char dirty;
		unsigned char vop;
		unsigned char on;
	};

	mp_ret_t mp_drv_nokia3310_init(mp_kernel_t *kernel, mp_drv_nokia3310_t *lcd, mp_options_t *options, char *who);
	mp_ret_t mp_drv_nokia3310_fini(mp_drv_nokia3310_t *lcd);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_DISPLAY */
	extern const mp_device_ops_t mp_drv_nokia3310_device;
#endif

#endif
