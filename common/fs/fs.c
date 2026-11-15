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

#include <mp.h>

#ifdef SUPPORT_COMMON_FS

static mp_ret_t _mp_fs_range(mp_fs_block_t *block, unsigned long sector, const void *data, unsigned long count) {
	if(!block || !data || !count || !block->size)
		return(FALSE);
	if(!block->sectors)
		return(TRUE);
	if(sector >= block->sectors || count > block->sectors - sector)
		return(FALSE);
	return(TRUE);
}

mp_ret_t mp_fs_block_read(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count) {
	if(_mp_fs_range(block, sector, data, count) != TRUE || !block->read)
		return(FALSE);
	return(block->read(block, sector, data, count));
}

mp_ret_t mp_fs_block_write(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count) {
	if(_mp_fs_range(block, sector, data, count) != TRUE || !block->write)
		return(FALSE);
	return(block->write(block, sector, data, count));
}

#endif
