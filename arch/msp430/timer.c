/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2015  Michael VERGOZ                                      *
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

static void _mp_timer_interruptDispatch(void *user);

/**
@defgroup mpArchTiMSP430Timer MSP430 Timer

@ingroup mpArchTiMSP430

@brief Timer for Ti MSP430 F5xx/F6xx

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015


@{

*/

static mp_list_t _mcu_timer;

mp_ret_t mp_timer_init(mp_kernel_t *kernel) {
	mp_list_init(&_mcu_timer);
	return(TRUE);
}

void mp_timer_fini(mp_kernel_t *kernel) {


}


/*
 * gate
 * frequency
 * precision
 * mode : up, cont, updown
 */
mp_ret_t mp_timer_create(mp_kernel_t *kernel, mp_timer_t *timer, mp_options_t *options, char *who) {
	char *value;

	memset(timer, 0, sizeof(*timer));

	timer->kernel = kernel;

	/* get gate Id*/
	value = mp_options_get(options, "gate");
	if(!value)
		return(FALSE);
	timer->gate = mp_gate_handle(value, "Timer");
	if(timer->gate == NULL) {
		mp_printk("Timer needs a valid gate for %s", who);
		return(FALSE);
	}

	/* get frequency */
	value = mp_options_get(options, "frequency");
	if(!value)
		return(FALSE);
	timer->frequency = atol(value);
	if(timer->frequency == 0) {
		mp_printk("Timer needs a valid frequency for %s", who);
		return(FALSE);
	}

	/* get precision */
	value = mp_options_get(options, "precision");
	if(value) {
		timer->precision = atoi(value);

		if(timer->precision == 0) {
			mp_printk("Timer needs a valid precision for %s", who);
			return(FALSE);
		}
	}
	else {
		/* set precision to 1 */
		timer->precision = 1;
	}

	/* get frequency */
	mp_pwm_mode_t mode = MP_PWM_MODE_UP;
	value = mp_options_get(options, "mode");
	if(value) {
		if(strcmp(value, "up") == 0)
			mode = MP_PWM_MODE_UP;
		else if(strcmp(value, "cont") == 0)
			mode = MP_PWM_MODE_CONT;
		else if(strcmp(value, "updown") == 0)
			mode = MP_PWM_MODE_UPDOWN;
	}

	/* prepare alignement */
	timer->regCTL = (unsigned short *)timer->gate->_baseAddress;
	timer->regCCTL0 = (unsigned short *)(timer->gate->_baseAddress+0x2);
	timer->regCCR0 = (unsigned short *)(timer->gate->_baseAddress+0x12);
	timer->regIV = (unsigned short *)(timer->gate->_baseAddress+0x2e);

	/* ensure timer is off */
	*timer->regCTL = 0;

	/* Run the timer off of the ACLK. */
	*timer->regCTL = TASSEL_1 | ID_0;

	/* Clear everything to start with. */
	*timer->regCTL |= TACLR;

	/* source from auxi clock */
	if(timer->frequency <= ACLK_FREQ_HZ && ACLK_FREQ_HZ/timer->frequency >= timer->precision) {
		*timer->regCCR0 = ACLK_FREQ_HZ/timer->frequency;
		*timer->regCTL = TASSEL_1 + mode + TACLR;
	}
	/* source from DCO */
	else {
		*timer->regCCR0 = mp_clock_get_speed()/timer->frequency;
		*timer->regCTL = TASSEL_2 + mode + TACLR;
	}

	/* add the timer into the global list */
	mp_list_add_first(&_mcu_timer, &timer->item, timer);

	/* set dummy interrupt */
	mp_interrupt_set(timer->gate->_ISRVector, _mp_timer_interruptDispatch, timer, "Timer");

	return(TRUE);
}

mp_ret_t mp_timer_set_interrupt(mp_timer_t *timer, mp_timer_interrupt_t isr) {
	timer->isr = isr;
	return(TRUE);
}

mp_ret_t mp_timer_unset_interrupt(mp_timer_t *timer) {
	timer->isr = NULL;
	return(TRUE);
}

void mp_timer_destroy(mp_timer_t *timer) {
	*timer->regCCR0 = 0;
	*timer->regCTL = TASSEL_0 + MP_PWM_MODE_STOP;

	mp_timer_disable_interrupt(timer);
	mp_timer_unset_interrupt(timer);

	mp_list_remove(&_mcu_timer, &timer->item);

	mp_interrupt_unset(timer->gate->_ISRVector);
	mp_gate_release(timer->gate);

}

/*
 * port
 */
mp_ret_t mp_timer_pwm_create(mp_timer_t *timer, mp_pwm_t *pwm, mp_options_t *options, char *who) {
	char *value;

	memset(pwm, 0, sizeof(*pwm));

	pwm->timer = timer;

	/* gpio IO */
	value = mp_options_get(options, "port");
	if(!value) {
		mp_printk("PWM: needs port");
		mp_timer_pwm_destroy(pwm);
		return(FALSE);
	}

	pwm->gpio = mp_gpio_text_handle(value, "PWM gpio");
	if(!pwm->gpio) {
		mp_printk("PWM: needs valid port");
		mp_timer_pwm_destroy(pwm);
		return(FALSE);
	}
	mp_gpio_direction(pwm->gpio, MP_GPIO_OUTPUT);
	_GPIO_REG8(pwm->gpio, _GPIO_SEL) |= 1<<pwm->gpio->pin;
	mp_gpio_unset(pwm->gpio);

	mp_timer_pwm_set(pwm, 0);
	*(timer->regCCTL0+pwm->gpio->pin) = OUTMOD_6;
	*timer->regCTL |= MC_3;

	/* add the timer into the global list */
	pwm->state = MP_PWM_STATE_PENDING;
	mp_list_add_first(&timer->pending, &pwm->item, pwm);

	return(TRUE);
}

void mp_timer_pwm_destroy(mp_pwm_t *pwm) {
	if(pwm->gpio) {
		mp_timer_pwm_set(pwm, 0);
		*(pwm->timer->regCCTL0+pwm->gpio->pin) = OUTMOD_0;
		_GPIO_REG8(pwm->gpio, _GPIO_SEL) &= ~(1<<pwm->gpio->pin);
		mp_gpio_unset(pwm->gpio);
		mp_gpio_direction(pwm->gpio, MP_GPIO_INPUT);
		mp_gpio_release(pwm->gpio);
	}

	if(pwm->state == MP_PWM_STATE_PENDING)
		mp_list_remove(&pwm->timer->pending, &pwm->item);
	else if(pwm->state == MP_PWM_STATE_ACTIVE)
		mp_list_remove(&pwm->timer->active, &pwm->item);
}

void mp_timer_pwm_set(mp_pwm_t *pwm, unsigned int dutyCycle) {
	unsigned short *ptr = pwm->timer->regCCR0;
	ptr += pwm->gpio->pin;
	*ptr = dutyCycle;
}

void mp_timer_pwm_set_percentil(mp_pwm_t *pwm, unsigned int dutyCycle) {
	unsigned short *ptr = pwm->timer->regCCR0;
	unsigned long duty = (*ptr);
	duty *= dutyCycle;
	duty /= 100;
	ptr += pwm->gpio->pin;
	*ptr = duty;
}

/** @} */

static void _mp_timer_interruptDispatch(void *user) {
	mp_timer_t *timer = user;

	/* master timer receiver */
	if(timer->isr)
		timer->isr(timer);

	/* follow active pwm interupt */
}

