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
@defgroup mpArchSTM32F1Clock STM32F1 clock tree, power levels and kernel tick

@ingroup mpArchSTM32

Each power level has its own tree:

- HIGH runs MP_CLOCK_HE_FREQ, MEDIUM MP_CLOCK_ME_FREQ, LOW MP_CLOCK_LE_FREQ,
  from the PLL fed by the HSE crystal (MP_CLOCK_HSE_HZ), or by HSI / 2 when
  the crystal does not start. A level equal to the crystal runs from it,
  8 MHz without a crystal runs from HSI.
- SLEEP runs HSI at 8 MHz with HSE and the PLL off: Stop wakes up on HSI,
  so there is nothing to restore.

AHB and APB2 run at SYSCLK, APB1 at most 36 MHz. A level only changes
while no UART, I2C, SPI or ADC transfer runs; then the UARTs, I2C, SPI,
timers, FSMC banks, the ADC prescaler and SysTick are retimed.

SysTick ticks at 1 kHz, so one tick is one ms. It is tickless: before WFI
it is stretched to the next deadline, at most 2^24 core cycles. With
MP_CLOCK_LSE the RTC runs on the 32.768 kHz crystal at 1024 Hz. Stop is
not entered: on the STM32-LCD it is followed by the board losing power,
and a press no longer wakes it.

@{
*/

#define _HSI_HZ 8000000UL
#define _PLL_MAX_HZ 72000000UL
#define _APB1_MAX_HZ 36000000UL
#define _ADC_MAX_HZ 14000000UL
#define _HSE_TIMEOUT 200000UL
#define _SYSTICK_MAX 0xFFFFFFUL

#ifndef MP_CLOCK_ME_FREQ
	#define MP_CLOCK_ME_FREQ MHZ24_t
#endif

#ifndef MP_CLOCK_LE_FREQ
	#define MP_CLOCK_LE_FREQ MHZ8_t
#endif

#ifdef MP_CLOCK_LSE
	#define _RTC_HZ 1024UL
	#define _RTC_PRL (32768UL / _RTC_HZ - 1)
	/* RTC counts left for the wake up itself: the sync and the next pass */
	#define _RTC_MARGIN 2UL
	/* the alarm is set again after the longest Stop */
	#define _STOP_MAX_MS 3600000UL
#endif

static const unsigned char __mhz[] = { 8, 16, 24, 32, 40, 48, 56, 64, 72 };
static const char *__names[] = { "8 MHz", "16 MHz", "24 MHz", "32 MHz", "40 MHz", "48 MHz", "56 MHz", "64 MHz", "72 MHz" };

typedef struct {
	/* source as RCC_CFGR SW: 0 HSI, 1 HSE, 2 PLL */
	uint32_t sw;
	unsigned long mul;
	mp_bool_t hse;
	unsigned long hz;
} _mp_clock_plan_t;

static volatile unsigned long __ticks = 0;
static unsigned long __sysclk = _HSI_HZ;
static unsigned long __pclk1 = _HSI_HZ;
static unsigned long __pclk2 = _HSI_HZ;

/* SysTick counts in one tick at the running clock */
static unsigned long __tickCounts = _HSI_HZ / STM32_TICK_RATE_HZ;

static _mp_clock_plan_t __plan = { 0, 0, NO, _HSI_HZ };
static mp_bool_t __hseBroken = NO;

#ifdef MP_CLOCK_LSE
static mp_bool_t __lseWaiting = NO;
static mp_bool_t __rtc = NO;

/* ms * 1024 left over by the last catch up */
static unsigned long __rtcCarry = 0;
#endif

static void _mp_clock_plan(mp_power_level_t level, _mp_clock_plan_t *plan) {
	mp_clock_freq_t freq;
	unsigned long target;
	unsigned long source;
	unsigned long mul;
	mp_bool_t hse = MP_CLOCK_HSE_HZ != 0 && __hseBroken == NO ? YES : NO;

	plan->sw = 0;
	plan->mul = 0;
	plan->hse = NO;
	plan->hz = _HSI_HZ;

	if(level == MP_POWER_SLEEP)
		return;

	freq = level == MP_POWER_HIGH ? MP_CLOCK_HE_FREQ : level == MP_POWER_MEDIUM ? MP_CLOCK_ME_FREQ : MP_CLOCK_LE_FREQ;
	target = (unsigned long)__mhz[freq] * 1000000UL;

	if(hse == YES && target == MP_CLOCK_HSE_HZ) {
		plan->sw = 1;
		plan->hse = YES;
		plan->hz = MP_CLOCK_HSE_HZ;
		return;
	}
	if(target <= _HSI_HZ)
		return;

	source = hse == YES ? MP_CLOCK_HSE_HZ : _HSI_HZ / 2;
	mul = target / source;
	if(mul < 2)
		mul = 2;
	if(mul > 16)
		mul = 16;
	while(mul > 2 && source * mul > _PLL_MAX_HZ)
		mul--;

	plan->sw = 2;
	plan->mul = mul;
	plan->hse = hse;
	plan->hz = source * mul;
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

static void _mp_clock_switch(uint32_t sw) {
	RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | sw;
	while(((RCC->CFGR & RCC_CFGR_SWS) >> 2) != sw);
}

/* flash wait states, APB1 under 36 MHz and the ADC under 14 MHz, for hz */
static void _mp_clock_bus(unsigned long hz) {
	uint32_t adcpre;

	if(hz / 2 <= _ADC_MAX_HZ)
		adcpre = 0;
	else if(hz / 4 <= _ADC_MAX_HZ)
		adcpre = 1;
	else if(hz / 6 <= _ADC_MAX_HZ)
		adcpre = 2;
	else
		adcpre = 3;

	FLASH->ACR = FLASH_ACR_PRFTBE | (hz <= 24000000UL ? 0 : hz <= 48000000UL ? 1 : 2);
	RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE))
		| ((hz > _APB1_MAX_HZ ? 4UL : 0UL) << 8)
		| (adcpre << 14);
}

static void _mp_clock_apply(mp_kernel_t *kernel, const _mp_clock_plan_t *plan) {
	unsigned long top = plan->hz > __sysclk ? plan->hz : __sysclk;
	uint32_t cfgr;

	/* while switching, everything has to fit the faster of both trees */
	mp_fsmc_retime(top);
	_mp_clock_bus(top);

	if(plan->sw == 2) {
		if(((RCC->CFGR & RCC_CFGR_SWS) >> 2) == 2)
			_mp_clock_switch(0);
		RCC->CR &= ~RCC_CR_PLLON;
		while(RCC->CR & RCC_CR_PLLRDY);

		cfgr = RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMUL);
		cfgr |= (plan->mul - 2) << 18;
		if(plan->hse == YES)
			cfgr |= RCC_CFGR_PLLSRC;
		RCC->CFGR = cfgr;

		RCC->CR |= RCC_CR_PLLON;
		while(!(RCC->CR & RCC_CR_PLLRDY));
	}
	_mp_clock_switch(plan->sw);

	if(plan->sw != 2)
		RCC->CR &= ~RCC_CR_PLLON;
	if(plan->hse == NO)
		RCC->CR &= ~RCC_CR_HSEON;

	_mp_clock_bus(plan->hz);

	__sysclk = plan->hz;
	__pclk1 = plan->hz > _APB1_MAX_HZ ? plan->hz / 2 : plan->hz;
	__pclk2 = plan->hz;
#ifdef SUPPORT_COMMON_USB
	/* 72 MHz PLL / 1.5, or the 48 MHz PLL itself */
	if(__sysclk == 48000000UL)
		RCC->CFGR |= RCC_CFGR_USBPRE;
	else
		RCC->CFGR &= ~RCC_CFGR_USBPRE;
#endif
	__plan = *plan;
	kernel->arch.sysclk = plan->hz;
	kernel->arch.hse = plan->hse;
}

/* the current tick keeps its share, the next ones run at the new clock */
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
	return(a->sw == b->sw && a->mul == b->mul && a->hse == b->hse && a->hz == b->hz ? YES : NO);
}

/**
 * @brief Run the tree of a power level
 *
 * @param[in] kernel Kernel handler
 * @param[in] level The level
 * @return FALSE while a transfer runs, the governor asks again
 */
mp_ret_t mp_clock_level(mp_kernel_t *kernel, mp_power_level_t level) {
	_mp_clock_plan_t plan;

	_mp_clock_plan(level, &plan);
	if(_mp_clock_same(&plan, &__plan) == YES)
		return(TRUE);
	if(_mp_clock_quiet() == NO)
		return(FALSE);

	/* the crystal starts on the old tree, SysTick keeps the time meanwhile */
	if(plan.hse == YES && _mp_clock_hse_start() == NO)
		_mp_clock_plan(level, &plan);

	if(_mp_clock_same(&plan, &__plan) == NO) {
#ifdef SUPPORT_COMMON_USB
		/* the PHY is parked and comes back only if this tree still has 48 MHz */
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

/**
 * @brief SYSCLK a level would run at
 *
 * @param[in] level The level
 * @return Hz
 */
unsigned long mp_clock_level_hz(mp_power_level_t level) {
	_mp_clock_plan_t plan;

	_mp_clock_plan(level, &plan);
	return(plan.hz);
}

#ifdef MP_CLOCK_LSE
static void _mp_rtc_wait(void) {
	while(!(RTC->CRL & RTC_CRL_RTOFF));
}

/* after Stop the APB1 side of the RTC is stale until the next sync */
static void _mp_rtc_sync(void) {
	RTC->CRL &= ~RTC_CRL_RSF;
	while(!(RTC->CRL & RTC_CRL_RSF));
}

static unsigned long _mp_rtc_count(void) {
	unsigned long high;
	unsigned long low;

	do {
		high = RTC->CNTH & 0xFFFF;
		low = RTC->CNTL & 0xFFFF;
	} while(high != (RTC->CNTH & 0xFFFF));
	return((high << 16) | low);
}

static void _mp_rtc_alarm(unsigned long at) {
	_mp_rtc_wait();
	RTC->CRL |= RTC_CRL_CNF;
	RTC->ALRH = at >> 16;
	RTC->ALRL = at & 0xFFFF;
	RTC->CRL &= ~RTC_CRL_CNF;
	_mp_rtc_wait();
}

static void _mp_clock_alarm(void *user) {
	RTC->CRL &= ~RTC_CRL_ALRF;
	EXTI->PR = 1UL << EXTI_LINE_RTC_ALARM;
}

static void _mp_clock_lse_start(void) {
	RCC->APB1ENR |= RCC_APB1_PWR | RCC_APB1_BKP;
	(void)RCC->APB1ENR;
	PWR->CR |= PWR_CR_DBP;

	/* the RTC source is only chosen once per backup domain reset */
	if((RCC->BDCR & RCC_BDCR_RTCSEL) && (RCC->BDCR & RCC_BDCR_RTCSEL) != RCC_BDCR_RTCSEL_LSE) {
		RCC->BDCR |= RCC_BDCR_BDRST;
		RCC->BDCR &= ~RCC_BDCR_BDRST;
	}
	RCC->BDCR |= RCC_BDCR_LSEON;
	__lseWaiting = YES;
}

/* the crystal takes up to a few seconds: the scheduler polls it */
static void _mp_clock_lse_poll(void) {
	if(__lseWaiting == NO || !(RCC->BDCR & RCC_BDCR_LSERDY))
		return;
	__lseWaiting = NO;

	RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | RCC_BDCR_RTCSEL_LSE | RCC_BDCR_RTCEN;
	_mp_rtc_sync();

	_mp_rtc_wait();
	RTC->CRL |= RTC_CRL_CNF;
	RTC->PRLH = 0;
	RTC->PRLL = _RTC_PRL;
	RTC->CRL &= ~RTC_CRL_CNF;
	_mp_rtc_wait();
	RTC->CRH |= RTC_CRH_ALRIE;
	_mp_rtc_wait();

	EXTI->RTSR |= 1UL << EXTI_LINE_RTC_ALARM;
	EXTI->PR = 1UL << EXTI_LINE_RTC_ALARM;
	EXTI->IMR |= 1UL << EXTI_LINE_RTC_ALARM;
	mp_interrupt_set(RTCAlarm_IRQn, _mp_clock_alarm, NULL, "RTC alarm");

	__rtc = YES;
}

/* masked, at SLEEP: HSI runs, HSE and the PLL are off */
static mp_bool_t _mp_clock_stop(mp_kernel_t *kernel, unsigned long idle) {
	unsigned long counts;

	/* Stop is followed by the board losing power, and a press cannot wake it */
	(void)kernel;
	(void)idle;
	return(NO);
	unsigned long start;
	unsigned long elapsed;
	unsigned long left;

	if(__rtc == NO)
		return(NO);
	if(idle > _STOP_MAX_MS)
		idle = _STOP_MAX_MS;
	counts = idle * _RTC_HZ / 1000UL;
	if(counts <= _RTC_MARGIN + 1)
		return(NO);
	counts -= _RTC_MARGIN;

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE;
	if(SCB->ICSR & SCB_ICSR_PENDSTSET) {
		SysTick->CTRL |= SysTick_CTRL_ENABLE;
		return(NO);
	}
	left = SysTick->VAL;

	start = _mp_rtc_count();
	RTC->CRL &= ~RTC_CRL_ALRF;
	_mp_rtc_alarm(start + counts);
	EXTI->PR = 1UL << EXTI_LINE_RTC_ALARM;

	mp_uart_wake(YES);

	/* a debugger keeps its link through Stop */
	if(DHCSR & DHCSR_C_DEBUGEN)
		DBGMCU_CR |= DBGMCU_CR_STOP | DBGMCU_CR_SLEEP;

	PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS | PWR_CR_CWUF;
	SCB->SCR |= SCB_SCR_SLEEPDEEP;
	__asm volatile ("dsb" ::: "memory");
	__asm volatile ("wfi");
	__asm volatile ("isb");
	SCB->SCR &= ~SCB_SCR_SLEEPDEEP;

	mp_uart_wake(NO);

	_mp_rtc_sync();
	elapsed = (_mp_rtc_count() - start) * 1000UL + __rtcCarry;
	__rtcCarry = elapsed % _RTC_HZ;
	__ticks += elapsed / _RTC_HZ;

	/* the tick that was running goes on where it stopped */
	if(left < 2)
		left = 2;
	SysTick->LOAD = left - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SysTick_CTRL_ENABLE;
	SysTick->LOAD = __tickCounts - 1;

	kernel->power.deep++;
	return(YES);
}
#endif

/* masked: SysTick stretched to idle ticks, as FreeRTOS does */
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
	/* a tick came while stopping: its interrupt counts it, the next pass sleeps */
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
		/* all of it went by: the pending interrupt counts the last tick */
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

mp_ret_t mp_clock_init(mp_kernel_t *kernel) {
	/* HSI, the tree reset leaves */
	RCC->CR |= RCC_CR_HSION;
	while(!(RCC->CR & RCC_CR_HSIRDY));
	_mp_clock_switch(0);
	RCC->CR &= ~RCC_CR_PLLON;
	_mp_clock_bus(_HSI_HZ);

	/* kernel tick */
	SysTick->CTRL = 0;
	SysTick->LOAD = __tickCounts - 1;
	SysTick->VAL = 0;
	SCB->SHP[11] = 0x40;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE | SysTick_CTRL_TICKINT | SysTick_CTRL_ENABLE;

	/* nothing is open yet: always quiet */
	mp_clock_level(kernel, MP_POWER_HIGH);

	/* cycle counter for mp_clock_nanoDelay() */
	DEMCR |= DEMCR_TRCENA;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA;

#ifdef MP_CLOCK_LSE
	_mp_clock_lse_start();
#endif

	return(TRUE);
}

mp_ret_t mp_clock_fini(mp_kernel_t *kernel) {
	SysTick->CTRL = 0;
#ifdef MP_CLOCK_LSE
	if(__rtc == YES) {
		mp_interrupt_unset(RTCAlarm_IRQn);
		EXTI->IMR &= ~(1UL << EXTI_LINE_RTC_ALARM);
		__rtc = NO;
	}
#endif
	return(TRUE);
}

void mp_clock_reset(mp_kernel_t *kernel) {
}

/**
 * @brief Sleep until the next deadline or an interrupt
 *
 * WFI with SysTick stretched. Stop stays off: it cuts this board.
 *
 * @param[in] kernel Kernel handler
 * @param[in] limit Ticks the sleep can not exceed, a hold expires then
 */
void mp_clock_schedule(mp_kernel_t *kernel, unsigned long limit) {
	unsigned long idle;

#ifdef MP_CLOCK_LSE
	_mp_clock_lse_poll();
#endif

	/* masked so a wake up between the test and WFI is not lost: WFI still returns on it */
	__asm volatile ("cpsid i" ::: "memory");
	idle = mp_task_idle(&kernel->tasks);
	if(limit < idle)
		idle = limit;
	if(idle > 0) {
		mp_adc_idle();
#ifdef MP_CLOCK_LSE
		if(idle < MP_POWER_STOP_MIN || mp_power_deep(kernel) == NO || _mp_clock_quiet() == NO || _mp_clock_stop(kernel, idle) == NO)
#endif
			_mp_clock_sleep(idle);
	}
	__asm volatile ("cpsie i" ::: "memory");
}

/**
 * @brief What the deepest sleep is
 *
 * @return A name for the Power page
 */
const char *mp_clock_deep(void) {
#ifdef MP_CLOCK_LSE
	if(__lseWaiting == YES)
		return("WFI, LSE starting");
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

/**
 * @brief Microseconds since boot, wraps after 71 minutes
 *
 * @return µs
 */
unsigned long mp_clock_us(void) {
	unsigned long ticks;
	unsigned long val;
	unsigned long counts;

	MP_INTERRUPT_SAFE_BEGIN
	ticks = __ticks;
	val = SysTick->VAL;
	counts = __tickCounts;
	/* wrapped, its interrupt did not run yet */
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
	if((unsigned int)clock >= sizeof(__names) / sizeof(__names[0]))
		return("unknown");
	return(__names[clock]);
}

mp_bool_t mp_clock_usb48(void) {
	if(__plan.sw != 2)
		return(NO);
	return(__plan.hz == 48000000UL || __plan.hz == 72000000UL ? YES : NO);
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
