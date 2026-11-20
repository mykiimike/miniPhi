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

#ifndef _HAVE_RP2350_INTERRUPT_H
	#define _HAVE_RP2350_INTERRUPT_H

	typedef void (*mp_interrupt_cb_t)(void *user);

	typedef struct mp_interrupt_s mp_interrupt_t;

	struct mp_interrupt_s {
		mp_interrupt_cb_t callback;
		void *user;
		char *who;
	};

	mp_ret_t mp_interrupt_init();
	mp_ret_t mp_interrupt_fini();

	/* vector is the NVIC line (IRQn), the callback runs in handler mode */
	mp_interrupt_t *mp_interrupt_set(int vector, mp_interrupt_cb_t in, void *user, char *who);
	mp_ret_t mp_interrupt_unset(int vector);

	/* every peripheral IRQ of the vector table lands here */
	void mp_interrupt_dispatch(void);

	#if MP_CPUS > 1
		/* From another core: core 0 turns the NVIC line on or off */
		void mp_cpu_line(int vector, int on);
	#endif

	static inline void mp_interrupt_enable() {
		__asm volatile ("cpsie i" ::: "memory");
	}

	static inline void mp_interrupt_disable() {
		__asm volatile ("cpsid i" ::: "memory");
	}

	static inline mp_bool_t mp_interrupt_state() {
		uint32_t primask;
		__asm volatile ("mrs %0, primask" : "=r" (primask));
		return(primask & 1 ? FALSE : TRUE);
	}

	static inline void mp_interrupt_restore(mp_bool_t state) {
		if(state == TRUE)
			mp_interrupt_enable();
		else
			mp_interrupt_disable();
	}

	#define MP_ARCH_ISR_HW 1

	static inline int mp_interrupt_active(void) {
		return((SCB->ICSR & SCB_ICSR_VECTACTIVE) != 0);
	}

	static inline void mp_task_share_ack(void) {
	}

	#if MP_CPUS > 1
		/* Masks this core and holds the other one out: a section that was
		 * safe against its interrupts stays safe against the other core. */
		void mp_task_cpu_lock(void);
		void mp_task_cpu_unlock(void);

		#define MP_INTERRUPT_SAFE_BEGIN { mp_task_cpu_lock();

		#define MP_INTERRUPT_SAFE_END mp_task_cpu_unlock(); }
	#else
		#define MP_INTERRUPT_SAFE_BEGIN { mp_bool_t _____state = mp_interrupt_state(); \
			mp_interrupt_disable();

		#define MP_INTERRUPT_SAFE_END mp_interrupt_restore(_____state); }
	#endif

#endif
