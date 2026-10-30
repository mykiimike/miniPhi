/* ESP32-S3 CPU0. The ROM at flash offset 0 copies RAM segments and jumps
 * to the entry; it does not map the flash cache. Code is fetched from the
 * IRAM alias. Data lives in the DRAM alias, which does not overlap that
 * code window. .bss is zeroed in startup. The ROM stack stays above DRAM.
 */

ENTRY(_start)

MEMORY
{
	iram (xrw) : ORIGIN = 0x40378000, LENGTH = 0x18000
	dram (rw)  : ORIGIN = 0x3FCA0000, LENGTH = 0x49000
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
