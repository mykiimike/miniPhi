

#ifndef _MP_H
	#define MP_H

	#include <config.h>

	#define NULL (void *)0

	#define TRUE 0
	#define FALSE -1

	#define YES 1
	#define NO 0
	#define ON 1
	#define OFF 0

	typedef signed char mp_ret_t;
	typedef signed char mp_bool_t;

	#include "common/list.h"

	typedef struct mp_kernel_s mp_kernel_t;
	typedef void (*mp_kernel_onBoot_t)(void *user);

	#ifdef __MSP430__
		#include <msp430.h>
		#include <string.h>
		#include <stdio.h>
		#include <msp430/internal.h>
	#endif

	#define MP_KERNEL_KPANIC 0
	#define MP_KERNEL_BOOT   1
	#define MP_KERNEL_RES01  2
	#define MP_KERNEL_RES02  3
	#define MP_KERNEL_RES03  4
	#define MP_KERNEL_RES04  5
	#define MP_KERNEL_RES05  6
	#define MP_KERNEL_RES06  7
	#define MP_KERNEL_RES07  8
	#define MP_KERNEL_RES08  9
	#define MP_KERNEL_RES09  10
	#define MP_KERNEL_RES10  11

	#include "common/mem.h"
	#include "common/task.h"
	#include "common/state.h"
	#include "common/led.h"
	#include "common/serial.h"
	#include "common/button.h"
	#include "common/pinout.h"

	#define MP_KERNEL_VERSION "1.0.0"

	struct mp_kernel_s {
		/** Vendor name  */
		char *mcuVendor;

		/** Microchip name  */
		char *mcuName;

		/** Kernel states */
		mp_state_handler_t states;

		/** Kernel tasks */
		mp_task_handler_t tasks;

		/** Kernel on boot state */
		mp_kernel_onBoot_t onBoot;

		/** User pointer for onBoot */
		void *onBootUser;

#ifdef __MSP430__
		/* necessary ? */
#endif

	};



	void mp_kernel_init(mp_kernel_t *kernel, mp_kernel_onBoot_t onBoot, void *user);
	void mp_kernel_fini(mp_kernel_t *kernel);
	void mp_kernel_state(mp_kernel_t *kernel, char number);
	void mp_kernel_loop(mp_kernel_t *kernel);

#endif
