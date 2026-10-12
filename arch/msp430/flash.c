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

#include <mp.h>

#ifdef __MSP430_HAS_FLASH__

mp_ret_t mp_flash_init(mp_flash_t *flash, mp_flash_addr_t base, unsigned int size) {
	flash->base = base;
	flash->size = size;
	return(TRUE);
}

mp_ret_t mp_flash_fini(mp_flash_t *flash) {
	/* nothing */
	return(TRUE);
}

mp_ret_t mp_flash_erase(mp_flash_t *flash) {
	int a;

	FCTL3 = FWKEY; // Clear Lock bit

	FCTL1 = FWKEY+ERASE; // Set Erase bit
	__data20_write_char(flash->base, 0);
	FCTL1 = FWKEY + WRT; // Set WRT bit for write operation

	for(a=0; a<flash->size; a++) {
		__data20_write_char(flash->base+a, 0);
	}

	FCTL1 = FWKEY;
	FCTL3 = FWKEY+LOCK;
	return(TRUE);
}

mp_ret_t mp_flash_read(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *in_mem) {
	mp_flash_addr_t addr = flash->base+offset;
	unsigned char *ptr;
	int a;

	ptr = (unsigned char *)in_mem;
	for(a=0; a<size; a++, ptr++)
		*ptr = __data20_read_char(addr+a);

	return(TRUE);
}

mp_ret_t mp_flash_write(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *out_mem) {
	mp_flash_addr_t addr = flash->base+offset;
	unsigned char *ptr;
	int a;

	FCTL3 = FWKEY; // Clear Lock bit

	FCTL1 = FWKEY+ERASE; // Set Erase bit
	__data20_write_char(addr, 0);
	FCTL1 = FWKEY + WRT; // Set WRT bit for write operation

	ptr = (unsigned char *)out_mem;
	for(a=0; a<size; a++, ptr++) {
		__data20_write_char(addr+a, *ptr);
	}

	FCTL1 = FWKEY;
	FCTL3 = FWKEY+LOCK;

	return(TRUE);
}

#endif


