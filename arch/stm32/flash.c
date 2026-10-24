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

/**
@defgroup mpArchSTM32Flash Internal flash

@ingroup mpArchSTM32

The CPU stalls while a page is erased or written, so keep the region
away from the code and call these outside of interrupt handlers.

@{
*/

#if defined(STM32F407xx)
static int __flashSector = -1;
#endif

static void _mp_flash_unlock(void) {
#if defined(STM32F407xx)
	__flashSector = -1;
#endif
	if(FLASH->CR & FLASH_CR_LOCK) {
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}
}

static void _mp_flash_lock(void) {
	FLASH->CR |= FLASH_CR_LOCK;
}

static mp_ret_t _mp_flash_wait(void) {
	uint32_t sr;

	while(FLASH->SR & FLASH_SR_BSY);
	sr = FLASH->SR;
#if defined(STM32F407xx)
	FLASH->SR = FLASH_SR_EOP | FLASH_SR_ERR;
	return(sr & FLASH_SR_ERR ? FALSE : TRUE);
#else
	FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
	return(sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR) ? FALSE : TRUE);
#endif
}

#if defined(STM32F407xx)
/* sectors 0-3 are 16 KB, 4 is 64 KB, 5-11 are 128 KB */
static int _mp_flash_sector(mp_flash_addr_t address) {
	unsigned long off = address - MP_FLASH_ORIGIN;

	if(off < 0x10000UL)
		return((int)(off / 0x4000UL));
	if(off < 0x20000UL)
		return(4);
	return(5 + (int)((off - 0x20000UL) / 0x20000UL));
}
#endif

static mp_ret_t _mp_flash_erase_page(mp_flash_addr_t address) {
	mp_ret_t ret;

#if defined(STM32F407xx)
	int sector = _mp_flash_sector(address);

	/* a 16 KB step can land in a sector that was just erased */
	if(sector == __flashSector)
		return(TRUE);
	__flashSector = sector;

	FLASH->CR = (FLASH->CR & ~(FLASH_CR_SNB | FLASH_CR_PSIZE)) | FLASH_CR_SER | FLASH_CR_PSIZE_16 | ((uint32_t)sector << 3);
	FLASH->CR |= FLASH_CR_STRT;
	ret = _mp_flash_wait();
	FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_PSIZE);
#else
	FLASH->CR |= FLASH_CR_PER;
	FLASH->AR = address;
	FLASH->CR |= FLASH_CR_STRT;
	ret = _mp_flash_wait();
	FLASH->CR &= ~FLASH_CR_PER;
#endif
	return(ret);
}

mp_ret_t mp_flash_init(mp_flash_t *flash, mp_flash_addr_t base, unsigned int size) {
	if(base % MP_FLASH_PAGE_SIZE || size % MP_FLASH_PAGE_SIZE)
		return(FALSE);
	flash->base = base;
	flash->size = size;
	return(TRUE);
}

mp_ret_t mp_flash_fini(mp_flash_t *flash) {
	return(TRUE);
}

mp_ret_t mp_flash_erase(mp_flash_t *flash) {
	mp_flash_addr_t address;
	mp_ret_t ret = TRUE;

	MP_INTERRUPT_SAFE_BEGIN
	_mp_flash_unlock();
	for(address = flash->base; address < flash->base + flash->size; address += MP_FLASH_PAGE_SIZE)
		if(_mp_flash_erase_page(address) == FALSE)
			ret = FALSE;
	_mp_flash_lock();
	MP_INTERRUPT_SAFE_END

	return(ret);
}

mp_ret_t mp_flash_read(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *in_mem) {
	if(size < 0 || offset + size > flash->size)
		return(FALSE);
	memcpy(in_mem, (const void *)(flash->base + offset), size);
	return(TRUE);
}

/* unlocked, interrupts off; an odd tail is padded with 0xFF */
static mp_ret_t _mp_flash_program(mp_flash_addr_t address, const unsigned char *data, int size) {
	mp_ret_t ret = TRUE;
	uint16_t half;
	int a;

#if defined(STM32F407xx)
	FLASH->CR = (FLASH->CR & ~FLASH_CR_PSIZE) | FLASH_CR_PG | FLASH_CR_PSIZE_16;
#else
	FLASH->CR |= FLASH_CR_PG;
#endif
	for(a=0; a<size && ret == TRUE; a+=2) {
		half = data[a];
		half |= (a + 1 < size ? data[a + 1] : 0xFF) << 8;
		*(volatile uint16_t *)(address + a) = half;
		ret = _mp_flash_wait();
	}
#if defined(STM32F407xx)
	FLASH->CR &= ~(FLASH_CR_PG | FLASH_CR_PSIZE);
#else
	FLASH->CR &= ~FLASH_CR_PG;
#endif
	return(ret);
}

mp_ret_t mp_flash_write(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *out_mem) {
	const unsigned char *data = out_mem;
	mp_flash_addr_t first, last, address;
	mp_ret_t ret = TRUE;

	if(size <= 0 || offset & 1 || offset + size > flash->size)
		return(FALSE);

	first = (flash->base + offset) & ~(MP_FLASH_PAGE_SIZE - 1);
	last = (flash->base + offset + size - 1) & ~(MP_FLASH_PAGE_SIZE - 1);

	MP_INTERRUPT_SAFE_BEGIN

	_mp_flash_unlock();

	for(address = first; address <= last; address += MP_FLASH_PAGE_SIZE)
		if(_mp_flash_erase_page(address) == FALSE)
			ret = FALSE;

	if(ret == TRUE)
		ret = _mp_flash_program(flash->base + offset, data, size);

	_mp_flash_lock();

	MP_INTERRUPT_SAFE_END

	return(ret);
}

mp_ret_t mp_flash_program(mp_flash_t *flash, mp_flash_addr_t offset, int size, const void *data) {
	mp_ret_t ret;

	if(size <= 0 || offset & 1 || offset + size > flash->size)
		return(FALSE);

	MP_INTERRUPT_SAFE_BEGIN
	_mp_flash_unlock();
	ret = _mp_flash_program(flash->base + offset, data, size);
	_mp_flash_lock();
	MP_INTERRUPT_SAFE_END

	return(ret);
}

extern uint32_t _sidata, _sdata, _edata;

mp_flash_addr_t mp_flash_image_end(void) {
	return((mp_flash_addr_t)&_sidata + ((mp_flash_addr_t)&_edata - (mp_flash_addr_t)&_sdata));
}

/**@}*/
