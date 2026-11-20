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
@defgroup mpArchRP2350Startup RP2350 reset and vector table

@ingroup mpArchRP2350

@{
*/

/* from the linker script */
extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

int main(void);
void Reset_Handler(void);
void SysTick_Handler(void);

/* last fault, readable from the debugger */
volatile uint32_t mp_arch_fault[4];

void Default_Handler(void) {
	unsigned int n;

	mp_arch_fault[0] = SCB->ICSR & SCB_ICSR_VECTACTIVE;
	mp_arch_fault[1] = SCB->CFSR;
	mp_arch_fault[2] = SCB->HFSR;
	mp_arch_fault[3] = SCB->BFAR;
	/* SIO owns GP21 after the glass is up. A blink means a fault; a
	 * frozen picture with a steady backlight is a hang in thread mode. */
	IO_BANK0_GPIO_CTRL(21) = IO_GPIO_FUNCSEL_SIO;
	for(;;) {
		SIO_GPIO_OUT_XOR = 1UL << 21;
		n = 8000000;
		while(n--)
			__asm volatile("nop");
	}
}

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SecureFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* a strong handler in another file replaces the weak one */
#define _IRQ_WEAK(name) \
	void name(void) __attribute__((weak)); \
	void name(void) { mp_interrupt_dispatch(); }

_IRQ_WEAK(TIMER0_IRQ_0_Handler)
_IRQ_WEAK(TIMER0_IRQ_1_Handler)
_IRQ_WEAK(TIMER0_IRQ_2_Handler)
_IRQ_WEAK(TIMER0_IRQ_3_Handler)
_IRQ_WEAK(TIMER1_IRQ_0_Handler)
_IRQ_WEAK(TIMER1_IRQ_1_Handler)
_IRQ_WEAK(TIMER1_IRQ_2_Handler)
_IRQ_WEAK(TIMER1_IRQ_3_Handler)
_IRQ_WEAK(PWM_IRQ_WRAP_0_Handler)
_IRQ_WEAK(PWM_IRQ_WRAP_1_Handler)
_IRQ_WEAK(DMA_IRQ_0_Handler)
_IRQ_WEAK(DMA_IRQ_1_Handler)
_IRQ_WEAK(DMA_IRQ_2_Handler)
_IRQ_WEAK(DMA_IRQ_3_Handler)
_IRQ_WEAK(USBCTRL_IRQ_Handler)
_IRQ_WEAK(PIO0_IRQ_0_Handler)
_IRQ_WEAK(PIO0_IRQ_1_Handler)
_IRQ_WEAK(PIO1_IRQ_0_Handler)
_IRQ_WEAK(PIO1_IRQ_1_Handler)
_IRQ_WEAK(PIO2_IRQ_0_Handler)
_IRQ_WEAK(PIO2_IRQ_1_Handler)
_IRQ_WEAK(IO_IRQ_BANK0_Handler)
_IRQ_WEAK(IO_IRQ_BANK0_NS_Handler)
_IRQ_WEAK(IO_IRQ_QSPI_Handler)
_IRQ_WEAK(IO_IRQ_QSPI_NS_Handler)
_IRQ_WEAK(SIO_IRQ_FIFO_Handler)
_IRQ_WEAK(SIO_IRQ_BELL_Handler)
_IRQ_WEAK(SIO_IRQ_FIFO_NS_Handler)
_IRQ_WEAK(SIO_IRQ_BELL_NS_Handler)
_IRQ_WEAK(SIO_IRQ_MTIMECMP_Handler)
_IRQ_WEAK(CLOCKS_IRQ_Handler)
_IRQ_WEAK(SPI0_IRQ_Handler)
_IRQ_WEAK(SPI1_IRQ_Handler)
_IRQ_WEAK(UART0_IRQ_Handler)
_IRQ_WEAK(UART1_IRQ_Handler)
_IRQ_WEAK(ADC_IRQ_FIFO_Handler)
_IRQ_WEAK(I2C0_IRQ_Handler)
_IRQ_WEAK(I2C1_IRQ_Handler)
_IRQ_WEAK(OTP_IRQ_Handler)
_IRQ_WEAK(TRNG_IRQ_Handler)
_IRQ_WEAK(PROC0_IRQ_CTI_Handler)
_IRQ_WEAK(PROC1_IRQ_CTI_Handler)
_IRQ_WEAK(PLL_SYS_IRQ_Handler)
_IRQ_WEAK(PLL_USB_IRQ_Handler)
_IRQ_WEAK(POWMAN_IRQ_POW_Handler)
_IRQ_WEAK(POWMAN_IRQ_TIMER_Handler)
_IRQ_WEAK(SPAREIRQ_IRQ_0_Handler)
_IRQ_WEAK(SPAREIRQ_IRQ_1_Handler)
_IRQ_WEAK(SPAREIRQ_IRQ_2_Handler)
_IRQ_WEAK(SPAREIRQ_IRQ_3_Handler)
_IRQ_WEAK(SPAREIRQ_IRQ_4_Handler)
_IRQ_WEAK(SPAREIRQ_IRQ_5_Handler)

__attribute__((section(".isr_vector"), used))
void (* const mp_arch_vectors[16 + MP_ARCH_IRQ_COUNT])(void) = {
	(void (*)(void))&_estack,
	Reset_Handler,
	NMI_Handler,
	HardFault_Handler,
	MemManage_Handler,
	BusFault_Handler,
	UsageFault_Handler,
	SecureFault_Handler,
	0, 0, 0,
	SVC_Handler,
	DebugMon_Handler,
	0,
	PendSV_Handler,
	SysTick_Handler,

	TIMER0_IRQ_0_Handler,
	TIMER0_IRQ_1_Handler,
	TIMER0_IRQ_2_Handler,
	TIMER0_IRQ_3_Handler,
	TIMER1_IRQ_0_Handler,
	TIMER1_IRQ_1_Handler,
	TIMER1_IRQ_2_Handler,
	TIMER1_IRQ_3_Handler,
	PWM_IRQ_WRAP_0_Handler,
	PWM_IRQ_WRAP_1_Handler,
	DMA_IRQ_0_Handler,
	DMA_IRQ_1_Handler,
	DMA_IRQ_2_Handler,
	DMA_IRQ_3_Handler,
	USBCTRL_IRQ_Handler,
	PIO0_IRQ_0_Handler,
	PIO0_IRQ_1_Handler,
	PIO1_IRQ_0_Handler,
	PIO1_IRQ_1_Handler,
	PIO2_IRQ_0_Handler,
	PIO2_IRQ_1_Handler,
	IO_IRQ_BANK0_Handler,
	IO_IRQ_BANK0_NS_Handler,
	IO_IRQ_QSPI_Handler,
	IO_IRQ_QSPI_NS_Handler,
	SIO_IRQ_FIFO_Handler,
	SIO_IRQ_BELL_Handler,
	SIO_IRQ_FIFO_NS_Handler,
	SIO_IRQ_BELL_NS_Handler,
	SIO_IRQ_MTIMECMP_Handler,
	CLOCKS_IRQ_Handler,
	SPI0_IRQ_Handler,
	SPI1_IRQ_Handler,
	UART0_IRQ_Handler,
	UART1_IRQ_Handler,
	ADC_IRQ_FIFO_Handler,
	I2C0_IRQ_Handler,
	I2C1_IRQ_Handler,
	OTP_IRQ_Handler,
	TRNG_IRQ_Handler,
	PROC0_IRQ_CTI_Handler,
	PROC1_IRQ_CTI_Handler,
	PLL_SYS_IRQ_Handler,
	PLL_USB_IRQ_Handler,
	POWMAN_IRQ_POW_Handler,
	POWMAN_IRQ_TIMER_Handler,
	SPAREIRQ_IRQ_0_Handler,
	SPAREIRQ_IRQ_1_Handler,
	SPAREIRQ_IRQ_2_Handler,
	SPAREIRQ_IRQ_3_Handler,
	SPAREIRQ_IRQ_4_Handler,
	SPAREIRQ_IRQ_5_Handler,
};

/* Datasheet 5.9.5.1, minimum Arm IMAGE_DEF plus an explicit VECTOR_TABLE
 * item. The minimum block (IMAGE_TYPE + LAST) already boots: the bootrom
 * defaults the entry to the vector table at image base (0x10000000). The
 * explicit VECTOR_TABLE item is added so the entry does not depend on that
 * default - it matches the form Zephyr uses for RP2350 Arm. */
__attribute__((section(".picobin"), used, aligned(4)))
const uint32_t mp_arch_image_def[7] = {
	0xffffded3,              /* PICOBIN_BLOCK_MARKER_START            */
	0x10210142,              /* IMAGE_TYPE: EXE|S|ARM|RP2350          */
	0x00000203,              /* VECTOR_TABLE item, 2 words           */
	(uint32_t)mp_arch_vectors, /* address of the Cortex-M vector table */
	0x000003ff,              /* LAST item, 3 words (hdr + next + end) */
	0x00000000,              /* next block offset (none)             */
	0xab123579,              /* PICOBIN_BLOCK_MARKER_END             */
};

/* The bootrom isolates every pad (ISO resets to 1) and our image runs
 * before mp_gpio_init. Light the backlight here so a later fault still
 * leaves the glass glowing. */
#define _ALIAS_CLR 0x3000UL

static void _mp_arch_backlight(void) {
	unsigned int spins = 100000;
	volatile unsigned long *pad_clr;

	*(volatile unsigned long *)(RP2350_RESETS_BASE + _ALIAS_CLR) =
		RESETS_IO_BANK0 | RESETS_PADS_BANK0;
	while(spins-- && (RESETS_RESET_DONE & (RESETS_IO_BANK0 | RESETS_PADS_BANK0))
		!= (RESETS_IO_BANK0 | RESETS_PADS_BANK0));

	IO_BANK0_GPIO_CTRL(21) = IO_GPIO_FUNCSEL_SIO
		| (3UL << 12)   /* OUTOVER: force high */
		| (3UL << 14);  /* OEOVER: force enable */
	PADS_BANK0_GPIO(21) = (PADS_SCHMITT | PADS_DRIVE_4) & ~PADS_ISO;
	pad_clr = (volatile unsigned long *)(RP2350_PADS_BANK0_BASE + 0x004UL + 4UL * 21UL + _ALIAS_CLR);
	*pad_clr = PADS_ISO;
	SIO_GPIO_OE_SET = (1UL << 21);
	SIO_GPIO_OUT_SET = (1UL << 21);
}

__attribute__((noinline, used)) void mp_arch_reset(void) {
	uint32_t *src, *dst;

	_mp_arch_backlight();

	for(src = &_sidata, dst = &_sdata; dst < &_edata;)
		*dst++ = *src++;

	for(dst = &_sbss; dst < &_ebss;)
		*dst++ = 0;

	/* FPv5-SP, full access. Lazy stacking off, so a handler sees the
	 * extended frame already written and EXC_RETURN bit 4 says so. */
	ACTLR |= ACTLR_EXTEXCLALL;
	CPACR |= CPACR_FPU_FULL;
	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("isb" ::: "memory");
	FPCCR = (FPCCR | FPCCR_ASPEN) & ~FPCCR_LSPEN;

	/* Enable the configurable fault handlers so a UsageFault (INVPC on a
	 * forged EXC_RETURN, FPU access, divide-by-zero, unaligned) reports as
	 * its own exception instead of escalating to HardFault. */
	SCB->SHCSR |= (1UL << 16) | (1UL << 17) | (1UL << 18);

	SCB->VTOR = (uint32_t)mp_arch_vectors;

	main();

	while(1);
}

/* The bootrom arrives with its own stack limit. Clear it and take our
 * stack before any push, or the first prologue can fault. */
__attribute__((naked)) void Reset_Handler(void) {
	__asm volatile(
		"cpsid i\n"
		"movs r0, #0\n"
		"msr msplim, r0\n"
		"msr psplim, r0\n"
		"movw r0, #0x2000\n"
		"movt r0, #0x2008\n"
		"msr msp, r0\n"
		"bl mp_arch_reset\n"
		"b .\n"
	);
}

/**@}*/
