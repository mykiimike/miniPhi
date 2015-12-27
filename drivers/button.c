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


#ifdef SUPPORT_DRV_BUTTON

static void ___on_button(void *user);
MP_TASK(_mp_drv_button_asr);

mp_ret_t mp_drv_button_init(mp_kernel_t *kernel, mp_drv_button_t *button, mp_options_t *options, char *who) {
	mp_ret_t ret;
	char *value, *port;

	memset(button, 0, sizeof(*button));

	button->pressed = NO;

	/* get port */
	port = value = mp_options_get(options, "port");
	if(!value)
		return(FALSE);

    /* allocate GPIO */
	button->gpio = mp_gpio_text_handle(value, who);
    if(!button->gpio)
    	return(FALSE);

	/* set interrupt */
	ret = mp_gpio_interrupt_set(button->gpio, ___on_button, button, who);
	if(ret == FALSE) {
		mp_gpio_release(button->gpio);
		return(FALSE);
	}

	/* create ASR task */
	button->task = mp_task_create(&kernel->tasks, "BUTTON DRV", _mp_drv_button_asr, button, 1000);
	if(!button->task) {
		mp_printk("BUTTON(%p) FATA could not create task", button);
		mp_gpio_release(button->gpio);
		return(FALSE);
	}
	mp_task_signal(button->task, MP_TASK_SIG_SLEEP);

	/* setup list on events */
	mp_list_init(&button->events);

	/* set hi to low */
	mp_gpio_interrupt_hi2lo(button->gpio);
	mp_gpio_interrupt_enable(button->gpio);

	mp_printk("Creating button driver on GPIO %s used by %s", port, who);

	return(TRUE);
}

mp_ret_t mp_drv_button_fini(mp_drv_button_t *button) {

	mp_printk("Removing button driver on GPIO used by %s", button->gpio->who);

	if(button->task)
		mp_task_destroy(button->task);

	/* unset interrupt */
	mp_gpio_interrupt_disable(button->gpio);
	mp_gpio_interrupt_unset(button->gpio);

	/* remove gpio button */
	mp_gpio_release(button->gpio);

	return(TRUE);
}

mp_ret_t mp_drv_button_event_create(
		mp_drv_button_t *button, mp_drv_button_event_t *bac,
		int delay, int time, mp_drv_button_event_on_t cb, void *user
	) {
	memset(bac, 0, sizeof(*bac));

	bac->button = button;
	bac->cb = cb;
	bac->user = user;
	bac->delay = delay;
	bac->time = time;
	bac->downDelay = 0;
	bac->howManyDown = 0;

	/* add the event */
	mp_list_add_last(&button->events, &bac->item, bac);

	mp_printk("Creating button event on %s delay=%d time=%d cb=%p", button->gpio->who, delay, time, cb);

	return(TRUE);
}

mp_ret_t mp_drv_button_event_destroy(mp_drv_button_t *button, mp_drv_button_event_t *bac) {
	mp_printk("Destroying button event on %s cb=%p", bac->button->gpio->who, bac->cb);
	mp_list_remove(&button->events, &bac->item);
	return(TRUE);
}

MP_TASK(_mp_drv_button_asr) {
	unsigned long now;
	mp_drv_button_event_t *next;
	mp_drv_button_event_t *seek;
	mp_drv_button_t *button = task->user;

	/* receive shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {
		/* acknowledging */
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* get now */
	now = mp_clock_ticks();

	/* follow events */
	if(button->events.first != NULL) {

		/* just send the message to all tasks */
		seek = button->events.first->user;
		while(seek != NULL) {
			next = seek->item.next != NULL ? seek->item.next->user : NULL;

			/* only wait for pressed button state */
			if(button->pressed == NO) {

				/* multi down within delay */
				if(seek->time > 0) {
					if(seek->downDelay == 0)
						seek->downDelay = now;

					/* we can increment */
					if(now-seek->downDelay <= seek->delay) {
						seek->howManyDown++;

						if(seek->howManyDown == seek->time) {
							if(seek->cb)
								seek->cb(seek->user);
							/* could have access violation here if seek is freed during
							 * callback but it is acceptable because seek should handled
							 * in permanent memory */
							seek->howManyDown = 0;
							seek->downDelay = 0;
							break;
						}
					}
					else {
						seek->howManyDown = 0;
						seek->downDelay = 0;
					}

				}
				/* check press delay */
				else {
					if(button->pressDelay >= seek->delay) {
						if(seek->cb)
							seek->cb(seek->user);
						break;
					}
				}
			}
			seek = next;
		}
	}

	/* execute callback */
	if(button->onSwitch)
		button->onSwitch(button->user);

	mp_task_signal(button->task, MP_TASK_SIG_SLEEP);

}

static void ___on_button(void *user) {
	volatile unsigned long now;
	volatile mp_drv_button_t *button = user;

	/* get now */
	now = mp_clock_ticks();

	/* switch interrupt direction */
	mp_gpio_interrupt_hilo_switch(button->gpio);

	/* button state */
	button->pressed = button->pressed == NO ? YES : NO;
	if(button->pressed == YES)
		button->pressDelay = now;
	else {
		mp_task_signal(button->task, MP_TASK_SIG_PENDING);
		button->pressDelay = now-button->pressDelay;
	}


}

#endif
