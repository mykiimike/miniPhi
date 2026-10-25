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

#ifndef _HAVE_STM32_ADC_H
	#define _HAVE_STM32_ADC_H

	/* F1: channel 16 is the die temperature, 17 the 1.20 V reference.
	   F4: channel 17 is VREFINT, 18 the temperature sensor. */
	#if defined(STM32F407xx)
		#define MP_ADC_CHANNEL_TEMP  18
		#define MP_ADC_CHANNEL_VREF  17
		#define MP_ADC_CHANNEL_MAX   18
	#else
		#define MP_ADC_CHANNEL_TEMP  16
		#define MP_ADC_CHANNEL_VREF  17
		#define MP_ADC_CHANNEL_MAX   17
	#endif

	typedef struct mp_adc_s mp_adc_t;

	typedef void (*mp_adc_callback_t)(mp_adc_t *adc);

	struct mp_adc_s {
		mp_gpio_port_t *port;

		mp_kernel_t *kernel;

		char state;

		char channelId;

		/* 12-bit result, 0 to 4095 of VDDA */
		int result;

		mp_adc_callback_t callback;

		void *user;

		mp_task_t *task;

		mp_list_item_t item;
	};

	/* options: channel "A0" to "A17", port (puts the pin in analog mode), delay between captures in ticks */
	mp_ret_t mp_adc_create(mp_kernel_t *kernel, mp_adc_t *adc, mp_options_t *options, const char *who);
	mp_ret_t mp_adc_remove(mp_adc_t *adc);
	void mp_adc_enable(mp_adc_t *adc);
	void mp_adc_disable(mp_adc_t *adc);
	void mp_adc_start();
	void mp_adc_stop();
	mp_ret_t mp_adc_state();
	void mp_adc_restore(mp_bool_t status);

	/** keeps ADC1 on when no channel owns it; paired with mp_adc_close() */
	mp_ret_t mp_adc_open(mp_kernel_t *kernel);
	void mp_adc_close(void);

	/**
	 * One polled conversion on ADC1, 0 to 4095.
	 * -1 when the channel is invalid or a conversion is already running.
	 */
	int mp_adc_convert(int channel);

	/**
	 * ADC1 channel of a GPIO pin.
	 * F1: PA0-PA7 are IN0-IN7, PB0-PB1 are IN8-IN9, PC0-PC5 are IN10-IN15.
	 * -1 when the pin is not an ADC input.
	 */
	int mp_adc_channel(mp_gpio_port_t *pin);

	void mp_adc_idle(void);
	mp_bool_t mp_adc_quiet(void);

	/* keeps the ADC clock at or under 36 MHz. The F1 sets this in the clock tree. */
	void mp_adc_prescale(unsigned long pclk2);

#endif
