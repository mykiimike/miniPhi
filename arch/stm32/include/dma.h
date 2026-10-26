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

#ifndef _HAVE_STM32_DMA_H
	#define _HAVE_STM32_DMA_H

	#ifdef SUPPORT_COMMON_DMA

	/**
	 * One request line of the central controller, held until release.
	 * request is "spi2.rx" style. arm is the peripheral's DMA enable,
	 * called for the life of the claim. prep runs at each start.
	 * width is 1, 2 or 4. The engine accepts one segment and refuses cyclic.
	 * NULL when the request is missing or its channel is already claimed.
	 */
	mp_dma_engine_t *mp_dma_claim(
		const char *request,
		mp_dma_dir_t dir,
		volatile void *periph,
		unsigned char width,
		void (*arm)(void *user, mp_bool_t on),
		void (*prep)(void *user),
		void *user,
		char *who
	);
	void mp_dma_release(mp_dma_engine_t *engine);

	/** YES when no claimed channel is shifting */
	mp_bool_t mp_dma_quiet(void);

	#endif

#endif
