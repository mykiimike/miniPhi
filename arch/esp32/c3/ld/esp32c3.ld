/* ESP32-C3. The ROM at flash offset 0 copies RAM segments and jumps to
 * the entry; it does not leave the flash cache mapped for this image.
 * The 384 KB SRAM is one bank with two addresses: instruction at
 * 0x40380000 and data at 0x3FC80000, 0x700000 apart. Code takes the low
 * 256 KB. Data takes the high 128 KB, so the two windows do not alias.
 * .bss is zeroed in startup. The ROM stack stays at the top of DRAM,
 * which this image does not load.
 */

ENTRY(_start)

MEMORY
{
	iram (xrw) : ORIGIN = 0x40380000, LENGTH = 0x40000
	dram (rw)  : ORIGIN = 0x3FCC0000, LENGTH = 0x20000
}

SECTIONS
{
	.text : {
		. = ALIGN(4);
		KEEP(*(.init))
		*(.text .text.*)
		. = ALIGN(4);
	} > iram

	.vectors : ALIGN(256) {
		KEEP(*(.vectors))
	} > iram

	.rodata : {
		. = ALIGN(4);
		*(.rodata .rodata.*)
		*(.srodata .srodata.*)
		. = ALIGN(4);
	} > dram

	.data : {
		. = ALIGN(4);
		_sdata = .;
		*(.data .data.*)
		*(.sdata .sdata.*)
		. = ALIGN(4);
		_edata = .;
	} > dram

	_sidata = LOADADDR(.data);

	.bss (NOLOAD) : {
		. = ALIGN(4);
		_sbss = .;
		*(.bss .bss.*)
		*(.sbss .sbss.*)
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
