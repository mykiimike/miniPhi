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
@defgroup mpArchSTM32F1Clock STM32F1 clock tree and kernel tick

@ingroup mpArchSTM32

SYSCLK comes from the PLL fed by the HSE crystal (MP_CLOCK_HSE_HZ), or
by HSI / 2 when the crystal does not start. AHB runs at SYSCLK, APB2 too,
APB1 at most 36 MHz. SysTick ticks at 1 kHz, so one tick is one ms.

@{
*/

#define _HSI_HZ 8000000UL
#define _PLL_MAX_HZ 72000000UL
#define _APB1_MAX_HZ 36000000UL
#define _ADC_MAX_HZ 14000000UL
#define _HSE_TIMEOUT 200000UL

static const unsigned char __mhz[] = { 8, 16, 24, 32, 40, 48, 56, 64, 72 };
static const char *__names[] = { "8 MHz", "16 MHz", "24 MHz", "32 MHz", "40 MHz", "48 MHz", "56 MHz", "64 MHz", "72 MHz" };

static volatile unsigned long __ticks = 0;
static unsigned long __sysclk = _HSI_HZ;
static unsigned long __pclk1 = _HSI_HZ;
static unsigned long __pclk2 = _HSI_HZ;

static mp_bool_t _mp_clock_hse_start(void) {
	unsigned long timeout = _HSE_TIMEOUT;

	if(MP_CLOCK_HSE_HZ == 0)
		return(NO);

	RCC->CR |= RCC_CR_HSEON;
	while(!(RCC->CR & RCC_CR_HSERDY)) {
		if(--timeout == 0) {
			RCC->CR &= ~RCC_CR_HSEON;
			return(NO);
		}
	}
	return(YES);
}

static void _mp_clock_switch(uint32_t sw) {
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | sw;
	while(((RCC->CFGR & RCC_CFGR_SWS) >> 2) != sw);
}

static unsigned long _mp_clock_tree(mp_kernel_t *kernel, unsigned long target) {
	unsigned long source;
	unsigned long sysclk;
	unsigned long mul;
	uint32_t cfgr;
	uint32_t ppre1;
	uint32_t adcpre;
	mp_bool_t hse;

	/* run from HSI while the tree changes */
	RCC->CR |= RCC_CR_HSION;
	while(!(RCC->CR & RCC_CR_HSIRDY));
	_mp_clock_switch(0);
	RCC->CR &= ~RCC_CR_PLLON;

	hse = _mp_clock_hse_start();
	kernel->arch.hse = hse;
	source = hse == YES ? MP_CLOCK_HSE_HZ : _HSI_HZ / 2;

	if(hse == YES && target == MP_CLOCK_HSE_HZ)
		mul = 0;
	else if(hse == NO && target == _HSI_HZ)
		mul = 0;
	else {
		mul = target / source;
		if(mul < 2)
			mul = 2;
		if(mul > 16)
			mul = 16;
		while(mul > 2 && source * mul > _PLL_MAX_HZ)
			mul--;
	}
	sysclk = mul ? source * mul : (hse == YES ? MP_CLOCK_HSE_HZ : _HSI_HZ);

	/* flash wait states before going faster */
	FLASH->ACR = FLASH_ACR_PRFTBE | (sysclk <= 24000000UL ? 0 : sysclk <= 48000000UL ? 1 : 2);

	ppre1 = sysclk > _APB1_MAX_HZ ? 4 : 0;
	if(sysclk / 2 <= _ADC_MAX_HZ)
		adcpre = 0;
	else if(sysclk / 4 <= _ADC_MAX_HZ)
		adcpre = 1;
	else if(sysclk / 6 <= _ADC_MAX_HZ)
		adcpre = 2;
	else
		adcpre = 3;

	cfgr = RCC->CFGR & ~(RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL | (1UL << 17) | (0xFUL << 4));
	cfgr |= ppre1 << 8;
	cfgr |= adcpre << 14;
	if(mul) {
		cfgr |= (mul - 2) << 18;
		if(hse == YES)
			cfgr |= RCC_CFGR_PLLSRC;
	}
	RCC->CFGR = cfgr;

	if(mul) {
		RCC->CR |= RCC_CR_PLLON;
		while(!(RCC->CR & RCC_CR_PLLRDY));
		_mp_clock_switch(2);
	}
	else if(hse == YES)
		_mp_clock_switch(1);

	__sysclk = sysclk;
	__pclk1 = ppre1 ? sysclk / 2 : sysclk;
	__pclk2 = sysclk;
	kernel->arch.sysclk = sysclk;

	return(sysclk);
}

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	unsigned long sysclk;

	sysclk = _mp_clock_tree(kernel, (unsigned long)__mhz[MP_CLOCK_HE_FREQ] * 1000000UL);

	/* kernel tick */
	SysTick->CTRL = 0;
	SysTick->LOAD = sysclk / STM32_TICK_RATE_HZ - 1;
	SysTick->VAL = 0;
	SCB->SHP[11] = 0x40;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;

	/* cycle counter for mp_clock_nanoDelay() */
	DEMCR |= DEMCR_TRCENA;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA;

	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	SysTick->CTRL = 0;
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
}

mp_ret_t mp_clock_low_energy(mp_kernel_t *kernel) {
	return(TRUE);
}

mp_ret_t mp_clock_high_energy(mp_kernel_t *kernel) {
	return(TRUE);
}

void mp_clock_schedule(mp_kernel_t *kernel) {
	/* masked so a wake up between the test and WFI is not lost: WFI still returns on it */
	__asm volatile ("cpsid i" ::: "memory");
	if(kernel->tasks.pendingNumber == 0)
		__asm volatile ("wfi");
	__asm volatile ("cpsie i" ::: "memory");
}

void mp_clock_task_change(mp_task_t *task) {
}

void mp_clock_wakeup(mp_kernel_t *kernel) {
}

void SysTick_Handler(void) {
	__ticks++;
}

unsigned long mp_clock_ticks() {
	return(__ticks);
}

unsigned long mp_clock_tick_hz() {
	return(STM32_TICK_RATE_HZ);
}

unsigned long mp_clock_get_speed() {
	return(__sysclk);
}

unsigned long mp_clock_hclk(void) {
	return(__sysclk);
}

unsigned long mp_clock_pclk1(void) {
	return(__pclk1);
}

unsigned long mp_clock_pclk2(void) {
	return(__pclk2);
}

const char *mp_clock_name(mp_clock_freq_t clock) {
	if((unsigned int)clock >= sizeof(__names) / sizeof(__names[0]))
		return("unknown");
	return(__names[clock]);
}

void mp_clock_delay(int delay) {
	unsigned long start;
	int seen = 0;

	if(delay <= 0)
		return;

	if(mp_interrupt_state() == TRUE) {
		start = __ticks;
		while((long)(__ticks - start) < delay);
		return;
	}

	/* masked: SysTick_Handler can not run, count the wraps instead */
	(void)SysTick->CTRL;
	while(seen < delay)
		if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG)
			seen++;
}

void mp_clock_nanoDelay(unsigned long delay) {
	uint32_t cycles = (uint32_t)(((uint64_t)delay * (__sysclk / 1000000UL) + 999) / 1000);
	uint32_t start = DWT->CYCCNT;

	while(DWT->CYCCNT - start < cycles);
}

/**@}*/
