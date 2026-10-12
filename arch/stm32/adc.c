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
@defgroup mpArchSTM32ADC ADC1

@ingroup mpArchSTM32

Same model as the MSP430: one task per channel asks the controller task
for a capture, the controller runs one single conversion at a time and
the end of conversion interrupt hands the result back.

@{
*/

static void __auto_init(mp_kernel_t *kernel);
static void __auto_fini(mp_kernel_t *kernel);
static void _mp_adc_interrupt(void *user);
MP_TASK(adcControler);
MP_TASK(adcChannel);

static mp_bool_t __isInit = NO;
static mp_list_t __list;
static mp_task_t *__controler = NULL;
static mp_adc_t * volatile __selected_channel = NULL;

mp_ret_t mp_adc_create(mp_kernel_t *kernel, mp_adc_t *adc, mp_options_t *options, const char *who) {
	int timer = 100;
	int channel;
	char *value;

	memset(adc, 0, sizeof(*adc));
	adc->kernel = kernel;
	adc->state = 0;

	value = mp_options_get(options, "channel");
	if(!value || (value[0] != 'A' && value[0] != 'a'))
		return(FALSE);
	channel = atoi(value + 1);
	if(channel < 0 || channel > MP_ADC_CHANNEL_VREF)
		return(FALSE);
	adc->channelId = channel;

	value = mp_options_get(options, "port");
	if(value) {
		adc->port = mp_gpio_text_handle(value, (char *)who);
		if(!adc->port)
			return(FALSE);
		mp_gpio_mode(adc->port, MP_GPIO_MODE_ANALOG);
	}

	value = mp_options_get(options, "delay");
	if(value)
		timer = atoi(value);

	__auto_init(kernel);
	if(__isInit == NO) {
		if(adc->port)
			mp_gpio_release(adc->port);
		return(FALSE);
	}

	adc->task = mp_task_create(&kernel->tasks, "ADC channel", adcChannel, adc, timer);
	if(!adc->task) {
		if(adc->port)
			mp_gpio_release(adc->port);
		__auto_fini(kernel);
		return(FALSE);
	}

	MP_INTERRUPT_SAFE_BEGIN
	mp_list_add_last(&__list, &adc->item, adc);
	MP_INTERRUPT_SAFE_END

	mp_printk("Creating ADC 12bits using A%d owned by %s", adc->channelId, who);

	return(TRUE);
}

mp_ret_t mp_adc_remove(mp_adc_t *adc) {
	mp_printk("Removing ADC 12bits using A%d", adc->channelId);

	MP_INTERRUPT_SAFE_BEGIN

	if(__selected_channel == adc) {
		mp_adc_disable(adc);
		__selected_channel = NULL;
	}

	if(adc->task)
		mp_task_destroy(adc->task);

	if(adc->port)
		mp_gpio_release(adc->port);

	mp_list_remove(&__list, &adc->item);

	if(__list.first == NULL)
		__auto_fini(adc->kernel);

	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

void mp_adc_enable(mp_adc_t *adc) {
	ADC1->CR1 |= ADC_CR1_EOCIE;
}

void mp_adc_disable(mp_adc_t *adc) {
	ADC1->CR1 &= ~ADC_CR1_EOCIE;
}

void mp_adc_start() {
	ADC1->CR2 |= ADC_CR2_ADON;
}

void mp_adc_stop() {
	ADC1->CR2 &= ~ADC_CR2_SWSTART;
}

mp_bool_t mp_adc_state() {
	return(ADC1->CR2 & ADC_CR2_ADON ? ON : OFF);
}

void mp_adc_restore(mp_bool_t status) {
	if(status == ON)
		ADC1->CR2 |= ADC_CR2_ADON;
	else
		ADC1->CR2 &= ~ADC_CR2_ADON;
}

static void __auto_init(mp_kernel_t *kernel) {
	if(__isInit == YES)
		return;

	__selected_channel = NULL;
	mp_list_init(&__list);

	__controler = mp_task_create(&kernel->tasks, "ADC controler", adcControler, NULL, 1000);
	if(!__controler)
		return;
	mp_task_signal(__controler, MP_TASK_SIG_SLEEP);

	RCC->APB2ENR |= RCC_APB2_ADC1;
	(void)RCC->APB2ENR;

	/* longest sample time on every channel: the temperature sensor wants 17.1 us */
	ADC1->SMPR1 = 0x00FFFFFF;
	ADC1->SMPR2 = 0x3FFFFFFF;
	ADC1->SQR1 = 0;
	ADC1->CR1 = 0;
	ADC1->CR2 = ADC_CR2_EXTSEL_SW | ADC_CR2_EXTTRIG | ADC_CR2_TSVREFE | ADC_CR2_ADON;

	/* tSTAB, then calibrate */
	mp_clock_delay(2);
	ADC1->CR2 |= ADC_CR2_RSTCAL;
	while(ADC1->CR2 & ADC_CR2_RSTCAL);
	ADC1->CR2 |= ADC_CR2_CAL;
	while(ADC1->CR2 & ADC_CR2_CAL);

	mp_interrupt_set(ADC1_2_IRQn, _mp_adc_interrupt, NULL, "ADC1");

	__isInit = YES;
}

static void __auto_fini(mp_kernel_t *kernel) {
	if(__isInit == NO)
		return;

	mp_task_destroy(__controler);
	__controler = NULL;

	mp_interrupt_unset(ADC1_2_IRQn);
	ADC1->CR1 = 0;
	ADC1->CR2 = 0;
	RCC->APB2ENR &= ~RCC_APB2_ADC1;

	__isInit = NO;
}

MP_TASK(adcControler) {
	mp_adc_t *seek;
	mp_list_item_t *item;

	if(task->signal == MP_TASK_SIG_STOP) {
		mp_task_signal(task, MP_TASK_SIG_DEAD);
		return;
	}

	if(__list.first == NULL || __selected_channel != NULL)
		return;

	for(item = __list.first; item != NULL; item = item->next) {
		seek = item->user;

		/* this one waits for a capture */
		if(seek->state == 1) {
			__selected_channel = seek;

			ADC1->SQR3 = seek->channelId;
			(void)ADC1->DR;
			mp_adc_enable(seek);

			/* ADON again starts the conversion once the ADC is on */
			ADC1->CR2 |= ADC_CR2_SWSTART;

			mp_task_signal(task, MP_TASK_SIG_SLEEP);
			return;
		}
	}

	mp_task_signal(task, MP_TASK_SIG_OK);
}

MP_TASK(adcChannel) {
	mp_adc_t *adc = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		mp_task_signal(task, MP_TASK_SIG_DEAD);
		return;
	}

	/* need to run a capture */
	if(adc->state == 0) {
		mp_task_signal(__controler, MP_TASK_SIG_PENDING);
		mp_task_signal(task, MP_TASK_SIG_SLEEP);
		adc->state = 1;
	}
	else if(adc->state == 2) {
		if(adc->callback)
			adc->callback(adc);

		adc->state = 0;
		mp_task_signal(task, MP_TASK_SIG_OK);

		MP_INTERRUPT_SAFE_BEGIN
		__selected_channel = NULL;
		MP_INTERRUPT_SAFE_END

		/* let the controler look for the next channel */
		mp_task_signal(__controler, MP_TASK_SIG_PENDING);
	}
}

static void _mp_adc_interrupt(void *user) {
	mp_adc_t *adc = __selected_channel;

	if(!(ADC1->SR & ADC_SR_EOC))
		return;

	/* reading DR clears EOC */
	if(!adc) {
		(void)ADC1->DR;
		return;
	}

	adc->result = ADC1->DR & 0xFFF;
	mp_adc_disable(adc);
	adc->state = 2;
	mp_task_signal(adc->task, MP_TASK_SIG_PENDING);
}

/**@}*/
