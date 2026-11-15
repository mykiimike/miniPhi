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

#ifdef SUPPORT_COMMON_FS_BLOCK_SD

#ifndef _HAVE_MP_COMMON_FS_BLOCK_SD_H
	#define _HAVE_MP_COMMON_FS_BLOCK_SD_H

	/**
	 * SD card in SPI mode. One struct, one card, its own bus and chip select.
	 * options: sck, mosi, miso, cs, all required. cs is active low and idle
	 * high. cd is optional, active low, with a pull-up: the pin reads low
	 * while a card is in the socket. Without it, presence is the last probe.
	 * mp_fs_sd_init claims the bus either way. A card that does not answer
	 * leaves the struct up and mp_fs_sd_present at NO, so a later probe can
	 * try again. Probe sends CMD0 only when the socket looks occupied and
	 * the card is not already identified. A transfer that gets no answer
	 * clears that identification. After a successful probe the block field
	 * is the medium FAT mounts. Sectors are 512 bytes. product is the
	 * five-character CID name. serial is the CID product serial.
	 * block_address is set when the card numbers sectors instead of bytes.
	 * ready is YES after the last probe identified a card.
	 * on_media, when set, runs on the transition: YES after a probe identifies
	 * the card, NO after that identification is cleared. Init probes before
	 * the owner can set it, so the owner checks ready afterwards. The callback
	 * runs once the bus transfer that noticed the change has finished.
	 */

	#include <fs/fs.h>
	#include <spi/spi.h>

	typedef struct mp_fs_sd_s mp_fs_sd_t;
	typedef void (*mp_fs_sd_media_t)(mp_fs_sd_t *card, mp_bool_t present, void *user);

	struct mp_fs_sd_s {
		mp_kernel_t *kernel;
		mp_spi_bus_t bus;
		mp_gpio_port_t *cs;
		/** NULL when the board did not wire a card-detect pin */
		mp_gpio_port_t *cd;
		mp_fs_block_t block;
		char product[6];
		unsigned long serial;
		int block_address;
		/** YES after a probe identified the card. The pin can still say empty. */
		mp_bool_t ready;
		/** NULL until the owner sets it. Not called from init. */
		mp_fs_sd_media_t on_media;
		void *on_media_user;
	};

	mp_ret_t mp_fs_sd_init(mp_kernel_t *kernel, mp_fs_sd_t *card, mp_options_t *options, char *who);
	void mp_fs_sd_fini(mp_fs_sd_t *card);
	/**
	 * YES when a card is in the socket. The cd pin wins when it is wired:
	 * low is present. Without the pin, YES means the last probe identified
	 * a card and no transfer has since gone unanswered.
	 */
	mp_bool_t mp_fs_sd_present(mp_fs_sd_t *card);
	/**
	 * Identify the card, or report the one already identified.
	 * A high cd pin skips the bus and clears a previous identification.
	 * FALSE when the socket is empty or the card does not answer.
	 */
	mp_ret_t mp_fs_sd_probe(mp_fs_sd_t *card);

#endif

#endif
