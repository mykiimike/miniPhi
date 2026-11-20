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
@defgroup mpArchRP2350Timer Timer

@ingroup mpArchRP2350

The kernel tick stays SysTick, and TIMER0 stays the microsecond counter.
A timer is one PWM slice, clocked by clk_sys. The pin names the slice
(GPIO n is slice (n/2) mod 8, channel B when n is odd), so create does
not take a gate. Both channels of a slice share one period. A count at
or above the period holds the output high, which stays high if clk_sys
stops. mp_timer_retime() keeps each duty's share when the governor moves
clk_sys.

@{
*/

#define _MP_PWM_SLICES 8
#define _MP_PWM_NONE   0xFF

static mp_list_t _mp_timers;
static mp_timer_t *_mp_slice_timer[_MP_PWM_SLICES];
static unsigned char _mp_slice_used[_MP_PWM_SLICES];
static unsigned char _mp_slice_inv[_MP_PWM_SLICES];
static int _mp_pwm_ready;
static int _mp_pwm_irq;

static int _mp_pwm_bound(mp_timer_t *timer) {
	return(timer && timer->slice < _MP_PWM_SLICES);
}

static int _mp_pwm_reset(void) {
	unsigned int spins = 100000;

	if(_mp_pwm_ready)
		return(1);
	RESETS_RESET &= ~RESETS_PWM;
	while(spins-- && !(RESETS_RESET_DONE & RESETS_PWM))
		;
	if(!(RESETS_RESET_DONE & RESETS_PWM))
		return(0);
	_mp_pwm_ready = 1;
	return(1);
}

/* period counts and a divider in sixteenths. period stays in 16 bits so a
 * duty equal to it can sit one above TOP and hold the pin high. */
static int _mp_pwm_fit(mp_timer_t *timer, unsigned long *div16, unsigned long *period) {
	unsigned long clk = mp_clock_get_speed();
	unsigned long cycles;
	unsigned long span;
	unsigned long div;
	unsigned long asked;

	if(!clk || !timer->frequency)
		return(0);
	cycles = clk / timer->frequency;
	if(timer->mode == MP_PWM_MODE_UPDOWN && cycles > 1)
		cycles /= 2;
	asked = timer->steps > 1 ? (unsigned long)timer->steps : 2;
	if(cycles < asked)
		return(0);
	span = cycles > 65535UL ? 65535UL : cycles;
	div = (cycles * 16UL + span / 2UL) / span;
	if(div < 16UL)
		div = 16UL;
	if(div > 256UL * 16UL)
		return(0);
	span = (cycles * 16UL) / div;
	if(span < asked)
		span = asked;
	if(span > 65535UL)
		span = 65535UL;
	if(span < 2UL)
		return(0);
	div = (cycles * 16UL + span / 2UL) / span;
	if(div < 16UL)
		div = 16UL;
	if(div > 256UL * 16UL)
		return(0);
	*div16 = div;
	*period = span;
	return(1);
}

static void _mp_pwm_div(int slice, unsigned long div16) {
	unsigned long integ = div16 / 16UL;
	unsigned long frac = div16 % 16UL;

	if(integ >= 256UL) {
		integ = 0;
		frac = 0;
	}
	PWM_DIV(slice) = (uint32_t)((integ << 4) | frac);
}

static void _mp_pwm_csr(int slice, mp_pwm_mode_t mode) {
	uint32_t csr = PWM_CSR_EN;

	if(mode == MP_PWM_MODE_UPDOWN)
		csr |= PWM_CSR_PH_CORRECT;
	if(_mp_slice_inv[slice] & 1)
		csr |= PWM_CSR_A_INV;
	if(_mp_slice_inv[slice] & 2)
		csr |= PWM_CSR_B_INV;
	PWM_CSR(slice) = csr;
}

static void _mp_pwm_level(int slice, int channel, unsigned int duty) {
	uint32_t cc = PWM_CC(slice);

	if(duty > 65535U)
		duty = 65535U;
	if(channel)
		cc = (cc & 0x0000FFFFUL) | ((uint32_t)duty << 16);
	else
		cc = (cc & 0xFFFF0000UL) | (duty & 0xFFFFU);
	PWM_CC(slice) = cc;
}

static void _mp_pwm_dispatch(void *user) {
	uint32_t fired = PWM_IRQ0_INTS;
	mp_list_item_t *item;

	(void)user;
	PWM_INTR = fired;
	for(item = _mp_timers.first; item; item = item->next) {
		mp_timer_t *timer = item->user;

		if(_mp_pwm_bound(timer) && (fired & (1UL << timer->slice)) && timer->isr)
			timer->isr(timer);
	}
}

static void _mp_pwm_apply(mp_timer_t *timer) {
	unsigned long div16;
	unsigned long period;
	unsigned long before;
	mp_list_item_t *seek;

	if(!_mp_pwm_bound(timer))
		return;
	if(!_mp_pwm_fit(timer, &div16, &period))
		return;
	before = (unsigned long)timer->precision;
	if(before < 1)
		before = period;

	MP_INTERRUPT_SAFE_BEGIN
	_mp_pwm_div(timer->slice, div16);
	PWM_TOP(timer->slice) = (uint32_t)(period - 1UL);
	for(seek = timer->pending.first; seek; seek = seek->next) {
		mp_pwm_t *pwm = seek->user;
		unsigned long duty = pwm->dutyCycle;

		/* at or above the old period the pin was held high */
		if(duty >= before)
			duty = period;
		else
			duty = (unsigned long)((unsigned long long)duty * period / before);
		if(duty > 65535UL)
			duty = 65535UL;
		pwm->dutyCycle = (unsigned int)duty;
		_mp_pwm_level(timer->slice, pwm->channel, (unsigned int)duty);
	}
	timer->precision = (int)period;
	_mp_pwm_csr(timer->slice, timer->mode);
	MP_INTERRUPT_SAFE_END
}

mp_ret_t mp_timer_init(mp_kernel_t *kernel) {
	(void)kernel;
	mp_list_init(&_mp_timers);
	return(TRUE);
}

void mp_timer_fini(mp_kernel_t *kernel) {
	(void)kernel;
}

mp_ret_t mp_timer_create(mp_kernel_t *kernel, mp_timer_t *timer, mp_options_t *options, char *who) {
	unsigned long div16;
	unsigned long period;
	char *value;

	memset(timer, 0, sizeof(*timer));
	timer->kernel = kernel;
	timer->who = who;
	timer->slice = _MP_PWM_NONE;
	timer->mode = MP_PWM_MODE_UP;
	mp_list_init(&timer->active);
	mp_list_init(&timer->pending);

	value = mp_options_get(options, "frequency");
	timer->frequency = value ? strtoul(value, NULL, 10) : 0;
	if(timer->frequency == 0) {
		mp_printk("Timer needs a valid frequency for %s", who);
		timer->kernel = NULL;
		return(FALSE);
	}

	value = mp_options_get(options, "precision");
	timer->precision = value ? atoi(value) : 0;
	if(timer->precision <= 1) {
		mp_printk("Timer needs a valid precision for %s", who);
		timer->kernel = NULL;
		return(FALSE);
	}
	timer->steps = timer->precision;

	value = mp_options_get(options, "mode");
	if(value && strcmp(value, "cont") == 0)
		timer->mode = MP_PWM_MODE_CONT;
	else if(value && strcmp(value, "updown") == 0)
		timer->mode = MP_PWM_MODE_UPDOWN;

	if(!_mp_pwm_fit(timer, &div16, &period)) {
		mp_printk("Timer %s can not give %d steps at %lu Hz", who, timer->steps, timer->frequency);
		timer->kernel = NULL;
		return(FALSE);
	}
	(void)div16;
	(void)period;

	mp_list_add_first(&_mp_timers, &timer->item, timer);
	return(TRUE);
}

void mp_timer_destroy(mp_timer_t *timer) {
	if(!timer || !timer->kernel)
		return;

	while(timer->pending.first)
		mp_timer_pwm_destroy(timer->pending.first->user);

	mp_timer_disable_interrupt(timer);
	if(_mp_pwm_bound(timer)) {
		PWM_CSR(timer->slice) = 0;
		_mp_slice_timer[timer->slice] = NULL;
		_mp_slice_used[timer->slice] = 0;
		_mp_slice_inv[timer->slice] = 0;
		timer->slice = _MP_PWM_NONE;
	}
	mp_list_remove(&_mp_timers, &timer->item);
	timer->kernel = NULL;
}

void mp_timer_enable_interrupt(mp_timer_t *timer) {
	if(!_mp_pwm_bound(timer))
		return;
	if(!_mp_pwm_irq) {
		if(!mp_interrupt_set(PWM_IRQ_WRAP_0, _mp_pwm_dispatch, NULL, "PWM"))
			return;
		_mp_pwm_irq = 1;
	}
	PWM_INTR = 1UL << timer->slice;
	PWM_IRQ0_INTE |= 1UL << timer->slice;
}

void mp_timer_disable_interrupt(mp_timer_t *timer) {
	if(!_mp_pwm_bound(timer))
		return;
	PWM_IRQ0_INTE &= ~(1UL << timer->slice);
}

mp_ret_t mp_timer_set_interrupt(mp_timer_t *timer, mp_timer_interrupt_t isr) {
	if(!timer || !timer->kernel)
		return(FALSE);
	timer->isr = isr;
	mp_timer_enable_interrupt(timer);
	return(TRUE);
}

mp_ret_t mp_timer_unset_interrupt(mp_timer_t *timer) {
	if(!timer || !timer->kernel)
		return(FALSE);
	mp_timer_disable_interrupt(timer);
	timer->isr = NULL;
	return(TRUE);
}

void mp_timer_retime(void) {
	mp_list_item_t *item;

	for(item = _mp_timers.first; item; item = item->next)
		_mp_pwm_apply(item->user);
}

static void _mp_pwm_drop(mp_timer_t *timer, int slice, int channel) {
	_mp_slice_used[slice] &= (unsigned char)~(1U << channel);
	_mp_slice_inv[slice] &= (unsigned char)~(1U << channel);
	if(_mp_slice_used[slice]) {
		_mp_pwm_csr(slice, timer->mode);
		return;
	}
	PWM_CSR(slice) = 0;
	_mp_slice_timer[slice] = NULL;
	_mp_slice_inv[slice] = 0;
	if(timer->slice == (unsigned char)slice)
		timer->slice = _MP_PWM_NONE;
}

mp_ret_t mp_timer_pwm_create(mp_timer_t *timer, mp_pwm_t *pwm, mp_options_t *options, char *who) {
	unsigned long div16;
	unsigned long period;
	char *port;
	char *value;
	int slice;
	int channel;
	int first;

	if(!timer || !timer->kernel)
		return(FALSE);
	memset(pwm, 0, sizeof(*pwm));
	pwm->timer = timer;
	pwm->who = who;

	port = mp_options_get(options, "port");
	if(!port) {
		mp_printk("PWM: needs port");
		return(FALSE);
	}
	pwm->gpio = mp_gpio_text_handle(port, who);
	if(!pwm->gpio) {
		mp_printk("PWM: needs valid port");
		return(FALSE);
	}

	slice = (int)((pwm->gpio->pin >> 1) & 7U);
	channel = (int)(pwm->gpio->pin & 1U);
	if(_mp_slice_timer[slice] && _mp_slice_timer[slice] != timer) {
		mp_printk("PWM: %s is on a slice already timed", port);
		mp_gpio_release(pwm->gpio);
		pwm->gpio = NULL;
		return(FALSE);
	}
	if(timer->slice != _MP_PWM_NONE && timer->slice != (unsigned char)slice) {
		mp_printk("PWM: %s is not on the slice this timer already drives", port);
		mp_gpio_release(pwm->gpio);
		pwm->gpio = NULL;
		return(FALSE);
	}
	if(_mp_slice_used[slice] & (1U << channel)) {
		mp_printk("PWM: %s channel is already used", port);
		mp_gpio_release(pwm->gpio);
		pwm->gpio = NULL;
		return(FALSE);
	}
	if(!_mp_pwm_reset()) {
		mp_printk("PWM: block stayed in reset");
		mp_gpio_release(pwm->gpio);
		pwm->gpio = NULL;
		return(FALSE);
	}

	first = _mp_slice_used[slice] == 0;
	_mp_slice_used[slice] |= (unsigned char)(1U << channel);
	_mp_slice_timer[slice] = timer;
	timer->slice = (unsigned char)slice;
	pwm->channel = (unsigned char)channel;

	value = mp_options_get(options, "polarity");
	if(value && mp_options_cmp(value, "low") == TRUE)
		_mp_slice_inv[slice] |= (unsigned char)(1U << channel);
	else
		_mp_slice_inv[slice] &= (unsigned char)~(1U << channel);

	if(first) {
		if(!_mp_pwm_fit(timer, &div16, &period)) {
			mp_printk("PWM: %s can not be timed at %lu Hz", port, timer->frequency);
			_mp_pwm_drop(timer, slice, channel);
			mp_gpio_release(pwm->gpio);
			pwm->gpio = NULL;
			return(FALSE);
		}
		_mp_pwm_div(slice, div16);
		PWM_TOP(slice) = (uint32_t)(period - 1UL);
		timer->precision = (int)period;
	}
	_mp_pwm_level(slice, channel, 0);
	_mp_pwm_csr(slice, timer->mode);

	mp_gpio_af(pwm->gpio, IO_GPIO_FUNCSEL_PWM);
	mp_gpio_mode(pwm->gpio, MP_GPIO_MODE_AF);

	pwm->state = MP_PWM_STATE_PENDING;
	mp_list_add_first(&timer->pending, &pwm->item, pwm);
	if(timer->isr)
		mp_timer_enable_interrupt(timer);
	return(TRUE);
}

void mp_timer_pwm_destroy(mp_pwm_t *pwm) {
	mp_timer_t *timer;
	int slice;

	if(!pwm || !pwm->gpio || !pwm->timer)
		return;
	timer = pwm->timer;
	slice = timer->slice;
	if(slice < _MP_PWM_SLICES)
		_mp_pwm_level(slice, pwm->channel, 0);
	if(slice < _MP_PWM_SLICES)
		_mp_pwm_drop(timer, slice, pwm->channel);
	mp_gpio_release(pwm->gpio);
	pwm->gpio = NULL;
	mp_list_remove(&timer->pending, &pwm->item);
}

void mp_timer_pwm_set(mp_pwm_t *pwm, unsigned int dutyCycle) {
	if(!pwm)
		return;
	if(dutyCycle > 65535U)
		dutyCycle = 65535U;
	pwm->dutyCycle = dutyCycle;
	if(pwm->timer && _mp_pwm_bound(pwm->timer))
		_mp_pwm_level(pwm->timer->slice, pwm->channel, dutyCycle);
}

void mp_timer_pwm_set_percentil(mp_pwm_t *pwm, unsigned int dutyCycle) {
	unsigned long duty;

	if(!pwm || !pwm->timer)
		return;
	if(dutyCycle > 100)
		dutyCycle = 100;
	duty = (unsigned long)mp_timer_get_precision(pwm->timer) * dutyCycle / 100UL;
	mp_timer_pwm_set(pwm, (unsigned int)duty);
}

/**@}*/
