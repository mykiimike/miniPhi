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
@defgroup mpArchSTM32Timer Timers and PWM

@ingroup mpArchSTM32

The update interrupt calls timer->isr once per period. PWM runs in PWM
mode 1, so the duty cycle is the active time; option polarity "low"
makes the active level low. A channel pin picks the remap of its timer:
the first PWM decides it, a PWM on a pin of another remap is refused.

@{
*/

typedef struct {
	const char *gate;
	const char *pins[4];

	/* AFIO_MAPR field of the timer, and its value for these pins */
	uint32_t mask;
	uint32_t remap;
} _mp_timer_channels_t;

static const _mp_timer_channels_t __channels[] = {
	{ "TIM1", { "pa8", "pa9", "pa10", "pa11" }, 3UL << 6, 0 },
	{ "TIM1", { "pe9", "pe11", "pe13", "pe14" }, 3UL << 6, 3UL << 6 },
	{ "TIM2", { "pa0", "pa1", "pa2", "pa3" }, 3UL << 8, 0 },
	{ "TIM2", { "pa15", "pb3", "pb10", "pb11" }, 3UL << 8, 3UL << 8 },
	{ "TIM3", { "pa6", "pa7", "pb0", "pb1" }, 3UL << 10, 0 },
	{ "TIM3", { "pb4", "pb5", "pb0", "pb1" }, 3UL << 10, 2UL << 10 },
	{ "TIM3", { "pc6", "pc7", "pc8", "pc9" }, 3UL << 10, 3UL << 10 },
	{ "TIM4", { "pb6", "pb7", "pb8", "pb9" }, 1UL << 12, 0 },
	{ "TIM4", { "pd12", "pd13", "pd14", "pd15" }, 1UL << 12, 1UL << 12 },
#if defined(STM32F10X_HD) || defined(STM32F10X_XL)
	{ "TIM5", { "pa0", "pa1", "pa2", "pa3" }, 0, 0 },
	{ "TIM8", { "pc6", "pc7", "pc8", "pc9" }, 0, 0 },
#endif
	{ NULL }
};

/* SWJ_CFG reads back undefined: every write of MAPR puts the debug port again */
#ifdef _SWD_ONLY
	#define _SWJ AFIO_MAPR_SWJ_SWD
#else
	#define _SWJ 0
#endif

static mp_list_t _mcu_timer;

static void _mp_timer_interruptDispatch(void *user);

/* APB timers run at twice PCLK when the bus is divided */
static unsigned long _mp_timer_clock(mp_timer_t *timer) {
	unsigned long pclk = mp_gate_pclk(timer->gate);
	return(pclk == mp_clock_hclk() ? pclk : pclk * 2);
}

mp_ret_t mp_timer_init(mp_kernel_t *kernel) {
	mp_list_init(&_mcu_timer);
	return(TRUE);
}

void mp_timer_fini(mp_kernel_t *kernel) {
	while(_mcu_timer.first)
		mp_timer_destroy(_mcu_timer.first->user);
}

/* counts per period, halved when counting up and down */
static unsigned long _mp_timer_total(mp_timer_t *timer) {
	unsigned long total = _mp_timer_clock(timer) / timer->frequency;

	if(timer->mode == MP_PWM_MODE_UPDOWN)
		total /= 2;
	return(total);
}

static void _mp_timer_period(mp_timer_t *timer, unsigned long total, unsigned long *psc, unsigned long *arr) {
	*psc = (total - 1) / 65536;
	*arr = timer->mode == MP_PWM_MODE_CONT ? 0xFFFF : total / (*psc + 1) - 1;
}

mp_ret_t mp_timer_create(mp_kernel_t *kernel, mp_timer_t *timer, mp_options_t *options, char *who) {
	unsigned long total;
	unsigned long psc;
	unsigned long arr;
	TIM_TypeDef *regs;
	mp_pwm_mode_t mode = MP_PWM_MODE_UP;
	char *value;

	memset(timer, 0, sizeof(*timer));
	timer->kernel = kernel;
	timer->who = who;

	value = mp_options_get(options, "frequency");
	timer->frequency = value ? strtoul(value, NULL, 10) : 0;
	if(timer->frequency == 0) {
		mp_printk("Timer needs a valid frequency for %s", who);
		return(FALSE);
	}

	value = mp_options_get(options, "precision");
	timer->precision = value ? atoi(value) : 1;
	if(timer->precision <= 0) {
		mp_printk("Timer needs a valid precision for %s", who);
		return(FALSE);
	}

	value = mp_options_get(options, "mode");
	if(value && strcmp(value, "cont") == 0)
		mode = MP_PWM_MODE_CONT;
	else if(value && strcmp(value, "updown") == 0)
		mode = MP_PWM_MODE_UPDOWN;

	timer->gate = mp_gate_handle(mp_options_get(options, "gate"), who);
	if(!timer->gate) {
		mp_printk("Timer needs a valid gate for %s", who);
		return(FALSE);
	}
	timer->regs = regs = timer->gate->base;
	timer->mode = mode;

	total = _mp_timer_total(timer);
	if(total < (unsigned long)timer->precision) {
		mp_printk("Timer %s can not give %d steps at %lu Hz", timer->gate->portDevice, timer->precision, timer->frequency);
		mp_gate_release(timer->gate);
		return(FALSE);
	}

	_mp_timer_period(timer, total, &psc, &arr);
	regs->CR1 = 0;
	regs->PSC = psc;
	regs->ARR = arr;
	regs->CNT = 0;
	regs->EGR = TIM_EGR_UG;
	regs->SR = 0;
	regs->DIER = 0;
	regs->CR1 = TIM_CR1_ARPE | (mode == MP_PWM_MODE_UPDOWN ? TIM_CR1_CMS_CENTER : 0) | TIM_CR1_CEN;

	mp_list_add_first(&_mcu_timer, &timer->item, timer);

	mp_interrupt_set(timer->gate->irq, _mp_timer_interruptDispatch, timer, who);

	return(TRUE);
}

void mp_timer_destroy(mp_timer_t *timer) {
	if(!timer->gate)
		return;

	while(timer->pending.first)
		mp_timer_pwm_destroy(timer->pending.first->user);

	mp_interrupt_unset(timer->gate->irq);
	timer->regs->CR1 = 0;
	timer->regs->DIER = 0;

	mp_list_remove(&_mcu_timer, &timer->item);

	mp_gate_release(timer->gate);
	timer->gate = NULL;
}

mp_ret_t mp_timer_set_interrupt(mp_timer_t *timer, mp_timer_interrupt_t isr) {
	timer->isr = isr;
	timer->regs->SR = ~TIM_SR_UIF;
	mp_timer_enable_interrupt(timer);
	return(TRUE);
}

mp_ret_t mp_timer_unset_interrupt(mp_timer_t *timer) {
	mp_timer_disable_interrupt(timer);
	timer->isr = NULL;
	return(TRUE);
}

/**
 * @brief Recompute every timer period after the bus clocks changed
 *
 * The duty cycles keep their share of the period.
 */
void mp_timer_retime(void) {
	mp_list_item_t *item;
	mp_list_item_t *seek;
	mp_timer_t *timer;
	mp_pwm_t *pwm;
	unsigned long total;
	unsigned long psc;
	unsigned long arr;
	unsigned long before;
	unsigned long ccr;

	for(item = _mcu_timer.first; item; item = item->next) {
		timer = item->user;
		total = _mp_timer_total(timer);
		if(total < 1)
			total = 1;
		_mp_timer_period(timer, total, &psc, &arr);
		before = timer->regs->ARR + 1;

		MP_INTERRUPT_SAFE_BEGIN
		timer->regs->PSC = psc;
		timer->regs->ARR = arr;
		for(seek = timer->pending.first; seek; seek = seek->next) {
			pwm = seek->user;
			ccr = timer->regs->CCR[pwm->channel];
			/* above the period is always active */
			if(ccr >= before)
				ccr = arr + 1;
			else
				ccr = (unsigned long)((unsigned long long)ccr * (arr + 1) / before);
			timer->regs->CCR[pwm->channel] = ccr;
			pwm->dutyCycle = ccr;
		}
		/* load the preloads without an update interrupt */
		timer->regs->CR1 |= TIM_CR1_URS;
		timer->regs->EGR = TIM_EGR_UG;
		timer->regs->CR1 &= ~TIM_CR1_URS;
		MP_INTERRUPT_SAFE_END
	}
}

static const _mp_timer_channels_t *_mp_timer_channel(mp_timer_t *timer, mp_gpio_port_t *gpio, int *channel) {
	const _mp_timer_channels_t *map;
	char name[5];
	int a;

	snprintf(name, sizeof(name), "p%c%u", 'a' + gpio->port, gpio->pin);

	for(map = __channels; map->gate; map++) {
		if(mp_options_cmp((char *)map->gate, timer->gate->portDevice) == FALSE)
			continue;
		if(timer->remapped == YES && map->remap != timer->remap)
			continue;
		for(a=0; a<4; a++) {
			if(mp_options_cmp((char *)map->pins[a], name) == TRUE) {
				*channel = a;
				return(map);
			}
		}
	}
	return(NULL);
}

mp_ret_t mp_timer_pwm_create(mp_timer_t *timer, mp_pwm_t *pwm, mp_options_t *options, char *who) {
	TIM_TypeDef *regs = timer->regs;
	const _mp_timer_channels_t *map;
	volatile uint32_t *ccmr;
	unsigned int shift;
	char *value;
	int channel;

	memset(pwm, 0, sizeof(*pwm));
	pwm->timer = timer;
	pwm->who = who;

	value = mp_options_get(options, "port");
	if(!value) {
		mp_printk("PWM: needs port");
		return(FALSE);
	}

	pwm->gpio = mp_gpio_text_handle(value, who);
	if(!pwm->gpio) {
		mp_printk("PWM: needs valid port");
		return(FALSE);
	}

	map = _mp_timer_channel(timer, pwm->gpio, &channel);
	if(!map) {
		mp_printk("PWM: %s is not a %s channel%s", value, timer->gate->portDevice,
			timer->remapped == YES ? " with the remap of its other channels" : "");
		mp_gpio_release(pwm->gpio);
		pwm->gpio = NULL;
		return(FALSE);
	}
	pwm->channel = channel;

#if defined(STM32F407xx)
	{
		unsigned int af = 2;

		if(mp_options_cmp(timer->gate->portDevice, "TIM1") == TRUE || mp_options_cmp(timer->gate->portDevice, "TIM2") == TRUE)
			af = 1;
		else if(mp_options_cmp(timer->gate->portDevice, "TIM8") == TRUE)
			af = 3;
		mp_gpio_af(pwm->gpio, af);
	}
#else
	if(timer->remapped == NO && map->mask) {
		MP_INTERRUPT_SAFE_BEGIN
		AFIO->MAPR = (AFIO->MAPR & ~(map->mask | AFIO_MAPR_SWJ_CFG)) | map->remap | _SWJ;
		MP_INTERRUPT_SAFE_END
	}
#endif
	timer->remap = map->remap;
	timer->remapped = YES;

	regs->CCR[channel] = 0;

	/* PWM mode 1 with preload */
	ccmr = channel < 2 ? &regs->CCMR1 : &regs->CCMR2;
	shift = (channel & 1) * 8;
	*ccmr = (*ccmr & ~(0xFFUL << shift)) | ((6UL << 4 | 1UL << 3) << shift);
	value = mp_options_get(options, "polarity");
	if(value && mp_options_cmp(value, "low") == TRUE)
		regs->CCER |= 2UL << (channel * 4);
	else
		regs->CCER &= ~(2UL << (channel * 4));
	regs->CCER |= 1UL << (channel * 4);
	if(regs == TIM1 || regs == TIM8)
		regs->BDTR |= TIM_BDTR_MOE;

	mp_gpio_mode(pwm->gpio, MP_GPIO_MODE_AF);

	pwm->state = MP_PWM_STATE_PENDING;
	mp_list_add_first(&timer->pending, &pwm->item, pwm);

	return(TRUE);
}

void mp_timer_pwm_destroy(mp_pwm_t *pwm) {
	mp_timer_t *timer = pwm->timer;

	if(!pwm->gpio)
		return;

	timer->regs->CCER &= ~(1UL << (pwm->channel * 4));
	mp_gpio_release(pwm->gpio);
	pwm->gpio = NULL;

	mp_list_remove(&timer->pending, &pwm->item);
}

void mp_timer_pwm_set(mp_pwm_t *pwm, unsigned int dutyCycle) {
	pwm->dutyCycle = dutyCycle;
	pwm->timer->regs->CCR[pwm->channel] = dutyCycle;
}

void mp_timer_pwm_set_percentil(mp_pwm_t *pwm, unsigned int dutyCycle) {
	unsigned long duty = (unsigned long)mp_timer_get_precision(pwm->timer) * dutyCycle / 100;
	mp_timer_pwm_set(pwm, duty);
}

static void _mp_timer_interruptDispatch(void *user) {
	mp_timer_t *timer = user;

	if(!(timer->regs->SR & TIM_SR_UIF))
		return;
	timer->regs->SR = ~TIM_SR_UIF;

	if(timer->isr)
		timer->isr(timer);
}

/**@}*/
