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

#ifdef SUPPORT_DRV_FS_SD

#ifndef _HAVE_MP_DRV_FS_SD_H
	#define _HAVE_MP_DRV_FS_SD_H

	/**
	 * One SD socket as block devices. options are the SD module's: sck,
	 * mosi, miso, cs, and optional cd. Open claims the bus even when the
	 * socket is empty. The whole card is registered as channel 0 when a
	 * probe identifies it, then one channel per primary partition. A
	 * protective MBR registers the card only. The card going away
	 * unregisters them. face() and a non-zero set() probe, so a card
	 * inserted later is registered on the next one. set(0) does not
	 * remove a card the socket still holds.
	 */

	#include <sd/sd.h>
	#include <part/part.h>

	#define MP_DRV_SD_PARTS 4

	typedef struct mp_drv_sd_s mp_drv_sd_t;

	struct mp_drv_sd_s {
		mp_kernel_t *kernel;
		mp_fs_sd_t card;
		mp_device_driver_t binding;
		mp_device_registry_t disk;
		mp_fs_part_t part[MP_DRV_SD_PARTS];
		mp_device_registry_t part_dev[MP_DRV_SD_PARTS];
		int parts;
		/** hex of the CID serial while a card is registered */
		char serial_text[16];
	};

	mp_ret_t mp_drv_sd_init(mp_kernel_t *kernel, mp_drv_sd_t *sd, mp_options_t *options, char *who);
	void mp_drv_sd_fini(mp_drv_sd_t *sd);

#ifdef SUPPORT_COMMON_DEVICE
	extern const mp_device_ops_t mp_drv_sd_device;
#endif

#endif

#endif
