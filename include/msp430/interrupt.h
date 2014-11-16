#ifndef _HAVE_MSP430_INTERRUPT_H
	#define _HAVE_MSP430_INTERRUPT_H

	typedef void (*mp_interrupt_cb_t)(void *user);

	typedef struct mp_interrupt_s mp_interrupt_t;

	struct mp_interrupt_s {
		mp_interrupt_cb_t callback;

		void *user;

		char *who;

	};
	mp_ret_t mp_interrupt_init();
	mp_ret_t mp_interrupt_fini();
	mp_interrupt_t *mp_interrupt_set(int vector, mp_interrupt_cb_t in, void *user, char *who);
	mp_ret_t mp_interrupt_unset(int vector);
#endif
