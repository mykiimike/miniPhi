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
@defgroup mpArchSTM32F4Clock STM32F4 clock tree, power levels and kernel tick

@ingroup mpArchSTM32

HIGH, MEDIUM and LOW run MP_CLOCK_HE_FREQ, MP_CLOCK_ME_FREQ and
MP_CLOCK_LE_FREQ. A level equal to the HSE crystal runs from it. Anything
else is the PLL, fed by that crystal, or by the 16 MHz HSI when the
crystal does not start. SLEEP is the HSI with the PLL off. APB1 stays at
or under 42 MHz and APB2 at or under 84 MHz. SysTick is tickless. Stop is
not used.

@{
*/

#define _HSI_HZ 16000000UL
#define _APB1_MAX_HZ 42000000UL
#define _APB2_MAX_HZ 84000000UL
#define _HSE_TIMEOUT 2000000UL
#define _SYSTICK_MAX 0xFFFFFFUL

#ifndef MP_CLOCK_ME_FREQ
	#define MP_CLOCK_ME_FREQ MHZ84_t
#endif

#ifndef MP_CLOCK_LE_FREQ
	#define MP_CLOCK_LE_FREQ MHZ24_t
#endif

static volatile unsigned long __ticks = 0;
static unsigned long __sysclk = _HSI_HZ;
static unsigned long __pclk1 = _HSI_HZ;
static unsigned long __pclk2 = _HSI_HZ;
static unsigned long __tickCounts = _HSI_HZ / STM32_TICK_RATE_HZ;

typedef struct {
	/* RCC_CFGR SW: 0 HSI, 1 HSE, 2 PLL */
	uint32_t sw;
	uint32_t pllm;
	uint32_t plln;
	uint32_t pllp;
	mp_bool_t hse;
	unsigned long hz;
} _mp_clock_plan_t;

static _mp_clock_plan_t __plan = { 0, 0, 0, 0, NO, _HSI_HZ };
static mp_bool_t __hseBroken = NO;

#ifdef MP_CLOCK_LSE
static mp_bool_t __lseWaiting = NO;
static mp_bool_t __lse = NO;
#endif

static unsigned long _mp_clock_hz(mp_clock_freq_t freq) {
	switch(freq) {
		case MHZ12_t: return 12000000UL;
		case MHZ24_t: return 24000000UL;
		case MHZ48_t: return 48000000UL;
		case MHZ84_t: return 84000000UL;
		case MHZ96_t: return 96000000UL;
		case MHZ120_t: return 120000000UL;
		case MHZ144_t: return 144000000UL;
		case MHZ168_t: return 168000000UL;
		default: return _HSI_HZ;
	}
}

static unsigned long _mp_clock_apb1(unsigned long hz) {
	if(hz <= _APB1_MAX_HZ)
		return(hz);
	if(hz / 2 <= _APB1_MAX_HZ)
		return(hz / 2);
	return(hz / 4);
}

static unsigned long _mp_clock_apb2(unsigned long hz) {
	return(hz <= _APB2_MAX_HZ ? hz : hz / 2);
}

/* PPRE encoding: 0 is /1, 4 is /2, 5 is /4 */
static uint32_t _mp_clock_ppre(unsigned long hz, unsigned long bus) {
	if(bus == hz)
		return(0);
	if(bus * 2 == hz)
		return(4);
	return(5);
}

static unsigned _mp_clock_latency(unsigned long hz) {
	if(hz <= 30000000UL)
		return(0);
	if(hz <= 60000000UL)
		return(1);
	if(hz <= 90000000UL)
		return(2);
	if(hz <= 120000000UL)
		return(3);
	if(hz <= 150000000UL)
		return(4);
	return(5);
}

static void _mp_clock_flash(unsigned long hz) {
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | _mp_clock_latency(hz);
}

static void _mp_clock_dividers(unsigned long hz) {
	uint32_t ppre1 = _mp_clock_ppre(hz, _mp_clock_apb1(hz));
	uint32_t ppre2 = _mp_clock_ppre(hz, _mp_clock_apb2(hz));

	RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) | (ppre1 << 10) | (ppre2 << 13);
	mp_adc_prescale(_mp_clock_apb2(hz));
}

static void _mp_clock_switch(uint32_t sw) {
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | sw;
	while(((RCC->CFGR & RCC_CFGR_SWS) >> 2) != sw);
}

static mp_bool_t _mp_clock_hse_start(void) {
	unsigned long timeout = _HSE_TIMEOUT;

	if(RCC->CR & RCC_CR_HSERDY)
		return(YES);

	RCC->CR |= RCC_CR_HSEON;
	while(!(RCC->CR & RCC_CR_HSERDY)) {
		if(--timeout == 0) {
			RCC->CR &= ~RCC_CR_HSEON;
			__hseBroken = YES;
			return(NO);
		}
	}
	return(YES);
}

/* VCO in [192, 432] MHz, input 1 MHz from src. pllp is 2, 4, 6 or 8. */
static mp_bool_t _mp_clock_pll_fit(unsigned long target, unsigned long src, _mp_clock_plan_t *plan) {
	uint32_t pllm;
	uint32_t pllp;

	if(src < 2000000UL || src % 1000000UL)
		return(NO);
	pllm = src / 1000000UL;
	for(pllp = 2; pllp <= 8; pllp += 2) {
		unsigned long vco = target * pllp;

		if(vco < 192000000UL || vco > 432000000UL || (vco % 1000000UL) != 0)
			continue;
		plan->pllm = pllm;
		plan->plln = vco / 1000000UL;
		plan->pllp = pllp;
		if(plan->plln >= 192 && plan->plln <= 432)
			return(YES);
	}
	return(NO);
}

static void _mp_clock_plan(mp_power_level_t level, _mp_clock_plan_t *plan) {
	unsigned long target;
	unsigned long src;
	mp_bool_t hse = MP_CLOCK_HSE_HZ != 0 && __hseBroken == NO ? YES : NO;

	plan->sw = 0;
	plan->pllm = 0;
	plan->plln = 0;
	plan->pllp = 0;
	plan->hse = NO;
	plan->hz = _HSI_HZ;

	if(level == MP_POWER_SLEEP)
		return;

	target = _mp_clock_hz(level == MP_POWER_HIGH ? MP_CLOCK_HE_FREQ : level == MP_POWER_MEDIUM ? MP_CLOCK_ME_FREQ : MP_CLOCK_LE_FREQ);
	src = hse == YES ? MP_CLOCK_HSE_HZ : _HSI_HZ;

	if(hse == YES && target == src) {
		plan->sw = 1;
		plan->hse = YES;
		plan->hz = src;
		return;
	}
	if(_mp_clock_pll_fit(target, src, plan) == NO) {
		plan->hz = _HSI_HZ;
		return;
	}
	plan->sw = 2;
	plan->hse = hse;
	plan->hz = target;
}

static void _mp_clock_hsi(_mp_clock_plan_t *plan) {
	plan->sw = 0;
	plan->pllm = 0;
	plan->plln = 0;
	plan->pllp = 0;
	plan->hse = NO;
	plan->hz = _HSI_HZ;
}

static void _mp_clock_apply(mp_kernel_t *kernel, const _mp_clock_plan_t *plan) {
	_mp_clock_plan_t use = *plan;
	unsigned long top = use.hz > __sysclk ? use.hz : __sysclk;
	unsigned long timeout;
	uint32_t q;

	if(use.hz > __sysclk)
		_mp_clock_flash(use.hz);
	_mp_clock_dividers(top);

	if(use.sw == 2) {
		if(((RCC->CFGR & RCC_CFGR_SWS) >> 2) == 2)
			_mp_clock_switch(0);
		RCC->CR &= ~RCC_CR_PLLON;
		timeout = _HSE_TIMEOUT;
		while((RCC->CR & RCC_CR_PLLRDY) && --timeout);

		q = use.plln * 1000000UL / 48000000UL;
		if(q < 2)
			q = 2;
		if(q > 15)
			q = 15;

		RCC->PLLCFGR = (use.pllm & 0x3F)
			| ((use.plln & 0x1FF) << 6)
			| ((((use.pllp / 2) - 1) & 3) << 16)
			| (q << 24)
			| (use.hse == YES ? RCC_PLLCFGR_PLLSRC : 0);

		RCC->CR |= RCC_CR_PLLON;
		timeout = _HSE_TIMEOUT;
		while(!(RCC->CR & RCC_CR_PLLRDY) && --timeout);
		if(timeout == 0) {
			RCC->CR &= ~RCC_CR_PLLON;
			_mp_clock_hsi(&use);
		}
	}

	_mp_clock_switch(use.sw);

	if(use.sw != 2)
		RCC->CR &= ~RCC_CR_PLLON;
	if(use.hse == NO)
		RCC->CR &= ~RCC_CR_HSEON;

	_mp_clock_flash(use.hz);
	_mp_clock_dividers(use.hz);

	__sysclk = use.hz;
	__pclk1 = _mp_clock_apb1(use.hz);
	__pclk2 = _mp_clock_apb2(use.hz);
	__plan = use;
	kernel->arch.sysclk = use.hz;
	kernel->arch.hse = use.hse;
}

static void _mp_clock_tick_retime(void) {
	unsigned long counts = __sysclk / STM32_TICK_RATE_HZ;
	unsigned long left;

	MP_INTERRUPT_SAFE_BEGIN
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE;
	left = (unsigned long)((unsigned long long)SysTick->VAL * counts / __tickCounts);
	if(left < 2)
		left = 2;
	SysTick->LOAD = left - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE;
	__tickCounts = counts;
	SysTick->LOAD = counts - 1;
	MP_INTERRUPT_SAFE_END
}

static mp_bool_t _mp_clock_quiet(void) {
	if(mp_uart_quiet() == NO || mp_i2c_quiet() == NO || mp_spi_quiet() == NO || mp_adc_quiet() == NO)
		return(NO);
	return(YES);
}

static mp_bool_t _mp_clock_same(const _mp_clock_plan_t *a, const _mp_clock_plan_t *b) {
	return(a->sw == b->sw && a->pllm == b->pllm && a->plln == b->plln && a->pllp == b->pllp && a->hse == b->hse && a->hz == b->hz ? YES : NO);
}

mp_ret_t mp_clock_level(mp_kernel_t *kernel, mp_power_level_t level) {
	_mp_clock_plan_t plan;

	_mp_clock_plan(level, &plan);
	if(_mp_clock_same(&plan, &__plan) == YES)
		return(TRUE);
	if(_mp_clock_quiet() == NO)
		return(FALSE);

	if(plan.hse == YES && _mp_clock_hse_start() == NO)
		_mp_clock_plan(level, &plan);

	if(_mp_clock_same(&plan, &__plan) == NO) {
#ifdef SUPPORT_COMMON_USB
		mp_usb_clock_stop();
#endif
		_mp_clock_apply(kernel, &plan);
		_mp_clock_tick_retime();
		mp_uart_retime();
		mp_i2c_retime();
		mp_spi_retime();
		mp_timer_retime();
		mp_fsmc_retime(__sysclk);
#ifdef SUPPORT_COMMON_USB
		mp_usb_clock_start();
#endif
	}
	return(TRUE);
}

unsigned long mp_clock_level_hz(mp_power_level_t level) {
	_mp_clock_plan_t plan;

	_mp_clock_plan(level, &plan);
	return(plan.hz);
}

#ifdef MP_CLOCK_LSE
static void _mp_clock_lse_start(void) {
	RCC->APB1ENR |= RCC_APB1_PWR;
	(void)RCC->APB1ENR;
	PWR->CR |= PWR_CR_DBP;
	RCC->BDCR |= RCC_BDCR_LSEON;
	__lseWaiting = YES;
}

static void _mp_clock_lse_poll(void) {
	if(__lseWaiting == NO || !(RCC->BDCR & RCC_BDCR_LSERDY))
		return;
	__lseWaiting = NO;
	__lse = YES;
}
#endif

static void _mp_clock_sleep(unsigned long idle) {
	unsigned long counts = __tickCounts;
	unsigned long reload;
	unsigned long done;
	unsigned long whole;
	unsigned long left;
	uint32_t ctrl;

	if(idle > _SYSTICK_MAX / counts)
		idle = _SYSTICK_MAX / counts;
	if(idle < 2) {
		__asm volatile ("wfi");
		return;
	}

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE;
	if(SCB->ICSR & SCB_ICSR_PENDSTSET) {
		SysTick->CTRL |= SysTick_CTRL_ENABLE;
		return;
	}
	reload = SysTick->VAL + counts * (idle - 1);
	SysTick->LOAD = reload;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE;

	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("wfi");
	__asm volatile ("isb");

	ctrl = SysTick->CTRL;
	SysTick->CTRL = ctrl & ~SysTick_CTRL_ENABLE;
	if(ctrl & SysTick_CTRL_COUNTFLAG) {
		done = reload - SysTick->VAL;
		left = done < counts - 1 ? counts - 1 - done : counts - 1;
		whole = idle - 1;
	}
	else {
		done = idle * counts - SysTick->VAL;
		whole = done / counts;
		left = (whole + 1) * counts - done;
	}
	if(left < 2)
		left = 2;

	SysTick->LOAD = left - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE;
	__ticks += whole;
	SysTick->LOAD = counts - 1;
}

static void _mp_clock_scale(void) {
	unsigned long timeout = _HSE_TIMEOUT;

	RCC->APB1ENR |= RCC_APB1_PWR;
	(void)RCC->APB1ENR;
	PWR->CR |= PWR_CR_VOS;
	while(!(PWR->CSR & PWR_CSR_VOSRDY) && --timeout);
}

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	RCC->CR |= RCC_CR_HSION;
	while(!(RCC->CR & RCC_CR_HSIRDY));
	_mp_clock_switch(0);
	RCC->CR &= ~RCC_CR_PLLON;
	_mp_clock_flash(_HSI_HZ);
	_mp_clock_dividers(_HSI_HZ);
	_mp_clock_scale();

	SysTick->CTRL = 0;
	SysTick->LOAD = __tickCounts - 1;
	SysTick->VAL = 0;
	SCB->SHP[11] = 0x40;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;

	mp_clock_level(kernel, MP_POWER_HIGH);

	DEMCR |= DEMCR_TRCENA;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA;

#ifdef MP_CLOCK_LSE
	_mp_clock_lse_start();
#endif

	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	(void)kernel;
	SysTick->CTRL = 0;
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
	(void)kernel;
}

void mp_clock_schedule(mp_kernel_t *kernel, unsigned long limit) {
	unsigned long idle;

#ifdef MP_CLOCK_LSE
	_mp_clock_lse_poll();
#endif

	__asm volatile ("cpsid i" ::: "memory");
	idle = mp_task_idle(&kernel->tasks);
	if(limit < idle)
		idle = limit;
	if(idle > 0) {
		mp_adc_idle();
		_mp_clock_sleep(idle);
	}
	__asm volatile ("cpsie i" ::: "memory");
}

const char *mp_clock_deep(void) {
#ifdef MP_CLOCK_LSE
	if(__lseWaiting == YES)
		return("WFI, LSE starting");
	if(__lse == YES)
		return("WFI, LSE on");
#endif
	return("WFI, tickless");
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

unsigned long mp_clock_us(void) {
	unsigned long ticks;
	unsigned long val;
	unsigned long counts;

	MP_INTERRUPT_SAFE_BEGIN
	ticks = __ticks;
	val = SysTick->VAL;
	counts = __tickCounts;
	if(SCB->ICSR & SCB_ICSR_PENDSTSET) {
		ticks++;
		val = SysTick->VAL;
	}
	MP_INTERRUPT_SAFE_END

	if(val >= counts)
		val = counts - 1;
	return(ticks * 1000UL + (counts - 1 - val) * 1000UL / counts);
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
	switch(clock) {
		case MHZ12_t: return("12 MHz");
		case MHZ24_t: return("24 MHz");
		case MHZ48_t: return("48 MHz");
		case MHZ84_t: return("84 MHz");
		case MHZ96_t: return("96 MHz");
		case MHZ120_t: return("120 MHz");
		case MHZ144_t: return("144 MHz");
		case MHZ168_t: return("168 MHz");
		default: return("16 MHz");
	}
}

mp_bool_t mp_clock_usb48(void) {
	uint32_t q;

	/* PLLQ is chosen so VCO / Q is 48 MHz when the VCO is a multiple of 48 */
	if(__plan.sw != 2 || (__plan.plln % 48) != 0)
		return(NO);
	q = __plan.plln / 48;
	return(q >= 2 && q <= 15 ? YES : NO);
}

void mp_clock_delay(int delay) {
	unsigned long start;
	int seen = 0;

	if(delay <= 0)
		return;

	if(mp_interrupt_state() == TRUE) {
		start = __ticks;
		while((long)(__ticks - start) < delay)
			__asm volatile ("wfi");
		return;
	}

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
