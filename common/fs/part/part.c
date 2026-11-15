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

#ifdef SUPPORT_COMMON_FS_PART

static unsigned long _mp_fs_part_u32(const unsigned char *p) {
	return((unsigned long)p[0] | ((unsigned long)p[1] << 8) |
		((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24));
}

static mp_ret_t _mp_fs_part_range(mp_fs_part_t *part, unsigned long sector, unsigned long count) {
	if(!part || !part->parent || !count)
		return(FALSE);
	if(sector >= part->count || count > part->count - sector)
		return(FALSE);
	return(TRUE);
}

static mp_ret_t _mp_fs_part_read(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count) {
	mp_fs_part_t *part = block ? block->user : NULL;

	if(_mp_fs_part_range(part, sector, count) != TRUE)
		return(FALSE);
	return(mp_fs_block_read(part->parent, part->first + sector, data, count));
}

static mp_ret_t _mp_fs_part_write(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count) {
	mp_fs_part_t *part = block ? block->user : NULL;

	if(_mp_fs_part_range(part, sector, count) != TRUE)
		return(FALSE);
	return(mp_fs_block_write(part->parent, part->first + sector, data, count));
}

static void _mp_fs_part_bind(mp_fs_part_t *part, mp_fs_block_t *disk, unsigned long first, unsigned long count, unsigned char type) {
	memset(part, 0, sizeof(*part));
	part->parent = disk;
	part->first = first;
	part->count = count;
	part->type = type;
	part->block.user = part;
	part->block.size = disk->size;
	part->block.sectors = count;
	part->block.read = _mp_fs_part_read;
	part->block.write = _mp_fs_part_write;
}

int mp_fs_part_gpt(mp_fs_block_t *disk, mp_fs_part_t *parts, int max) {
	(void)disk;
	(void)parts;
	(void)max;
	return(MP_FS_PART_GPT);
}

int mp_fs_part_scan(mp_fs_block_t *disk, mp_fs_part_t *parts, int max) {
	unsigned char sector[512];
	struct {
		unsigned long first;
		unsigned long count;
		unsigned char type;
	} slot[4];
	int i, n;

	if(!disk || disk->size != 512 || !disk->read)
		return(MP_FS_PART_ERROR);
	if(max < 0 || (max > 0 && !parts))
		return(MP_FS_PART_ERROR);
	if(mp_fs_block_read(disk, 0, sector, 1) != TRUE)
		return(MP_FS_PART_ERROR);
	if(sector[510] != 0x55 || sector[511] != 0xAA)
		return(0);

	n = 0;
	for(i = 0; i < 4; i++) {
		const unsigned char *entry = sector + 446 + i * 16;
		unsigned char type = entry[4];
		unsigned long first = _mp_fs_part_u32(entry + 8);
		unsigned long count = _mp_fs_part_u32(entry + 12);

		if(type == 0xEE)
			return(mp_fs_part_gpt(disk, parts, max));
		if(type == 0 || first == 0 || count == 0)
			continue;
		if(first + count < first)
			continue;
		if(disk->sectors && first + count > disk->sectors)
			continue;
		slot[n].first = first;
		slot[n].count = count;
		slot[n].type = type;
		n++;
	}
	for(i = 0; i < n && i < max; i++)
		_mp_fs_part_bind(&parts[i], disk, slot[i].first, slot[i].count, slot[i].type);
	return(n);
}

#endif
