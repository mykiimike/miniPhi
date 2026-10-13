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
@defgroup mpArchSTM32F1Startup STM32F1 reset and vector table

@ingroup mpArchSTM32

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
	mp_arch_fault[0] = SCB->ICSR & SCB_ICSR_VECTACTIVE;
	mp_arch_fault[1] = SCB->CFSR;
	mp_arch_fault[2] = SCB->HFSR;
	mp_arch_fault[3] = SCB->BFAR;
	while(1);
}

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));

#define _IRQ mp_interrupt_dispatch

__attribute__((section(".isr_vector"), used))
void (* const mp_arch_vectors[16 + MP_ARCH_IRQ_COUNT])(void) = {
	(void (*)(void))&_estack,
	Reset_Handler,
	NMI_Handler,
	HardFault_Handler,
	MemManage_Handler,
	BusFault_Handler,
	UsageFault_Handler,
	0, 0, 0, 0,
	SVC_Handler,
	DebugMon_Handler,
	0,
	PendSV_Handler,
	SysTick_Handler,

	/* peripheral lines, all through the miniPhi interrupt table */
	_IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ,
	_IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ,
	_IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ,
	_IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ,
	_IRQ, _IRQ, _IRQ,
#if MP_ARCH_IRQ_COUNT > 43
	/* high density and XL: TIM8, ADC3, FSMC, SDIO, TIM5, SPI3, UART4, UART5, TIM6, TIM7, DMA2 */
	_IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ,
	_IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ, _IRQ,
#endif
};

void Reset_Handler(void) {
	uint32_t *src, *dst;

	for(src = &_sidata, dst = &_sdata; dst < &_edata;)
		*dst++ = *src++;

	for(dst = &_sbss; dst < &_ebss;)
		*dst++ = 0;

	/* a debugger reset may leave VTOR elsewhere */
	SCB->VTOR = (uint32_t)mp_arch_vectors;

	main();

	while(1);
}

/**@}*/
