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

static void _mp_kernel_state_boot_set(void *user);
static void _mp_kernel_state_boot_unset(void *user);
static void _mp_kernel_state_boot_tick(void *user);

static void _mp_kernel_state_kpanic_set(void *user);
static void _mp_kernel_state_kpanic_unset(void *user);
static void _mp_kernel_state_kpanic_tick(void *user);

/**
@defgroup mpCommonKernel miniPhi base of the kernel

@brief Provides logical base for an embedded kernel

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2014
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015


@{
*/


void mp_kernel_init(mp_kernel_t *kernel, mp_kernel_onBoot_t onBoot, void *user) {

	/* initialize printk */
	mp_printk_unset();

	kernel->version = MP_KERNEL_VERSION;

	/* setup structure */
	kernel->onBoot = onBoot;
	kernel->onBootUser = user;

	/* initialize the machine */
	mp_machine_init(kernel);

	/* erase mem if necessary */
#ifdef SUPPORT_COMMON_MEM
	mp_mem_erase(kernel);
#endif

	/* init sensors */
#ifdef SUPPORT_COMMON_SENSOR
	mp_sensor_init(kernel);
#endif

	/* initialize tasks */
	mp_task_init(&kernel->tasks);

	/* initialize logical machine state */
	mp_state_init(&kernel->states);

	/* define KPANIC machine state */
	mp_state_define(
		&kernel->states,
		MP_KERNEL_KPANIC, "KPANIC", kernel,
		_mp_kernel_state_kpanic_set,
		_mp_kernel_state_kpanic_unset,
		_mp_kernel_state_kpanic_tick
	);

	/* define BOOT machine state */
	mp_state_define(
		&kernel->states,
		MP_KERNEL_BOOT, "BOOT", kernel,
		_mp_kernel_state_boot_set,
		_mp_kernel_state_boot_unset,
		_mp_kernel_state_boot_tick
	);

	mp_kernel_state(kernel, MP_KERNEL_BOOT);
}


void mp_kernel_fini(mp_kernel_t *kernel) {
	/* terminate logical machine state */
	mp_state_fini(&kernel->states);

	/* initialize tasks */
	mp_task_init(&kernel->tasks);

	/* terminate the machine */
	mp_machine_fini(kernel);
}


void mp_kernel_state(mp_kernel_t *kernel, char number) {
	mp_state_switch(&kernel->states, number);
}

void mp_kernel_loop(mp_kernel_t *kernel) {
	mp_task_tick_t taskRet;
	while(1) {
		/* execute tasks */
		taskRet = mp_task_tick(&kernel->tasks);
		if(taskRet == MP_TASK_WORKING) {
			/* execute machine state */
			mp_state_tick(&kernel->states);
		}
	}
}

/**@}*/

void mp_kernel_panic(mp_kernel_t *kernel, int error) {


}

static void _mp_kernel_state_boot_set(void *user) { }

static void _mp_kernel_state_boot_unset(void *user) { }

static void _mp_kernel_state_boot_tick(void *user) {
	mp_kernel_t *kernel = user;
	if(kernel->onBoot)
		kernel->onBoot(kernel->onBootUser);
}


static void _mp_kernel_state_kpanic_set(void *user) {


}

static void _mp_kernel_state_kpanic_unset(void *user) {


}

static void _mp_kernel_state_kpanic_tick(void *user) {


}
