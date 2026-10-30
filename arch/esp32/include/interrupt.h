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

#ifndef _HAVE_ESP32_INTERRUPT_H
	#define _HAVE_ESP32_INTERRUPT_H

	typedef void (*mp_interrupt_cb_t)(void *user);

	typedef struct mp_interrupt_s mp_interrupt_t;

	struct mp_interrupt_s {
		mp_interrupt_cb_t callback;
		void *user;
		char *who;
	};

	mp_ret_t mp_interrupt_init(void);
	mp_ret_t mp_interrupt_fini(void);

	/* vector is the interrupt-matrix source, the callback runs in machine mode */
	mp_interrupt_t *mp_interrupt_set(int vector, mp_interrupt_cb_t in, void *user, char *who);
	mp_ret_t mp_interrupt_unset(int vector);

	#ifdef ESP32C6
		static inline void mp_interrupt_enable(void) {
			__asm__ volatile ("csrs mstatus, %0" :: "r"(8) : "memory");
		}

		static inline void mp_interrupt_disable(void) {
			__asm__ volatile ("csrc mstatus, %0" :: "r"(8) : "memory");
		}

		static inline mp_bool_t mp_interrupt_state(void) {
			unsigned long mstatus;

			__asm__ volatile ("csrr %0, mstatus" : "=r"(mstatus));
			return(mstatus & 8 ? TRUE : FALSE);
		}
	#elif defined(ESP32S3)
		/* INTLEVEL 0 leaves the level-1 lines unmasked. WOE stays as _start set it. */
		static inline void mp_interrupt_enable(void) {
			__asm__ volatile ("rsil a2, 0" ::: "a2", "memory");
		}

		static inline void mp_interrupt_disable(void) {
			__asm__ volatile ("rsil a2, 1" ::: "a2", "memory");
		}

		static inline mp_bool_t mp_interrupt_state(void) {
			unsigned long ps;

			__asm__ volatile ("rsr.ps %0" : "=r"(ps));
			return((ps & 0xFUL) == 0 ? TRUE : FALSE);
		}
	#else
		#error ESP32 series interrupt state is missing
	#endif

	static inline void mp_interrupt_restore(mp_bool_t state) {
		if(state == TRUE)
			mp_interrupt_enable();
		else
			mp_interrupt_disable();
	}

	#define MP_INTERRUPT_SAFE_BEGIN { mp_bool_t _____state = mp_interrupt_state(); \
		mp_interrupt_disable();

	#define MP_INTERRUPT_SAFE_END mp_interrupt_restore(_____state); }

#endif
