/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
 * Copyright (C) 2014  VERMAN                                              *
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

#ifndef _HAVE_MP_COMMON_CIRCULAR_H
	#define _HAVE_MP_COMMON_CIRCULAR_H

	#define MP_CIRCULAR_BUFFER_SIZE \
		MP_MEM_CHUNK-MP_MEM_SPACING-sizeof(int)-sizeof(mp_circular_buffer_t *)

	typedef struct mp_circular_s mp_circular_t;
	typedef struct mp_circular_buffer_s mp_circular_buffer_t;

	typedef void (*mp_circular_int_t)(mp_circular_t *cir);

	struct mp_circular_buffer_s {
		unsigned char data[MP_CIRCULAR_BUFFER_SIZE];
		unsigned int size;
		mp_circular_buffer_t *next;
	};

	struct mp_circular_s {
		mp_kernel_t *kernel;

		int totalSize;

		mp_circular_buffer_t *first;
		mp_circular_buffer_t *last;

		mp_circular_int_t enable;
		mp_circular_int_t disable;

		void *user;
	};

#endif
