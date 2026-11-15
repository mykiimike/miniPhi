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

#ifdef SUPPORT_COMMON_FS_PART

#ifndef _HAVE_MP_COMMON_FS_PART_H
	#define _HAVE_MP_COMMON_FS_PART_H

	/**
	 * One primary partition, as a block over a window of its parent.
	 * The caller owns the struct. block is the face: its size is the
	 * parent's, its sector count is the window, and its user is this struct.
	 * first is the parent's sector where the window starts. type is the
	 * MBR type byte. read and write add first and refuse a range past count.
	 */

	#include <fs/fs.h>

	/** The sector could not be read, or the arguments are unusable. */
	#define MP_FS_PART_ERROR (-1)
	/** Protective MBR. GPT is not parsed. */
	#define MP_FS_PART_GPT (-2)

	typedef struct mp_fs_part_s mp_fs_part_t;

	struct mp_fs_part_s {
		mp_fs_block_t block;
		mp_fs_block_t *parent;
		unsigned long first;
		unsigned long count;
		unsigned char type;
	};

	/**
	 * Read the four primary entries in sector 0.
	 * A 512-byte sector is required. Type 0, a zero start and a zero length
	 * are skipped. An entry whose end lies past a known parent sector count,
	 * or whose start and length overflow, is skipped. When the parent sector
	 * count is 0 the end is not checked.
	 * At most max entries are written. The return is how many usable entries
	 * the table holds, which may be greater than max. 0 is no signature or
	 * no usable entry: a superfloppy, mounted as the disk itself.
	 * One type 0xEE entry is a protective MBR. Nothing is written and the
	 * return is mp_fs_part_gpt().
	 */
	int mp_fs_part_scan(mp_fs_block_t *disk, mp_fs_part_t *parts, int max);

	/**
	 * Protective MBR. Returns MP_FS_PART_GPT. A GPT parser replaces this.
	 */
	int mp_fs_part_gpt(mp_fs_block_t *disk, mp_fs_part_t *parts, int max);

#endif

#endif
