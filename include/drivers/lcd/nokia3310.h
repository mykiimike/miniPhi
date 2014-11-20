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

	#ifdef SUPPORT_DRV_LCD_NOKIA3310

		typedef struct mp_drv_nokia3310_s mp_drv_nokia3310_t;

		struct mp_drv_nokia3310_s {
			/* */
			mp_kernel_t *kernel;

			/* SPI interface */
			mp_spi_t spi;

			/** DC port */
			mp_gpio_port_t *dc;

			/** Contrast port */
			mp_gpio_port_t *res;

			/* LCD memory */
			unsigned char LCDMemory[504];

			/* Reverse mode ? */
			unsigned char inverse;

			/* Global LCD memory position */
			int index;

			/** LCD TX position */
			int txX;
			int txY;

		};

		mp_ret_t mp_drv_nokia3310_init(mp_kernel_t *kernel, mp_drv_nokia3310_t *drv, mp_options_t *options, char *who);
		mp_ret_t mp_drv_nokia3310_fini(mp_drv_nokia3310_t *drv);
		void mp_drv_nokia3310_update(mp_drv_nokia3310_t *drv);
		void mp_drv_nokia3310_clear(mp_drv_nokia3310_t *drv);
		void mp_drv_nokia3310_contrast(mp_drv_nokia3310_t *drv, unsigned char contrast);
		void mp_drv_nokia3310_write(mp_drv_nokia3310_t *drv, unsigned char *a, int l);
		void mp_drv_nokia3310_writePos(mp_drv_nokia3310_t *drv, unsigned char *str, int l, int pos);
		unsigned char *mp_drv_nokia3310_getMemory(mp_drv_nokia3310_t *drv);
		void mp_drv_nokia3310_clearMemory(mp_drv_nokia3310_t *drv);
	#endif
#endif
