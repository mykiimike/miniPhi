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

#include <mp.h>

static void __auto_init(mp_kernel_t *kernel);
static void __auto_fini(mp_kernel_t *kernel);
MP_TASK(adcControler);
MP_TASK(adcChannel);

static mp_bool_t __isInit = NO;
static mp_list_t __list;
static mp_task_t *__controler = NULL;
static mp_adc_t *__selected_channel = NULL;


/*
 * port = GPIO
 * channel = A0 - A15
 * ref =
 */
mp_ret_t mp_adc_create(mp_kernel_t *kernel, mp_adc_t *adc, mp_options_t *options, const char *who) {
	int timer = 100;
	char tmp[5];
	char *value;
	int len;
	int a;

	memset(adc, 0, sizeof(*adc));
	adc->kernel = kernel;
	adc->state = 0;

	/* check for auto init */
	__auto_init(kernel);

	/* get port */
	value = mp_options_get(options, "port");
	if(value) {
		adc->port = mp_gpio_text_handle(value, "ADC12");
		if(!adc->port)
			return(FALSE);

		mp_gpio_direction(adc->port, MP_GPIO_INPUT);

		/* select alternative function */
		_GPIO_REG8(adc->port, _GPIO_SEL) |= 1<<adc->port->pin;
	}

	/* change timer value */
	value = mp_options_get(options, "delay");
	if(value)
		timer = atoi(value);

	/* get channel */
	value = mp_options_get(options, "channel");
	if(!value){
		mp_adc_remove(adc);
		return(FALSE);
	}
	len = strlen(value);
	len = len > sizeof(tmp) ? sizeof(tmp) : len;
	for(a=1, value++; a<len; a++, value++)
		tmp[a-1] = *value;
	tmp[a-1] = '\0';
	adc->channelId = atoi(tmp);

	/* A0 ... A15 on msp430 */
	if(adc->channelId > 15) {
		mp_adc_remove(adc);
		return(FALSE);
	}

	/* create task */
	adc->task = mp_task_create(&kernel->tasks, "ADC channel", adcChannel, adc, timer);
	if(!adc->task) {
		mp_adc_remove(adc);
		return(FALSE);
	}

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_BEGIN

	/* add list */
	mp_list_add_last(&__list, &adc->item, adc);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_END

	mp_printk("Creating ADC 12bits using A%d owned by %s", adc->channelId, who);

	return(TRUE);
}

mp_ret_t mp_adc_remove(mp_adc_t *adc) {
	mp_printk("Removing ADC 12bits using A%d", adc->channelId);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_BEGIN

	/* remove task */
	mp_task_destroy(adc->task);

	if(adc->port) {
		/* remove alternative function */
		_GPIO_REG8(adc->port, _GPIO_SEL) &= ~(1<<adc->port->pin);

		mp_gpio_release(adc->port);
	}

	/* remove from list */
	mp_list_remove(&__list, &adc->item);

	/* check for auto fini*/
	__auto_fini(adc->kernel);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

void mp_adc_enable(mp_adc_t *adc) {
	/* enable interrupt */
	ADC12IE = 1;
}

void mp_adc_disable(mp_adc_t *adc) {
	/* disable interrupt */
	ADC12IE = 0;
}

void mp_adc_start() {
	 ADC12CTL0 |= ADC12ENC;
}

void mp_adc_stop() {
	ADC12CTL0 &= ~ADC12ENC;
}

mp_bool_t mp_adc_state() {
	return(ADC12CTL0 & ADC12ENC ? ON : OFF);
}

void mp_adc_restore(mp_bool_t status) {
	if(status == ON)
		ADC12CTL0 |= ADC12ENC;
	else
		ADC12CTL0 &= ~ADC12ENC;
}

static void __auto_init(mp_kernel_t *kernel) {
	if(__isInit == YES)
		return;

	__selected_channel = NULL;

	/* create controler task */
	__controler = mp_task_create(&kernel->tasks, "ADC controler", adcControler, NULL, 0);
	if(!__controler)
		return;

	/* task sleeping */
	__controler->signal = MP_TASK_SIG_SLEEP;

#if defined(__msp430x54xA)
	  REFCTL0 |= REFMSTR + REFVSEL_0 + REFON;   // Enable internal 1.5V reference
	  ADC12CTL0 = ADC12SHT0_8 + ADC12ON; // Set sample time
#elif defined(__msp430x54x)
	  ADC12CTL0 = ADC12SHT0_8 + ADC12REFON + ADC12ON; // Internal ref = 1.5V
#endif

	/* setup list */
	mp_list_init(&__list);

	/* enable sample timer */
	ADC12CTL1 = ADC12SHP;

	/* 35us delay ADC is based on DCO */
	__delay_cycles(37);

	ADC12CTL0 |= ADC12MSC;

	__isInit = YES;
}

static void __auto_fini(mp_kernel_t *kernel) {
	if(__isInit == NO)
		return;

	/* remove task */
	mp_task_destroy(__controler);

	ADC12CTL0 &= ~(ADC12ON); // Set sample time

#if defined(__msp430x54xA)
	REFCTL0 &= ~REFON;   // Disable internal 1.5V reference
#endif

	/* stop ADC */
	mp_adc_stop();

	__isInit = YES;
}

MP_TASK(adcControler) {
	mp_adc_t *next;
	mp_adc_t *seek;
	int inch;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* assert */
	if(__list.first == NULL || __selected_channel != NULL)
		return;

	/* check for break list */
	seek = __list.first->user;
	while(seek != NULL) {
		next = seek->item.next != NULL ? seek->item.next->user : NULL;

		/* this one is free */
		if(seek->state == 1) {
			inch = ADC12INCH_0+seek->channelId;
			/* set channel */
			ADC12MCTL0 = ADC12SREF_1 | inch;
			__selected_channel = seek;

			/* enable channel */
			mp_adc_enable(seek);

			/* start ADC */
			mp_adc_start();

			/* send start */
			ADC12CTL0 |= ADC12SC;

			/* and wait */
			task->signal = MP_TASK_SIG_SLEEP;
			return;
		}

		seek = next;
	}

	task->signal = MP_TASK_SIG_OK;
	return;


}

MP_TASK(adcChannel) {
	mp_adc_t *adc = task->user;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* need to run a capture */
	if(adc->state == 0) {
		/* wakeup controler */
		__controler->signal = MP_TASK_SIG_PENDING;

		/* sleep my state */
		task->signal = MP_TASK_SIG_SLEEP;

		/* wait controler to send request */
		adc->state = 1;
	}
	else if(adc->state == 2) {
		/* execute end user callback */
		if(adc->callback)
			adc->callback(adc);

		adc->state = 0;
		task->signal = MP_TASK_SIG_OK;
		__selected_channel = NULL;

		mp_adc_enable(adc);
		mp_adc_stop();
	}


}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC12_VECTOR
__interrupt void ADC12ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12ISR (void)
#else
#error Compiler not supported!
#endif
{
	mp_adc_t *adc;

	switch(__even_in_range(ADC12IV,34)) {
		case  0: break;                           // Vector  0:  No interrupt
		case  2: break;                           // Vector  2:  ADC overflow
		case  4: break;                           // Vector  4:  ADC timing overflow
		case  6:
			adc = __selected_channel;
			if(!adc)
				break;
			adc->result = ADC12MEM0;
			adc->task->signal = MP_TASK_SIG_PENDING;
			adc->state = 2;
			ADC12IFG = 0;
			break;
		default: break;
	}

	ADC12IFG = 0;
}

