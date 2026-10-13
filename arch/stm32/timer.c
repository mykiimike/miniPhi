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

The update interrupt calls timer->isr once per period. PWM uses the
remap 0 channel pins in PWM mode 1, so the duty cycle is the high time.

@{
*/

typedef struct {
	const char *gate;
	const char *pins[4];
} _mp_timer_channels_t;

static const _mp_timer_channels_t __channels[] = {
	{ "TIM1", { "pa8", "pa9", "pa10", "pa11" } },
	{ "TIM2", { "pa0", "pa1", "pa2", "pa3" } },
	{ "TIM3", { "pa6", "pa7", "pb0", "pb1" } },
	{ "TIM4", { "pb6", "pb7", "pb8", "pb9" } },
#if defined(STM32F10X_HD) || defined(STM32F10X_XL)
	{ "TIM5", { "pa0", "pa1", "pa2", "pa3" } },
	{ "TIM8", { "pc6", "pc7", "pc8", "pc9" } },
#endif
	{ NULL }
};

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

mp_ret_t mp_timer_create(mp_kernel_t *kernel, mp_timer_t *timer, mp_options_t *options, char *who) {
	unsigned long total;
	unsigned long psc;
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

	/* counts per period, halved when counting up and down */
	total = _mp_timer_clock(timer) / timer->frequency;
	if(mode == MP_PWM_MODE_UPDOWN)
		total /= 2;
	if(total < (unsigned long)timer->precision) {
		mp_printk("Timer %s can not give %d steps at %lu Hz", timer->gate->portDevice, timer->precision, timer->frequency);
		mp_gate_release(timer->gate);
		return(FALSE);
	}

	psc = (total - 1) / 65536;
	regs->CR1 = 0;
	regs->PSC = psc;
	regs->ARR = mode == MP_PWM_MODE_CONT ? 0xFFFF : total / (psc + 1) - 1;
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

static int _mp_timer_channel(mp_timer_t *timer, mp_gpio_port_t *gpio) {
	const _mp_timer_channels_t *map;
	char name[5];
	int a;

	snprintf(name, sizeof(name), "p%c%u", 'a' + gpio->port, gpio->pin);

	for(map = __channels; map->gate; map++) {
		if(mp_options_cmp((char *)map->gate, timer->gate->portDevice) == FALSE)
			continue;
		for(a=0; a<4; a++)
			if(mp_options_cmp((char *)map->pins[a], name) == TRUE)
				return(a);
	}
	return(-1);
}

mp_ret_t mp_timer_pwm_create(mp_timer_t *timer, mp_pwm_t *pwm, mp_options_t *options, char *who) {
	TIM_TypeDef *regs = timer->regs;
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

	channel = _mp_timer_channel(timer, pwm->gpio);
	if(channel < 0) {
		mp_printk("PWM: %s is not a %s channel", value, timer->gate->portDevice);
		mp_gpio_release(pwm->gpio);
		pwm->gpio = NULL;
		return(FALSE);
	}
	pwm->channel = channel;

	regs->CCR[channel] = 0;

	/* PWM mode 1 with preload */
	ccmr = channel < 2 ? &regs->CCMR1 : &regs->CCMR2;
	shift = (channel & 1) * 8;
	*ccmr = (*ccmr & ~(0xFFUL << shift)) | ((6UL << 4 | 1UL << 3) << shift);
	regs->CCER |= 1UL << (channel * 4);
	if(regs == TIM1)
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
