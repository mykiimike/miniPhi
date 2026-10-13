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

#ifndef _HAVE_KFLASH_H
	#define _HAVE_KFLASH_H

	#ifdef SUPPORT_COMMON_KFLASH

		#ifndef MP_KFLASH_VALUE_MAX
			#define MP_KFLASH_VALUE_MAX 32
		#endif

		typedef struct mp_kflash_var_s mp_kflash_var_t;

		/** var->value already holds the new value */
		typedef void (*mp_kflash_on_t)(mp_kflash_var_t *var);

		/** a key a module listens to; the module allocates it and owns value */
		struct mp_kflash_var_s {
			mp_list_item_t item;
			const char *key;
			unsigned long hash;
			void *value;
			int size;
			mp_kflash_on_t onUpdate;
			void *user;
		};

		/** two banks of one flash page; records are appended to the active one */
		typedef struct {
			mp_flash_t bank[2];
			/** -1 when the store could not be opened */
			int active;
			unsigned long sequence;
			/** first free byte of the active bank */
			unsigned int used;
			/** the active bank holds something unreadable: the next write compacts */
			int full;
			mp_list_t vars;
		} mp_kflash_t;

		typedef struct {
			unsigned long base;
			unsigned int size;
			unsigned int used;
			int bank;
			int keys;
		} mp_kflash_state_t;

		/** called by mp_kernel_init() */
		mp_ret_t mp_kflash_init(mp_kernel_t *kernel);

		/**
		 * value gets what the flash holds for key, or fallback when it holds
		 * nothing of that size. onUpdate runs on later changes, not now.
		 */
		mp_ret_t mp_kflash_listen(mp_kernel_t *kernel, mp_kflash_var_t *var, const char *key, void *value, const void *fallback, int size, mp_kflash_on_t onUpdate, void *user);
		void mp_kflash_unlisten(mp_kernel_t *kernel, mp_kflash_var_t *var);

		/**
		 * Stores value and tells every listener of key. Nothing is written when
		 * the value does not change. The CPU stalls while the flash is programmed:
		 * call it from a task, never from an interrupt.
		 */
		mp_ret_t mp_kflash_set(mp_kernel_t *kernel, const char *key, const void *value, int size);

		/** TRUE when the flash holds a value of exactly size bytes for key */
		mp_ret_t mp_kflash_get(mp_kernel_t *kernel, const char *key, void *value, int size);

		mp_ret_t mp_kflash_listen_bool(mp_kernel_t *kernel, mp_kflash_var_t *var, const char *key, mp_bool_t *value, mp_bool_t fallback, mp_kflash_on_t onUpdate, void *user);
		mp_ret_t mp_kflash_set_bool(mp_kernel_t *kernel, const char *key, mp_bool_t value);

		void mp_kflash_state(mp_kernel_t *kernel, mp_kflash_state_t *state);

	#endif

#endif
