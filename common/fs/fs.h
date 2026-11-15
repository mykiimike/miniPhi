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

#ifdef SUPPORT_COMMON_FS

#ifndef _HAVE_MP_COMMON_FS_H
	#define _HAVE_MP_COMMON_FS_H

	#include <mp.h>

	/**
	 * One medium, addressed by sector.
	 * The caller owns the struct and every buffer passed in. size is the
	 * sector length in bytes. sectors is the count, or 0 when the medium has
	 * not reported it yet; a count of 0 skips the range check. read and write
	 * move count sectors. data holds count * size bytes. A FALSE write may
	 * have stored a prefix of the request.
	 */

	typedef struct mp_fs_block_s mp_fs_block_t;

	struct mp_fs_block_s {
		void *user;
		unsigned short size;
		unsigned long sectors;
		mp_ret_t (*read)(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count);
		mp_ret_t (*write)(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count);
	};

	mp_ret_t mp_fs_block_read(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count);
	mp_ret_t mp_fs_block_write(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count);

#endif

#endif
