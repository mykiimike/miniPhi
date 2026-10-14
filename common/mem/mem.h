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

#ifndef _HAVE_MEM_H
	#define _HAVE_MEM_H

	#ifdef SUPPORT_COMMON_MEM

		#include <stdint.h>

		typedef struct mp_mem_chunk_s mp_mem_chunk_t;

		/*
		 * the canary right after the data tells a handed out chunk from a free one;
		 * it is stored mixed with the chunk address and the key mp_mem_rekey() draws
		 */
		#define MEM_CANARY_USED 0xdeadbeefUL
		#define MEM_CANARY_FREE 0xfeedfaceUL

		struct mp_mem_chunk_s {
			unsigned char data[MP_MEM_CHUNK];
			unsigned long canary;

			/* next free chunk masked with the key, only meaningful while free */
			uintptr_t next;
		};

		/* every byte of MP_MEM_SIZE that holds a whole chunk is used */
		#define MP_MEM_CHUNKS ((int)(MP_MEM_SIZE / sizeof(mp_mem_chunk_t)))

		/** chunks handed out, and how many mp_mem_erase() prepared */
		typedef struct {
			int used;
			int total;
		} mp_mem_state_t;

		mp_ret_t mp_mem_erase(mp_kernel_t *kernel);
		void *mp_mem_alloc(mp_kernel_t *kernel, int size);
		void mp_mem_free(mp_kernel_t *kernel, void *ptr);
		void mp_mem_state(mp_mem_state_t *state);
		mp_ret_t mp_mem_rekey(mp_kernel_t *kernel);
	#endif

#endif
