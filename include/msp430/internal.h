#ifndef _HAVE_MSP430_INTERNAL_H
	#define _HAVE_MSP430_INTERNAL_H

	#define FLOAT_DIV(a, b) (float)((float)a/(float)b)

	#include <msp430.h>
	#include "interrupt.h"
	#include "gpio.h"
	#include "clock.h"
	#include "i2c.h"
	#include "uart.h"

	mp_ret_t mp_machine_init(mp_kernel_t *kernel);
	mp_ret_t mp_machine_fini(mp_kernel_t *kernel);
	mp_ret_t mp_machine_loop(mp_kernel_t *kernel);

	//void *memset(void *s, int c, int n);

	struct mp_msp430_s {


	};
#endif
