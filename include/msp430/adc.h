/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

#ifndef _HAVE_MSP430_ADC_H
	#define _HAVE_MSP430_ADC_H


	typedef struct mp_adc_s mp_adc_t;
	typedef void (*mp_adc_callback_t)(mp_adc_t *adc);

	struct mp_adc_s {
		mp_gpio_port_t *port;
		mp_kernel_t *kernel;

		char state;

		char ctlId;

		char channelId;

		int result;
		mp_adc_callback_t callback;
		mp_task_t *task;

		mp_list_item_t item;
	};

	mp_ret_t mp_adc_create(mp_kernel_t *kernel, mp_adc_t *adc, mp_options_t *options, const char *who);
	mp_ret_t mp_adc_remove(mp_adc_t *adc);

	void mp_adc_enable(mp_adc_t *adc);
	void mp_adc_disable(mp_adc_t *adc);
	void mp_adc_start();
	void mp_adc_stop();
	mp_ret_t mp_adc_state();
	void mp_adc_restore(mp_bool_t status);

#endif

