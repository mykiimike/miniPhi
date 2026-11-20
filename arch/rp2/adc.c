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
@defgroup mpArchRP2350ADC ADC

@ingroup mpArchRP2350

One 12-bit SAR. GPIO26..29 are channels 0..3, the die temperature is
channel 4, and the reference is the analog supply. clk_adc is PLL_USB
at 48 MHz. A channel task asks the controller for one capture; the FIFO
interrupt returns the result. mp_adc_convert() polls and needs no callback.

@{
*/

static void __auto_init(mp_kernel_t *kernel);
static void __auto_fini(mp_kernel_t *kernel);
static int _mp_adc_periodic(mp_kernel_t *kernel);
static void _mp_adc_periodic_off(void);
static int _mp_adc_clock(void);
static int _mp_adc_power(void);
static int _mp_adc_once(int channel, int *out);
static int _mp_adc_kick(int channel);
static void _mp_adc_interrupt(void *user);
MP_TASK(adcControler);
MP_TASK(adcChannel);

int mp_arch_pll_usb_init(void);

static mp_bool_t __isInit = NO;
static mp_list_t __list;
static mp_task_t *__controler = NULL;
static mp_adc_t * volatile __selected_channel = NULL;
static mp_kernel_t *__kernel = NULL;
static int __users = 0;
static int __irq = 0;
static int __settled = 0;
static mp_adc_t __hold;

static int _mp_adc_reset(void) {
	unsigned int spins = 100000;

	RESETS_RESET &= ~RESETS_ADC;
	while(spins-- && !(RESETS_RESET_DONE & RESETS_ADC))
		;
	if(RESETS_RESET_DONE & RESETS_ADC)
		return(1);
	RESETS_RESET |= RESETS_ADC;
	mp_printk("ADC: block stayed in reset");
	return(0);
}

static int _mp_adc_clock(void) {
	unsigned int spins = 100000;

	if(mp_clock_usb48() == NO && !mp_arch_pll_usb_init())
		return(0);
	if(CLOCKS_CLK_ADC_CTRL & CLOCKS_ADC_ENABLED)
		return(1);
	CLOCKS_CLK_ADC_DIV = CLOCKS_DIV_INT1;
	CLOCKS_CLK_ADC_CTRL = CLOCKS_ADC_ENABLE | (CLOCKS_ADC_AUX_USB << 5);
	while(spins-- && !(CLOCKS_CLK_ADC_CTRL & CLOCKS_ADC_ENABLED))
		;
	return((CLOCKS_CLK_ADC_CTRL & CLOCKS_ADC_ENABLED) != 0);
}

static void _mp_adc_hw_off(void) {
	ADC_INTE = 0;
	ADC_FCS = 0;
	ADC_CS = 0;
	CLOCKS_CLK_ADC_CTRL &= ~CLOCKS_ADC_ENABLE;
	RESETS_RESET |= RESETS_ADC;
	__settled = 0;
}

static uint32_t _mp_adc_keep(void) {
	return(ADC_CS & ~(ADC_CS_START_ONCE | ADC_CS_START_MANY | ADC_CS_ERR_STICKY | ADC_CS_ERR));
}

static int _mp_adc_ready(void) {
	unsigned int spins = 100000;

	while(spins-- && !(ADC_CS & ADC_CS_READY))
		;
	return((ADC_CS & ADC_CS_READY) != 0);
}

/* The temperature sensor wants time after TS_EN. */
static int _mp_adc_power(void) {
	if(!(CLOCKS_CLK_ADC_CTRL & CLOCKS_ADC_ENABLED) && !_mp_adc_clock())
		return(0);
	if(ADC_CS & ADC_CS_EN)
		return(_mp_adc_ready());

	ADC_CS = ADC_CS_EN | ADC_CS_TS_EN;
	if(!_mp_adc_ready())
		return(0);
	if(!__settled) {
		mp_clock_nanoDelay(100000);
		__settled = 1;
	}
	return(1);
}

/* A new sample is the FIFO leaving empty. Eight pops is the whole FIFO. */
static int _mp_adc_empty(void) {
	unsigned int n = 8;

	while(n-- && !(ADC_FCS & ADC_FCS_EMPTY))
		(void)ADC_FIFO;
	if(ADC_FCS & (ADC_FCS_OVER | ADC_FCS_UNDER))
		ADC_FCS |= ADC_FCS_OVER | ADC_FCS_UNDER;
	return((ADC_FCS & ADC_FCS_EMPTY) != 0);
}

static uint32_t _mp_adc_select(int channel) {
	return(ADC_CS_EN | ADC_CS_TS_EN | ((uint32_t)channel << ADC_CS_AINSEL_LSB));
}

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
	if(channel < 0 || channel > MP_ADC_CHANNEL_MAX)
		return(FALSE);
	adc->channelId = (char)channel;

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
	if(__isInit == NO || !_mp_adc_periodic(kernel)) {
		if(adc->port)
			mp_gpio_release(adc->port);
		if(__isInit == YES && __list.first == NULL && __users == 0)
			__auto_fini(kernel);
		return(FALSE);
	}

	adc->task = mp_task_create(&kernel->tasks, "ADC channel", adcChannel, adc, (unsigned long)timer);
	if(!adc->task) {
		mp_printk("ADC: no channel task for %s", who);
		if(adc->port)
			mp_gpio_release(adc->port);
		if(__list.first == NULL) {
			_mp_adc_periodic_off();
			if(__users == 0)
				__auto_fini(kernel);
		}
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

	if(__list.first == NULL) {
		_mp_adc_periodic_off();
		if(__users == 0)
			__auto_fini(adc->kernel);
	}

	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

void mp_adc_enable(mp_adc_t *adc) {
	(void)adc;
	ADC_INTE = ADC_INT_FIFO;
}

void mp_adc_disable(mp_adc_t *adc) {
	(void)adc;
	ADC_INTE = 0;
}

void mp_adc_start(void) {
	ADC_CS = _mp_adc_keep() | ADC_CS_EN | ADC_CS_TS_EN;
}

void mp_adc_stop(void) {
	ADC_CS = _mp_adc_keep();
}

mp_ret_t mp_adc_state(void) {
	return(ADC_CS & ADC_CS_EN ? ON : OFF);
}

void mp_adc_restore(mp_bool_t status) {
	uint32_t cs = _mp_adc_keep();

	if(status == ON)
		cs |= ADC_CS_EN | ADC_CS_TS_EN;
	else {
		cs &= ~(ADC_CS_EN | ADC_CS_TS_EN);
		__settled = 0;
	}
	ADC_CS = cs;
}

static void __auto_init(mp_kernel_t *kernel) {
	if(__isInit == YES)
		return;

	__selected_channel = NULL;
	memset(&__hold, 0, sizeof(__hold));
	mp_list_init(&__list);

	if(!_mp_adc_reset())
		return;
	if(!_mp_adc_clock()) {
		mp_printk("ADC: clk_adc stayed off");
		RESETS_RESET |= RESETS_ADC;
		return;
	}

	__kernel = kernel;
	__isInit = YES;
}

/* The FIFO interrupt is armed only for the periodic path. */
static int _mp_adc_periodic(mp_kernel_t *kernel) {
	if(!__controler) {
		__controler = mp_task_create(&kernel->tasks, "ADC controler", adcControler, NULL, 1000);
		if(!__controler) {
			mp_printk("ADC: no controler task");
			return(0);
		}
		mp_task_signal(__controler, MP_TASK_SIG_SLEEP);
	}
	if(__irq)
		return(1);
	if(!mp_interrupt_set(ADC_IRQ_FIFO, _mp_adc_interrupt, NULL, "ADC")) {
		mp_printk("ADC: FIFO IRQ is not available");
		mp_task_destroy(__controler);
		__controler = NULL;
		return(0);
	}
	__irq = 1;
	return(1);
}

static void _mp_adc_periodic_off(void) {
	if(__irq) {
		mp_interrupt_unset(ADC_IRQ_FIFO);
		__irq = 0;
	}
	if(__controler) {
		mp_task_destroy(__controler);
		__controler = NULL;
	}
}

static void __auto_fini(mp_kernel_t *kernel) {
	(void)kernel;
	if(__isInit == NO)
		return;

	__isInit = NO;
	__selected_channel = NULL;
	_mp_adc_periodic_off();
	_mp_adc_hw_off();
	__kernel = NULL;
}

static int _mp_adc_once(int channel, int *out) {
	uint32_t cs;
	unsigned int spins;

	ADC_INTE = 0;
	if(!_mp_adc_power())
		return(0);
	if(!_mp_adc_empty())
		return(0);
	cs = _mp_adc_select(channel);
	ADC_CS = cs | ADC_CS_ERR_STICKY;
	if(!_mp_adc_ready())
		return(0);

	ADC_FCS = ADC_FCS_EN | (1UL << ADC_FCS_THRESH_LSB);
	ADC_CS = cs | ADC_CS_START_ONCE;
	spins = 100000;
	while(spins-- && (ADC_FCS & ADC_FCS_EMPTY))
		;
	if(ADC_FCS & ADC_FCS_EMPTY) {
		ADC_FCS = (1UL << ADC_FCS_THRESH_LSB);
		return(0);
	}
	*out = (int)(ADC_FIFO & 0xFFF);
	ADC_FCS = (1UL << ADC_FCS_THRESH_LSB);
	return(1);
}

static int _mp_adc_kick(int channel) {
	uint32_t cs;

	ADC_INTE = 0;
	if(!_mp_adc_power())
		return(0);
	if(!_mp_adc_empty())
		return(0);
	cs = _mp_adc_select(channel);
	ADC_CS = cs | ADC_CS_ERR_STICKY;
	if(!_mp_adc_ready())
		return(0);

	ADC_FCS = ADC_FCS_EN | (1UL << ADC_FCS_THRESH_LSB);
	ADC_INTE = ADC_INT_FIFO;
	ADC_CS = cs | ADC_CS_START_ONCE;
	return(1);
}

void mp_adc_idle(void) {
	if(__isInit == YES && __selected_channel == NULL && (ADC_CS & ADC_CS_EN)) {
		ADC_CS = 0;
		__settled = 0;
	}
}

void mp_adc_prescale(unsigned long pclk2) {
	(void)pclk2;
}

mp_bool_t mp_adc_quiet(void) {
	return(__selected_channel == NULL ? YES : NO);
}

mp_ret_t mp_adc_open(mp_kernel_t *kernel) {
	__auto_init(kernel);
	if(__isInit == NO)
		return(FALSE);
	__users++;
	return(TRUE);
}

void mp_adc_close(void) {
	if(__users > 0)
		__users--;
	if(__users == 0 && __list.first == NULL && __kernel)
		__auto_fini(__kernel);
}

int mp_adc_channel(mp_gpio_port_t *pin) {
	if(!pin || pin->pin < 26 || pin->pin > 29)
		return(-1);
	return((int)pin->pin - 26);
}

int mp_adc_convert(int channel) {
	uint32_t saved = 0;
	int result = -1;
	int busy = 0;

	if(__isInit == NO || channel < 0 || channel > MP_ADC_CHANNEL_MAX)
		return(-1);

	MP_INTERRUPT_SAFE_BEGIN
	if(__selected_channel != NULL)
		busy = 1;
	else {
		saved = ADC_INTE;
		ADC_INTE = 0;
		__selected_channel = &__hold;
	}
	MP_INTERRUPT_SAFE_END
	if(busy)
		return(-1);

	if(!_mp_adc_once(channel, &result))
		result = -1;

	MP_INTERRUPT_SAFE_BEGIN
	__selected_channel = NULL;
	ADC_INTE = saved;
	MP_INTERRUPT_SAFE_END

	if(__controler)
		mp_task_signal(__controler, MP_TASK_SIG_PENDING);
	return(result);
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

		if(seek->state == 1) {
			__selected_channel = seek;
			if(!_mp_adc_kick((int)seek->channelId)) {
				__selected_channel = NULL;
				seek->state = 0;
				if(seek->task)
					mp_task_signal(seek->task, MP_TASK_SIG_OK);
				mp_task_signal(task, MP_TASK_SIG_SLEEP);
				return;
			}
			mp_task_signal(task, MP_TASK_SIG_SLEEP);
			return;
		}
	}

	mp_task_signal(task, MP_TASK_SIG_SLEEP);
}

MP_TASK(adcChannel) {
	mp_adc_t *adc = task->user;

	if(task->signal == MP_TASK_SIG_STOP) {
		mp_task_signal(task, MP_TASK_SIG_DEAD);
		return;
	}

	if(adc->state == 0) {
		adc->state = 1;
		if(__controler)
			mp_task_signal(__controler, MP_TASK_SIG_PENDING);
		mp_task_signal(task, MP_TASK_SIG_SLEEP);
	}
	else if(adc->state == 2) {
		if(adc->callback)
			adc->callback(adc);

		adc->state = 0;
		mp_task_signal(task, MP_TASK_SIG_OK);

		MP_INTERRUPT_SAFE_BEGIN
		__selected_channel = NULL;
		MP_INTERRUPT_SAFE_END

		if(__controler)
			mp_task_signal(__controler, MP_TASK_SIG_PENDING);
	}
}

static void _mp_adc_interrupt(void *user) {
	mp_adc_t *adc;
	int result = -1;

	(void)user;
	if(!(ADC_INTS & ADC_INT_FIFO))
		return;
	if(!(ADC_FCS & ADC_FCS_EMPTY))
		result = (int)(ADC_FIFO & 0xFFF);
	if(ADC_FCS & (ADC_FCS_OVER | ADC_FCS_UNDER))
		ADC_FCS |= ADC_FCS_OVER | ADC_FCS_UNDER;

	adc = __selected_channel;
	mp_adc_disable(adc);
	ADC_FCS = (1UL << ADC_FCS_THRESH_LSB);

	if(result < 0 || !adc || !adc->task)
		return;

	adc->result = result;
	adc->state = 2;
	mp_task_signal(adc->task, MP_TASK_SIG_PENDING);
}

/**@}*/
