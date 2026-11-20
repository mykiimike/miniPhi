/* RP2350A: 16 MB QSPI flash at the XIP window, 520 KB SRAM.
 * __stack_size and __heap_size come from the Makefile (--defsym). */

ENTRY(Reset_Handler)

MEMORY
{
	FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 16M
	RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 520K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

PROVIDE(__stack_size = 8192);
PROVIDE(__heap_size = 8192);

SECTIONS
{
	/* 68 vectors want 512-byte alignment. The bootrom enters this table. */
	.isr_vector :
	{
		. = ALIGN(512);
		KEEP(*(.isr_vector))
		. = ALIGN(4);
	} > FLASH

	/* IMAGE_DEF. The bootrom scans the first 4 KB. Nothing in the program
	 * references it, so KEEP stops --gc-sections from dropping it. */
	.picobin :
	{
		. = ALIGN(4);
		KEEP(*(.picobin))
		. = ALIGN(4);
	} > FLASH
	ASSERT(ADDR(.picobin) + SIZEOF(.picobin) <= ORIGIN(FLASH) + 4K, "PICOBIN block must sit in the first 4 KB")

	.text :
	{
		. = ALIGN(4);
		*(.text)
		*(.text*)
		*(.glue_7)
		*(.glue_7t)
		*(.eh_frame)
		KEEP(*(.init))
		KEEP(*(.fini))
		. = ALIGN(4);
		_etext = .;
	} > FLASH

	.rodata :
	{
		. = ALIGN(4);
		*(.rodata)
		*(.rodata*)
		. = ALIGN(4);
	} > FLASH

	.ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
	.ARM :
	{
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
	} > FLASH

	.preinit_array :
	{
		PROVIDE_HIDDEN(__preinit_array_start = .);
		KEEP(*(.preinit_array*))
		PROVIDE_HIDDEN(__preinit_array_end = .);
	} > FLASH
	.init_array :
	{
		PROVIDE_HIDDEN(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array*))
		PROVIDE_HIDDEN(__init_array_end = .);
	} > FLASH
	.fini_array :
	{
		PROVIDE_HIDDEN(__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array*))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} > FLASH

	_sidata = LOADADDR(.data);

	.data :
	{
		. = ALIGN(4);
		_sdata = .;
		*(.data)
		*(.data*)
		. = ALIGN(4);
		_edata = .;
	} > RAM AT> FLASH

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		_sbss = .;
		__bss_start__ = _sbss;
		*(.bss)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
		__bss_end__ = _ebss;
	} > RAM

	/* newlib's _sbrk grows from end; the stack sits at the top of RAM */
	._user_heap_stack (NOLOAD) :
	{
		. = ALIGN(8);
		PROVIDE(end = .);
		PROVIDE(_end = .);
		. = . + __heap_size;
		. = . + __stack_size;
		. = ALIGN(8);
	} > RAM

	.ARM.attributes 0 : { *(.ARM.attributes) }
}
