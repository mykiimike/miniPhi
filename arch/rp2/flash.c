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
@defgroup mpArchRP2350Flash QSPI flash

@ingroup mpArchRP2350

The external flash is memory-mapped at 0x10000000, so a read is a copy.
Erase and program call the bootrom (datasheet 5.4). flash_exit_xip leaves
QMI in direct mode, so the instructions that follow it cannot be fetched
from the image. That stretch is copied to RAM with .data. Interrupts are
off across the call, because a handler fetched from XIP would fault while
QMI is in direct mode.

The chip has to be in serial-command state first (connect, then
flash_exit_xip). Afterwards the XIP setup routine the bootrom left in boot
RAM is copied to SRAM (boot RAM is execute-never) and called, which puts
back the read mode found at boot. flash_enter_cmd_xip is the slow fallback.

The bootrom programs whole 256-byte pages and erases multiples of 4 KB.
A shorter write is merged into the page that holds it.

@{
*/

static mp_rom_void_fn _connect, _exit_xip, _flush, _enter_cmd, _xip_fn;
static mp_rom_erase_fn _erase_fn;
static mp_rom_program_fn _prog_fn;
static int _rom_ok, _xip_checked;
static unsigned long _xip_words[MP_ROM_XIP_SETUP_LEN / sizeof(unsigned long)];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static void *_mp_rom_lookup(unsigned long code, unsigned long flags) {
	unsigned long entry;
	mp_rom_lookup_fn lookup;
	unsigned char version;

	version = *(volatile unsigned char *)(uintptr_t)MP_ROM_VERSION_ADDR;
	if(version == 1)
		entry = *(volatile unsigned long *)(uintptr_t)MP_ROM_LOOKUP_V1;
	else
		entry = *(volatile unsigned short *)(uintptr_t)MP_ROM_LOOKUP;
	if(entry == 0 || entry >= 0x10000UL)
		return(NULL);
	lookup = (mp_rom_lookup_fn)entry;
	return(lookup(code, flags));
}
#pragma GCC diagnostic pop

static void *_mp_rom_func(unsigned long code) {
	void *fn;

	fn = _mp_rom_lookup(code, MP_ROM_FLAG_ARM_SEC);
	if(!fn)
		fn = _mp_rom_lookup(code, MP_ROM_FLAG_ARM_NONSEC);
	return(fn);
}

/* Datasheet 5.4.8.24, reboot(flags, delay_ms, p0, p1): type BOOTSEL (2),
 * no return. p0 and p1 0: no activity LED, both USB interfaces. */
void mp_flash_bootsel(void) {
	int (*reboot)(unsigned long, unsigned long, unsigned long, unsigned long);

	reboot = (int (*)(unsigned long, unsigned long, unsigned long, unsigned long))_mp_rom_func(MP_ROM_REBOOT);
	if(!reboot)
		return;
	reboot(0x0102UL, 10UL, 0UL, 0UL);
	/* the request is in the watchdog scratch registers, but the reset
	 * does not always follow: fire it */
	WATCHDOG_CTRL |= WATCHDOG_CTRL_TRIGGER;
	for(;;)
		;
}

/* Datasheet 5.2.7: the first 256 bytes of boot RAM are the XIP setup
 * routine. 'X','F' is a pointer to it (ROM_DATA_SAVED_XIP_SETUP_FUNC_PTR). */
static void _mp_flash_capture_xip(void) {
	void *slot;
	unsigned long addr, n, i;
	volatile unsigned char *src;
	unsigned char *dst;

	if(_xip_checked)
		return;
	_xip_checked = 1;

	addr = MP_ROM_BOOTRAM;
	slot = _mp_rom_lookup(MP_ROM_XIP_SETUP, MP_ROM_FLAG_DATA);
	if(slot) {
		unsigned long fn = *(volatile unsigned long *)slot & ~1UL;

		if(fn >= MP_ROM_BOOTRAM && fn < MP_ROM_BOOTRAM + MP_ROM_BOOTRAM_SIZE)
			addr = fn;
	}

	n = MP_ROM_XIP_SETUP_LEN;
	if(addr + n > MP_ROM_BOOTRAM + MP_ROM_BOOTRAM_SIZE)
		n = MP_ROM_BOOTRAM + MP_ROM_BOOTRAM_SIZE - addr;

	src = (volatile unsigned char *)addr;
	dst = (unsigned char *)_xip_words;
	for(i = 0; i < n; i++)
		dst[i] = src[i];
	for(; i < MP_ROM_XIP_SETUP_LEN; i++)
		dst[i] = 0;

	__asm volatile("dsb" ::: "memory");
	__asm volatile("isb" ::: "memory");

	if(_xip_words[0] != 0 && _xip_words[0] != 0xffffffffUL)
		_xip_fn = (mp_rom_void_fn)((uintptr_t)_xip_words | 1UL);
}

static mp_ret_t _mp_rom_open(void) {
	if(_rom_ok)
		return(TRUE);

	_mp_flash_capture_xip();
	_connect = (mp_rom_void_fn)_mp_rom_func(MP_ROM_CONNECT);
	_exit_xip = (mp_rom_void_fn)_mp_rom_func(MP_ROM_EXIT_XIP);
	_erase_fn = (mp_rom_erase_fn)_mp_rom_func(MP_ROM_ERASE);
	_prog_fn = (mp_rom_program_fn)_mp_rom_func(MP_ROM_PROGRAM);
	_flush = (mp_rom_void_fn)_mp_rom_func(MP_ROM_FLUSH);
	_enter_cmd = (mp_rom_void_fn)_mp_rom_func(MP_ROM_ENTER_XIP);
	if(!_connect || !_exit_xip || !_erase_fn || !_prog_fn || !_flush)
		return(FALSE);
	_rom_ok = 1;
	return(TRUE);
}

/* The pointers live in the caller's stack frame. Nothing in here may touch
 * the XIP window: flash_exit_xip has turned it off. */
struct _mp_flash_rom {
	mp_rom_void_fn connect;
	mp_rom_void_fn exit_xip;
	mp_rom_void_fn flush;
	mp_rom_void_fn enter_cmd;
	mp_rom_void_fn xip_fn;
	mp_rom_erase_fn erase;
	mp_rom_program_fn program;
};

__attribute__((noinline, long_call, aligned(4), section(".data.ramfunc")))
static void _mp_flash_xip_on(struct _mp_flash_rom *rom) {
	/* flash_enter_cmd_xip lives in ROM and turns the window back on.
	 * The boot-RAM setup copy is not relocatable: calling it from SRAM
	 * faults while XIP is still off, and the fault vector is in flash. */
	if(rom->enter_cmd)
		rom->enter_cmd();
	else if(rom->xip_fn)
		rom->xip_fn();
	if(rom->flush)
		rom->flush();
}

__attribute__((noinline, long_call, aligned(4), section(".data.ramfunc")))
static void _mp_flash_erase_ram(struct _mp_flash_rom *rom, unsigned long chip, unsigned long size) {
	rom->connect();
	rom->exit_xip();
	rom->erase(chip, size, MP_FLASH_BLOCK_SIZE, (unsigned char)MP_FLASH_BLOCK_CMD);
	rom->flush();
	_mp_flash_xip_on(rom);
}

__attribute__((noinline, long_call, aligned(4), section(".data.ramfunc")))
static void _mp_flash_program_ram(struct _mp_flash_rom *rom, unsigned long at, const unsigned char *page) {
	rom->connect();
	rom->exit_xip();
	rom->program(at, page, MP_FLASH_PROG_SIZE);
	rom->flush();
	_mp_flash_xip_on(rom);
}

static void _mp_flash_rom_fill(struct _mp_flash_rom *rom) {
	rom->connect = _connect;
	rom->exit_xip = _exit_xip;
	rom->flush = _flush;
	rom->enter_cmd = _enter_cmd;
	rom->xip_fn = _xip_fn;
	rom->erase = _erase_fn;
	rom->program = _prog_fn;
}

static mp_ret_t _mp_flash_span(mp_flash_t *flash, mp_flash_addr_t offset, int size, unsigned long *chip) {
	unsigned long abs;

	if(!flash || !chip || size <= 0)
		return(FALSE);
	if(offset > flash->size || (unsigned int)size > flash->size - offset)
		return(FALSE);
	abs = flash->base + offset;
	if(abs < flash->base || abs < MP_FLASH_ORIGIN)
		return(FALSE);
	if(abs + (unsigned long)size < abs)
		return(FALSE);
	if(abs + (unsigned long)size > MP_FLASH_ORIGIN + MP_FLASH_SIZE)
		return(FALSE);
	*chip = abs - MP_FLASH_ORIGIN;
	return(TRUE);
}

/* One writer at a time. The other core waits in RAM before this one masks
 * itself: a core spinning on the kernel lock would never take the park. */
#if MP_CPUS > 1
static mp_cpu_mutex_t _mp_flash_mutex = MP_CPU_MUTEX_INIT;
static mp_task_lock_t _mp_flash_key;

static mp_ret_t _mp_flash_hold(void) {
	mp_task_lock_t key = mp_task_mutex_lock(&_mp_flash_mutex);

	if(mp_cpu_park() != YES) {
		mp_task_mutex_unlock(&_mp_flash_mutex, key);
		return(FALSE);
	}
	_mp_flash_key = key;
	return(TRUE);
}

static void _mp_flash_release(void) {
	mp_cpu_unpark();
	mp_task_mutex_unlock(&_mp_flash_mutex, _mp_flash_key);
}
#else
static mp_ret_t _mp_flash_hold(void) {
	return(TRUE);
}

static void _mp_flash_release(void) {
}
#endif

static mp_ret_t _mp_flash_erase_at(unsigned long chip, unsigned long size) {
	struct _mp_flash_rom rom;
	mp_ret_t ret = FALSE;

	if(size == 0 || (chip & (MP_FLASH_PAGE_SIZE - 1)) || (size & (MP_FLASH_PAGE_SIZE - 1)))
		return(FALSE);
	if(chip > MP_FLASH_SIZE || size > MP_FLASH_SIZE - chip)
		return(FALSE);
	if(_mp_flash_hold() == FALSE)
		return(FALSE);

	MP_INTERRUPT_SAFE_BEGIN
	if(_mp_rom_open() == TRUE) {
		_mp_flash_rom_fill(&rom);
		_mp_flash_erase_ram(&rom, chip, size);
		ret = TRUE;
	}
	MP_INTERRUPT_SAFE_END
	_mp_flash_release();
	return(ret);
}

static mp_ret_t _mp_flash_program_at(unsigned long chip, const unsigned char *data, int size) {
	unsigned char page[MP_FLASH_PROG_SIZE] __attribute__((aligned(4)));
	unsigned long first, last, at;
	mp_ret_t ret = FALSE;

	if(!data || size <= 0)
		return(FALSE);
	if(chip > MP_FLASH_SIZE || (unsigned long)size > MP_FLASH_SIZE - chip)
		return(FALSE);

	first = chip & ~(MP_FLASH_PROG_SIZE - 1);
	last = (chip + (unsigned long)size - 1) & ~(MP_FLASH_PROG_SIZE - 1);
	if(_mp_flash_hold() == FALSE)
		return(FALSE);

	MP_INTERRUPT_SAFE_BEGIN
	if(_mp_rom_open() == TRUE) {
		struct _mp_flash_rom rom;

		_mp_flash_rom_fill(&rom);
		for(at = first; at <= last; at += MP_FLASH_PROG_SIZE) {
			unsigned long lo, hi, page_hi;

			/* Read the page while XIP is still on. */
			memcpy(page, (const void *)(MP_FLASH_ORIGIN + at), MP_FLASH_PROG_SIZE);
			page_hi = at + MP_FLASH_PROG_SIZE;
			lo = chip > at ? chip : at;
			hi = chip + (unsigned long)size;
			if(hi > page_hi)
				hi = page_hi;
			memcpy(page + (lo - at), data + (lo - chip), (size_t)(hi - lo));
			__asm volatile("" ::: "memory");
			_mp_flash_program_ram(&rom, at, page);
		}
		ret = TRUE;
	}
	MP_INTERRUPT_SAFE_END
	_mp_flash_release();
	return(ret);
}

mp_ret_t mp_flash_init(mp_flash_t *flash, mp_flash_addr_t base, unsigned int size) {
	if(!flash || size == 0)
		return(FALSE);
	if(base % MP_FLASH_PAGE_SIZE || size % MP_FLASH_PAGE_SIZE)
		return(FALSE);
	if(base < MP_FLASH_ORIGIN || base + size < base)
		return(FALSE);
	if(base + size > MP_FLASH_ORIGIN + MP_FLASH_SIZE)
		return(FALSE);
	flash->base = base;
	flash->size = size;
	return(TRUE);
}

mp_ret_t mp_flash_fini(mp_flash_t *flash) {
	(void)flash;
	return(TRUE);
}

mp_ret_t mp_flash_erase(mp_flash_t *flash) {
	if(!flash || flash->size == 0)
		return(FALSE);
	if(flash->base < MP_FLASH_ORIGIN || flash->base + flash->size < flash->base)
		return(FALSE);
	if(flash->base + flash->size > MP_FLASH_ORIGIN + MP_FLASH_SIZE)
		return(FALSE);
	return(_mp_flash_erase_at(flash->base - MP_FLASH_ORIGIN, flash->size));
}

mp_ret_t mp_flash_read(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *in_mem) {
	if(!flash || !in_mem || size <= 0)
		return(FALSE);
	if(offset > flash->size || (unsigned int)size > flash->size - offset)
		return(FALSE);
	memcpy(in_mem, (const void *)(flash->base + offset), (size_t)size);
	return(TRUE);
}

mp_ret_t mp_flash_write(mp_flash_t *flash, mp_flash_addr_t offset, int size, void *out_mem) {
	unsigned long chip, first, last;

	if(_mp_flash_span(flash, offset, size, &chip) == FALSE || !out_mem)
		return(FALSE);
	first = chip & ~(MP_FLASH_PAGE_SIZE - 1);
	last = (chip + (unsigned long)size - 1) & ~(MP_FLASH_PAGE_SIZE - 1);
	if(_mp_flash_erase_at(first, last - first + MP_FLASH_PAGE_SIZE) == FALSE)
		return(FALSE);
	return(_mp_flash_program_at(chip, out_mem, size));
}

mp_ret_t mp_flash_program(mp_flash_t *flash, mp_flash_addr_t offset, int size, const void *data) {
	unsigned long chip;

	if(_mp_flash_span(flash, offset, size, &chip) == FALSE || !data)
		return(FALSE);
	return(_mp_flash_program_at(chip, data, size));
}

extern uint32_t _sidata, _sdata, _edata;

mp_flash_addr_t mp_flash_image_end(void) {
	return((mp_flash_addr_t)&_sidata + ((mp_flash_addr_t)&_edata - (mp_flash_addr_t)&_sdata));
}

/**@}*/
