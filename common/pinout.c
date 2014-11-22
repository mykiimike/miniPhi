#include <mp.h>

#ifdef SUPPORT_COMMON_PINOUT

MP_TASK(onoffLive);
MP_TASK(onoffStep);

MP_TASK(onoffLive) {
	mp_pinout_t *pinout = task->user;
	char turn;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		mp_gpio_unset(pinout->gpio);
		mp_mem_free(pinout->kernel, pinout);
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	turn = pinout->turn == ON ? OFF : ON;
	if(turn == ON)
		mp_gpio_set(pinout->gpio);
	else
		mp_gpio_unset(pinout->gpio);

	//_DP("Live pinout type=%d turn=%s count=%d", pinout->type, turn == ON ? "ON" : "OFF", pinout->count);

	if(pinout->repeat >= 0) {
		pinout->task->wakeup = onoffStep;
		pinout->task->delay = pinout->step;
		return;
	}

	task->signal = MP_TASK_SIG_STOP;
}


MP_TASK(onoffStep) {
	mp_pinout_t *pinout = task->user;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		mp_gpio_unset(pinout->gpio);
		mp_mem_free(pinout->kernel, pinout);
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	if(pinout->turn == ON)
		mp_gpio_set(pinout->gpio);
	else
		mp_gpio_unset(pinout->gpio);

	//_DP("Step pinout type=%d turn=%s", pinout->type, pinout->turn == ON ? "ON" : "OFF");

	if(pinout->live > 0) {
		/* change wakeup task */
		pinout->task->wakeup = onoffLive;
		pinout->task->delay = pinout->live;
		pinout->count++;
		return;
	}

	task->signal = MP_TASK_SIG_STOP;
}

/**
  * manipulate async ON/OFF pinout
  * @param kernel The kernel context
  * @param type type of pinouts enumerate in mp_pinout_led_t
  * @param turn if ON then pinout device will be set to ON, OFF turns off.
  * @param live number of time before to reverse position (0 no reverse position)
  * @param step delay before to repeat again
  * @param repeat  -1 = no repeat | 0 = infinite repeat more than 0 count
  * @return The task used to schedule the device
  */
mp_task_t *mp_pinout_onoff(mp_kernel_t *kernel, mp_gpio_port_t *gpio, char turn, int live, int step, int repeat, char *who) {
	mp_pinout_t *pinout;

	if(step > 0) {
		pinout = mp_mem_alloc(kernel, sizeof(*pinout));
		if(pinout == NULL)
			return(NULL);
		memset(pinout, 0, sizeof(*pinout));

		pinout->kernel = kernel;
		pinout->gpio = gpio;
		pinout->turn = turn;
		pinout->live = live;
		pinout->step = step;
		pinout->repeat = repeat;

		pinout->task = mp_task_create(&kernel->tasks, who, onoffStep, pinout, step);

		return(pinout->task);
	}
	else {
		if(turn == ON)
			mp_gpio_set(gpio);
		else
			mp_gpio_unset(gpio);
		//_DP("Direct pinout type=%d turn=%s", type, turn == ON ? "ON" : "OFF");
	}

	if(live > 0) {
		pinout = mp_mem_alloc(kernel, sizeof(*pinout));
		if(pinout == NULL)
			return(NULL);

		memset(pinout, 0, sizeof(*pinout));

		pinout->kernel = kernel;
		pinout->gpio = gpio;
		pinout->turn = turn;
		pinout->live = live;
		pinout->step = step;
		pinout->repeat = repeat;

		pinout->task = mp_task_create(&kernel->tasks, who, onoffLive, pinout, live);

		return(pinout->task);
	}


	return(NULL);
}

#endif

