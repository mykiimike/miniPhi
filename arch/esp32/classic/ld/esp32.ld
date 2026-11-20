/* ESP32 PRO CPU. The ROM at flash offset 0x1000 copies the RAM segments
 * and jumps to the entry. It does not map the flash cache. Code is fetched
 * from SRAM0. Data stays in SRAM2, below the ROM bootloader DRAM: the ROM
 * stack sentinel is 0x3ffe1320 and __stack is 0x3ffe3f20, and the ROM's
 * own data starts at 0x3ffe0000. .bss is zeroed in startup.
 */

ENTRY(_start)

MEMORY
{
	iram (xrw) : ORIGIN = 0x40080000, LENGTH = 0x20000
	dram (rw)  : ORIGIN = 0x3FFB0000, LENGTH = 0x30000
}

SECTIONS
{
	.text : {
		. = ALIGN(4);
		KEEP(*(.init))
		*(.literal .literal.*)
		*(.text .text.*)
		. = ALIGN(1024);
		KEEP(*(.vecbase))
		. = ALIGN(4);
	} > iram

	.data : {
		. = ALIGN(4);
		_sdata = .;
		*(.data .data.*)
		*(.rodata .rodata.*)
		. = ALIGN(4);
		_edata = .;
	} > dram

	_sidata = LOADADDR(.data);

	.bss (NOLOAD) : {
		. = ALIGN(4);
		_sbss = .;
		*(.bss .bss.*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} > dram

	.heap (NOLOAD) : {
		. = ALIGN(16);
		end = .;
		_end = .;
		. = . + __heap_size;
	} > dram

	.stack (NOLOAD) : {
		. = ALIGN(16);
		. = . + __stack_size;
		. = ALIGN(16);
		__stack_top = .;
	} > dram

	/* top of the DRAM window; the system page measures RAM from here */
	_estack = ORIGIN(dram) + LENGTH(dram);

	ASSERT(__stack_top <= ORIGIN(dram) + LENGTH(dram), "DRAM overflow")
}
