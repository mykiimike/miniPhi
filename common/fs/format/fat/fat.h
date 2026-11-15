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

#ifdef SUPPORT_COMMON_FS_FORMAT_FAT

#ifndef _HAVE_MP_COMMON_FS_FORMAT_FAT_H
	#define _HAVE_MP_COMMON_FS_FORMAT_FAT_H

	/**
	 * FAT12, FAT16 and FAT32 over a 512-byte block device.
	 * The block is the volume: sector 0 is the boot sector, and a partition
	 * table is not followed. The caller owns mp_fs_fat_t for the life of the
	 * mount. It holds one sector of cache, so two volumes are two structs.
	 * A directory entry's name is UTF-8, cut on a character boundary when it
	 * does not fit.
	 * short_name is the 8.3 name with the dot. mp_fs_fat_read returns 0 at
	 * the end and -1 when the medium fails. mp_fs_fat_write replaces bytes
	 * inside the recorded size and returns 0 once the position is at that
	 * size; it does not allocate clusters.
	 */

	#include <fs/fs.h>

	#define MP_FS_FAT_NAME 256

	typedef struct mp_fs_fat_s mp_fs_fat_t;

	struct mp_fs_fat_s {
		mp_fs_block_t *block;
		/** 0. This block is the volume, so the boot sector is sector 0. */
		unsigned long start;
		unsigned char spc;
		unsigned char fats;
		/** 12, 16 or 32 */
		unsigned char kind;
		unsigned short root_entries;
		unsigned long spf;
		unsigned long fat_sector;
		unsigned long root_sector;
		unsigned long data_sector;
		unsigned long root_cluster;
		unsigned long clusters;
		unsigned long cache_at;
		int cache_ok;
		unsigned char cache[512];
	};

	typedef struct mp_fs_fat_ent_s {
		char name[MP_FS_FAT_NAME];
		char short_name[13];
		unsigned char attr;
		int dir;
		unsigned long size;
		unsigned long cluster;
	} mp_fs_fat_ent_t;

	typedef struct mp_fs_fat_dir_s {
		mp_fs_fat_t *fat;
		/** 0 is the FAT12/FAT16 root region. A cluster number otherwise. */
		unsigned long cluster;
		unsigned long index;
		int done;
	} mp_fs_fat_dir_t;

	typedef struct mp_fs_fat_file_s {
		mp_fs_fat_t *fat;
		unsigned long start;
		unsigned long size;
		unsigned long pos;
	} mp_fs_fat_file_t;

	mp_ret_t mp_fs_fat_mount(mp_fs_fat_t *fat, mp_fs_block_t *block);
	void mp_fs_fat_unmount(mp_fs_fat_t *fat);

	/** path NULL, "" or "/" is the root. The last component has to be a directory. */
	mp_ret_t mp_fs_fat_opendir(mp_fs_fat_t *fat, mp_fs_fat_dir_t *dir, const char *path);
	mp_ret_t mp_fs_fat_readdir(mp_fs_fat_dir_t *dir, mp_fs_fat_ent_t *ent);

	/** A directory is not a file. */
	mp_ret_t mp_fs_fat_open(mp_fs_fat_t *fat, mp_fs_fat_file_t *file, const char *path);
	int mp_fs_fat_read(mp_fs_fat_file_t *file, void *data, int len);
	int mp_fs_fat_write(mp_fs_fat_file_t *file, const void *data, int len);
	mp_ret_t mp_fs_fat_seek(mp_fs_fat_file_t *file, unsigned long pos);
	void mp_fs_fat_close(mp_fs_fat_file_t *file);

#endif

#endif
