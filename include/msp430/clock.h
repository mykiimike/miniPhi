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

#ifndef _HAVE_MSP430_CLOCK_H
	#define _HAVE_MSP430_CLOCK_H

	#define MSP430_TICK_RATE_HZ ((unsigned int)1000)

	/* Auxilary clock */
	#define ACLK_FREQ_HZ ((unsigned int)32768)

	typedef struct mp_clock_freq_settings_s {
		unsigned char ID;
		unsigned char DCORSEL;
		unsigned char VCORE;
		unsigned int DCO;
	} mp_clock_freq_settings_t;

	typedef enum {
		MHZ1_t,
		MHZ4_t,
		MHZ8_t,
		MHZ12_t,
		MHZ16_t,
		MHZ20_t,
		MHZ25_t
	} mp_clock_freq_t;

	typedef struct mp_clock_sched_s mp_clock_sched_t;

	struct mp_clock_sched_s {
		/** low or high clock frequency */
		char clockState;

		/** is scheduler off ? */
		char schedulerOff;

		unsigned long longestDelay;
		unsigned long shortestDelay;
		unsigned long currentWait;

	};

	mp_ret_t mp_clock_init(mp_kernel_t *kernel);
	mp_ret_t mp_clock_fini(mp_kernel_t *kernel);
	void mp_clock_reset(mp_kernel_t *kernel);

	mp_ret_t mp_clock_low_energy(mp_kernel_t *kernel);
	mp_ret_t mp_clock_high_energy(mp_kernel_t *kernel);

	void mp_clock_schedule(mp_kernel_t *kernel);
	void mp_clock_task_change(mp_task_t *task);
	void mp_clock_wakeup(mp_kernel_t *kernel);

	unsigned long mp_clock_ticks();
	unsigned long mp_clock_get_speed();
	const char *mp_clock_name(mp_clock_freq_t clock);

	void mp_clock_delay(int delay);
	void mp_clock_nanoDelay(unsigned long delay);

#endif
