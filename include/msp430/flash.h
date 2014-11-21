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

#ifndef _HAVE_MSP430_FLASH_H
	#define _HAVE_MSP430_FLASH_H

	#ifdef __MSP430_HAS_FLASH__
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
		mp_ret_t mp_flash_write(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *out_mem);
	#endif

#endif
