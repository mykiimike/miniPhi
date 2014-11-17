#include <mp.h>


#ifdef _SUPPORT_BUTTON

static void ___on_button(void *user);

mp_ret_t mp_button_init(mp_button_t *button, unsigned int port, unsigned int pin, char *who) {
	mp_ret_t ret;

	button->pressed = NO;

	/* open GPIO */
	button->gpio = mp_gpio_handle(port, pin, who);
	if(button->gpio == NULL)
		return(FALSE);

	/* set interrupt */
	ret = mp_gpio_interrupt_set(button->gpio, ___on_button, button, who);
	if(ret == FALSE) {
		mp_gpio_release(button->gpio);
		return(FALSE);
	}

	/* setup list on events */
	mp_list_init(&button->events);

	/* set hi to low */
	mp_gpio_interrupt_hi2lo(button->gpio);

	return(TRUE);
}

mp_ret_t mp_button_fini(mp_button_t *button) {

	/* unset interrupt */
	mp_gpio_interrupt_unset(button->gpio);

	/* remove gpio button */
	mp_gpio_release(button->gpio);

	return(TRUE);
}

mp_ret_t mp_button_event_create(
		mp_button_t *button, mp_button_event_t *bac,
		int delay, int time, mp_button_event_on_t cb, void *user
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
	mp_list_add_first(&button->events, &bac->item, bac);

	return(TRUE);
}

mp_ret_t mp_button_event_destroy(mp_button_event_t *bac) {
	return(TRUE);
}

static void ___on_button(void *user) {
	volatile unsigned long now;
	volatile mp_button_event_t *next;
	volatile mp_button_event_t *seek;
	volatile mp_button_t *button = user;

	/* get now */
	now = mp_clock_ticks();

	/* switch interrupt direction */
	mp_gpio_interrupt_hilo_switch(button->gpio);

	/* button state */
	button->pressed = button->pressed == NO ? YES : NO;
	if(button->pressed == YES)
		button->pressDelay = now;
	else
		button->pressDelay = now-button->pressDelay;

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

}

#endif
