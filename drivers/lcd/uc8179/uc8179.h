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

#ifndef _HAVE_MP_DRV_LCD_UC8179_H
	#define _HAVE_MP_DRV_LCD_UC8179_H

	/**
	 * Ultrachip UC8179, the 800x480 panel of the 7.5 inch ePaper.
	 * Clock and MOSI are GPIO (mode 0, MSB first). The panel has no MISO.
	 * The power pin feeds the panel rail and stays high while the device is open.
	 * With the TUI module, the struct starts with an mp_tui_display_t and
	 * the device face is that display. write() turns RGB565 into the 1-bit
	 * plane. flush() compares that plane to the image already on the glass
	 * and refreshes only the pixels that differ. A change that covers the
	 * glass uses the full waveform, which inverts every pixel.
	 */

	#define MP_DRV_UC8179_WIDTH 800
	#define MP_DRV_UC8179_HEIGHT 480

	typedef struct mp_drv_uc8179_s mp_drv_uc8179_t;

	struct mp_drv_uc8179_s {
#ifdef SUPPORT_COMMON_TUI
		mp_tui_display_t display;
#endif
		mp_kernel_t *kernel;
		mp_gpio_port_t *sck;
		mp_gpio_port_t *mosi;
		mp_gpio_port_t *cs;
		mp_gpio_port_t *dc;
		mp_gpio_port_t *reset;
		mp_gpio_port_t *busy;
		mp_gpio_port_t *power;
		/** 1 is white; the left pixel of a byte is the high bit */
		unsigned char plane[MP_DRV_UC8179_WIDTH * MP_DRV_UC8179_HEIGHT / 8];
#ifdef SUPPORT_COMMON_TUI
		/** the plane last pushed to the glass, so flush can skip unchanged pixels */
		unsigned char sent[MP_DRV_UC8179_WIDTH * MP_DRV_UC8179_HEIGHT / 8];
#endif
		/** panel setting: 0x1F native, 0x1B mirrors left to right, 0x13 turns 180 degrees */
		unsigned char psr;
		unsigned char on;
		/** the short waveform is selected */
		unsigned char fast;
		/** tick of the last glass update; a busy page waits between partials */
		unsigned long glassAt;
		/** writes since the last flush, union of their rectangles */
		unsigned char box;
		unsigned short boxX;
		unsigned short boxY;
		unsigned short boxW;
		unsigned short boxH;
	};

	mp_ret_t mp_drv_uc8179_init(mp_kernel_t *kernel, mp_drv_uc8179_t *panel, mp_options_t *options, char *who);
	mp_ret_t mp_drv_uc8179_fini(mp_drv_uc8179_t *panel);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_DISPLAY */
	extern const mp_device_ops_t mp_drv_uc8179_device;
#endif

#endif
