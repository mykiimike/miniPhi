#ifndef _HAVE_MP_PINOUT_H
	#define _HAVE_MP_PINOUT_H

	#ifdef SUPPORT_COMMON_PINOUT
		typedef struct mp_pinout_s {
			mp_task_t *task;
			mp_gpio_port_t *gpio;
			mp_kernel_t *kernel;
			int turn;
			int live;
			int step;
			int repeat;
			int count;
		} mp_pinout_t;

		mp_task_t *mp_pinout_onoff(mp_kernel_t *kernel, mp_gpio_port_t *gpio, char turn, int live, int step, int repeat, char *who);
	#endif

#endif
