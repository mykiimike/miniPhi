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

#ifndef _HAVE_MSP430_TIMER_H
	#define _HAVE_MSP430_TIMER_H

	typedef struct mp_timer_s mp_timer_t;
	typedef struct mp_pwm_s mp_pwm_t;

	typedef void (*mp_timer_interrupt_t)(mp_timer_t *timer);
	typedef void (*mp_pwm_interrupt_t)(mp_pwm_t *pwm);

	typedef enum {
		MP_PWM_MODE_STOP = MC_0,
		MP_PWM_MODE_UP = MC_1,
		MP_PWM_MODE_CONT = MC_2,
		MP_PWM_MODE_UPDOWN = MC_3,
	} mp_pwm_mode_t;

	typedef enum {
		MP_PWM_STATE_PENDING = 1,
		MP_PWM_STATE_ACTIVE = 2
	} mp_pwm_state_t;

	struct mp_pwm_s {
		mp_timer_t *timer;
		char *who;
		void *user;

		unsigned int dutyCycle;

		mp_gpio_port_t *gpio;

		mp_pwm_interrupt_t isr;

		mp_pwm_state_t state;

		/** Linked list */
		mp_list_item_t item;
	};

	struct mp_timer_s {
		/** Related kernel information */
		mp_kernel_t *kernel;

		/** Who own the timer */
		char *who;

		/* precision set to 1 for the moment */
		int precision;

		/** Timer frequency */
		unsigned long frequency;

		/** Embedded user pointer */
		void *user;

		/** Associated gate */
		mp_gate_t *gate;

		/** active PWM */
		mp_list_t active;

		/** Pending PWM */
		mp_list_t pending;

		mp_timer_interrupt_t isr;

		/** Linked list */
		mp_list_item_t item;

		/* MSP430 dependent */
		unsigned short *regCTL;
		unsigned short *regCCTL0;
		unsigned short *regCCR0;
		unsigned short *regIV;
	};

	mp_ret_t mp_timer_init(mp_kernel_t *kernel);
	void mp_timer_fini(mp_kernel_t *kernel);

	mp_ret_t mp_timer_create(mp_kernel_t *kernel, mp_timer_t *timer, mp_options_t *options, char *who);
	void mp_timer_destroy(mp_timer_t *timer);

	mp_ret_t mp_timer_set_interrupt(mp_timer_t *timer, mp_timer_interrupt_t isr);
	mp_ret_t mp_timer_unset_interrupt(mp_timer_t *timer);

	mp_ret_t mp_timer_pwm_create(mp_timer_t *timer, mp_pwm_t *pwm, mp_options_t *options, char *who);
	void mp_timer_pwm_destroy(mp_pwm_t *pwm);
	void mp_timer_pwm_set(mp_pwm_t *pwm, unsigned int dutyCycle);
	void mp_timer_pwm_set_percentil(mp_pwm_t *pwm, unsigned int dutyCycle);

	static inline int mp_timer_pwm_get_precision(mp_pwm_t *pwm) {
		return(*pwm->timer->regCCR0);
	}

	static inline void mp_timer_enable_interrupt(mp_timer_t *timer) {
		*timer->regCCTL0 |= CCIE;
		*timer->regCTL |= TACLR;
	}

	static inline void mp_timer_disable_interrupt(mp_timer_t *timer) {
		*timer->regCCTL0 &= ~(CCIE);
	}
#endif
