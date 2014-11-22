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
static char _mp_adc_get_mctl();
MP_TASK(adcControler);
MP_TASK(adcChannel);

static int __references = 0;
static mp_bool_t __isInit = NO;
static mp_list_t __list;
static mp_task_t *__controler = NULL;
static mp_adc_t *__controler_channelsId[16];

/*
 * port = GPIO
 * channel = A0 - A15
 * ref =
 */
mp_ret_t mp_adc_create(mp_kernel_t *kernel, mp_adc_t *adc, mp_options_t *options, const char *who) {
	int timer = 500;
	mp_bool_t state;
	int inch;
	char tmp[5];
	char *value;
	int len;
	int a;

	memset(adc, 0, sizeof(*adc));
	adc->kernel = kernel;

	/* check for auto init */
	__auto_init(kernel);
	state = mp_adc_state();

	/* get port */
	value = mp_options_get(options, "port");
	if(value) {
		adc->port = mp_gpio_text_handle(value, "ADC12");
		if(!adc->port)
			return(FALSE);
	}

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

	/* get modulation control ... */
	adc->ctlId = _mp_adc_get_mctl();

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_BEGIN

	/* stop ADC */
	mp_adc_stop();

	/* configure channel */
	inch = ADC12INCH_0+adc->channelId;
	value = ADC12MCTL+adc->ctlId;
	*value = ADC12SREF_1 + inch;

	/* add list */
	mp_list_add_last(&__list, &adc->item, adc);
	__controler_channelsId[adc->ctlId] = adc;

	/* start if necessary */
	mp_adc_restore(state);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

mp_ret_t mp_adc_remove(mp_adc_t *adc) {
	mp_bool_t state;

	/* remove task */
	mp_task_destroy(adc->task);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_BEGIN

	state = mp_adc_state();

	/* stop ADC */
	mp_adc_stop();

	/* disable ADC channel */
	mp_adc_disable(adc);

	if(adc->port)
		mp_gpio_release(adc->port);

	/* remove from list */
	mp_list_remove(&__list, &adc->item);
	__controler_channelsId[adc->ctlId] = NULL;

	/* restore state */
	mp_adc_restore(state);

	/* check for auto fini*/
	__auto_fini(adc->kernel);

	/* safe non interruptible block */
	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

void mp_adc_enable(mp_adc_t *adc) {
	char *ptr;
	ptr = ADC12MCTL+adc->ctlId;
	*ptr |= ADC12SC;

	/* enable interrupt */
	ADC12IE |= 1<<adc->ctlId;
}

void mp_adc_disable(mp_adc_t *adc) {
	char *ptr;
	ptr = ADC12MCTL+adc->ctlId;
	*ptr &= ~ADC12SC;

	/* disable interrupt */
	ADC12IE &= ~(1<<adc->ctlId);
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

static char _mp_adc_get_mctl() {
	char ctlId = 0;
	mp_adc_t *next;
	mp_adc_t *seek;

	/* assert */
	if(__list.first == NULL)
		return(0);

	/* check for break list */
	seek = __list.first->user;
	while(seek != NULL) {
		next = seek->item.next != NULL ? seek->item.next->user : NULL;

		/* this one is free */
		if(ctlId != seek->ctlId)
			return(ctlId);

		ctlId++;

		seek = next;
	}

	/* no one in break list then allocate new one */
	ctlId++;

	return(ctlId);
}

static void __auto_init(mp_kernel_t *kernel) {
	__references++;
	if(__isInit == YES)
		return;

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

	/* start ADC */
	mp_adc_start();

	__isInit = YES;
}

static void __auto_fini(mp_kernel_t *kernel) {
	__references--;
	if(__isInit == NO)
		return;

	if(__references != 0)
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
	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	ADC12CTL0 |= ADC12SC;
	task->signal = MP_TASK_SIG_SLEEP;
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

		/* enable ADC channel */
		mp_adc_enable(adc);

		adc->state = 1;
	}
	else {
		/* execute end user callback */
		if(adc->callback)
			adc->callback(adc);

		adc->state = 0;
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
	int dummy;

#define __SET(a) \
	adc = __controler_channelsId[a]; \
	if(!adc) { \
		dummy = ADC12MEM0+a; \
		break; \
	} \
	adc->result = ADC12MEM0+a; \
	adc->task->signal = MP_TASK_SIG_PENDING; \
	ADC12IFG &= ~(1<<a)

	switch(__even_in_range(ADC12IV,34)) {
		case  0: break;                           // Vector  0:  No interrupt
		case  2: break;                           // Vector  2:  ADC overflow
		case  4: break;                           // Vector  4:  ADC timing overflow
		case  6: __SET(0); break;                 // Vector  6:  ADC12IFG0
		case  8: __SET(1); break;                 // Vector  8:  ADC12IFG1
		case 10: __SET(2); break;                 // Vector 10:  ADC12IFG2
		case 12: __SET(3); break;                 // Vector 12:  ADC12IFG3
		case 14: __SET(4); break;                 // Vector 14:  ADC12IFG4
		case 16: __SET(5); break;                 // Vector 16:  ADC12IFG5
		case 18: __SET(6); break;                 // Vector 18:  ADC12IFG6
		case 20: __SET(7); break;                 // Vector 20:  ADC12IFG7
		case 22: __SET(8); break;                 // Vector 22:  ADC12IFG8
		case 24: __SET(9); break;                 // Vector 24:  ADC12IFG9
		case 26: __SET(10); break;                // Vector 26:  ADC12IFG10
		case 28: __SET(11); break;                // Vector 28:  ADC12IFG11
		case 30: __SET(12); break;                // Vector 30:  ADC12IFG12
		case 32: __SET(13); break;                // Vector 32:  ADC12IFG13
		case 34: __SET(14); break;                // Vector 34:  ADC12IFG14
		case 36: __SET(15); break;                // Vector 36:  ADC12IFG15
		default: break;
	}
}

