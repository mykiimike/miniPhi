/* ESP32-C6 HP core. The ROM at flash offset 0 copies RAM segments and
 * jumps to the entry; it does not map the flash cache. Code and constants
 * therefore live in HP SRAM, which the core can execute. .bss is zeroed
 * in startup. The top of HP SRAM stays free for the ROM stack.
 */

ENTRY(_start)

MEMORY
{
	sram (rwx) : ORIGIN = 0x40800000, LENGTH = 0x6E000
}

SECTIONS
{
	.text : {
		. = ALIGN(4);
		KEEP(*(.init))
		*(.text .text.*)
		*(.rodata .rodata.*)
		*(.srodata .srodata.*)
		. = ALIGN(4);
	} > sram

	.data : {
		. = ALIGN(4);
		_sdata = .;
		*(.data .data.*)
		*(.sdata .sdata.*)
		. = ALIGN(4);
		_edata = .;
	} > sram

	_sidata = LOADADDR(.data);

	.bss (NOLOAD) : {
		. = ALIGN(4);
		_sbss = .;
		*(.bss .bss.*)
		*(.sbss .sbss.*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} > sram

	.heap (NOLOAD) : {
		. = ALIGN(16);
		end = .;
		_end = .;
		. = . + __heap_size;
	} > sram

	.stack (NOLOAD) : {
		. = ALIGN(16);
		. = . + __stack_size;
		. = ALIGN(16);
		__stack_top = .;
	} > sram

	ASSERT(__stack_top <= ORIGIN(sram) + LENGTH(sram), "HP SRAM overflow")
}
