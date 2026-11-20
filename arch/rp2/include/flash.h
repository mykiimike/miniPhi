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

#ifndef _HAVE_RP2350_FLASH_H
	#define _HAVE_RP2350_FLASH_H

	/* W25Q128: KFLASH banks are one 4 KB erase sector. The bootrom programs
	 * 256 bytes at a time, and the chip is 16 MB. */
	#define MP_FLASH_PAGE_SIZE 4096UL
	#define MP_FLASH_PROG_SIZE 256UL
	#define MP_FLASH_SIZE (16UL * 1024UL * 1024UL)

	/* execute-in-place window. The bootrom maps the external QSPI flash here. */
	#define MP_FLASH_ORIGIN RP2350_XIP_BASE

	/* RP2350 bootrom, datasheet 5.5.1 "Locating the API functions", and the
	 * Pico SDK bootrom_constants.h ROM_TABLE_CODE / RT_FLAG values.
	 * 0x14 holds a 16-bit pointer to the function table. The lookup routine
	 * is a 16-bit pointer at 0x16 (bootrom version 2 and later). Version 1
	 * stores a 32-bit pointer at 0x18. The version byte is at 0x13.
	 * Codes are two ASCII bytes, little-endian: 'I','F' is connect. */
	#define MP_ROM_VERSION_ADDR 0x13UL
	#define MP_ROM_FUNC_TABLE   0x14UL
	#define MP_ROM_LOOKUP       0x16UL
	#define MP_ROM_LOOKUP_V1    0x18UL
	#define MP_ROM_CODE(a, b) ((unsigned long)(unsigned char)(a) | ((unsigned long)(unsigned char)(b) << 8))
	#define MP_ROM_FLAG_ARM_SEC    0x0004UL
	#define MP_ROM_FLAG_ARM_NONSEC 0x0010UL
	#define MP_ROM_FLAG_DATA       0x0040UL
	#define MP_ROM_CONNECT   MP_ROM_CODE('I', 'F')
	#define MP_ROM_EXIT_XIP  MP_ROM_CODE('E', 'X')
	#define MP_ROM_ERASE     MP_ROM_CODE('R', 'E')
	#define MP_ROM_PROGRAM   MP_ROM_CODE('R', 'P')
	#define MP_ROM_FLUSH     MP_ROM_CODE('F', 'C')
	#define MP_ROM_ENTER_XIP MP_ROM_CODE('C', 'X')
	#define MP_ROM_XIP_SETUP MP_ROM_CODE('X', 'F')
	#define MP_ROM_REBOOT    MP_ROM_CODE('R', 'B')

	/** Restarts into the bootrom USB mass storage and picotool interface. */
	void mp_flash_bootsel(void);

	/* Boot RAM is 1 KB at 0x400e0000 (RP2350 address map). Datasheet 5.2.7
	 * leaves the XIP setup routine in the first 256 bytes. That window is
	 * execute-never, so the routine is copied to SRAM before it is called.
	 * A 64 KB block erase is command 0xD8; the bootrom uses 4 KB sectors
	 * when the range is smaller (Pico SDK FLASH_BLOCK_SIZE / 0xD8). */
	#define MP_ROM_BOOTRAM       0x400e0000UL
	#define MP_ROM_BOOTRAM_SIZE  0x400UL
	#define MP_ROM_XIP_SETUP_LEN 256UL
	#define MP_FLASH_BLOCK_SIZE  65536UL
	#define MP_FLASH_BLOCK_CMD   0xD8UL

	typedef void (*mp_rom_void_fn)(void);
	typedef void (*mp_rom_erase_fn)(unsigned long addr, unsigned long count, unsigned long block_size, unsigned char block_cmd);
	typedef void (*mp_rom_program_fn)(unsigned long addr, const unsigned char *data, unsigned long count);
	typedef void *(*mp_rom_lookup_fn)(unsigned long code, unsigned long flags);

	typedef struct mp_flash_s mp_flash_t;
	typedef unsigned long mp_flash_addr_t;

	struct mp_flash_s {
		mp_flash_addr_t base;
		unsigned int size;
	};

	mp_ret_t mp_flash_init(mp_flash_t *flash, mp_flash_addr_t base, unsigned int size);
	mp_ret_t mp_flash_fini(mp_flash_t *flash);
	mp_ret_t mp_flash_erase(mp_flash_t *flash);
	mp_ret_t mp_flash_read(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *in_mem);

	/* erases the 4 KB sectors it touches, then programs */
	mp_ret_t mp_flash_write(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *out_mem);

	/* programs bytes without erasing: a 0 bit stays 0. Short writes are merged
	 * into the 256-byte page that holds them. */
	mp_ret_t mp_flash_program(mp_flash_t *flash, mp_flash_addr_t offset, int size, const void *data);
	mp_flash_addr_t mp_flash_image_end(void);

#endif
