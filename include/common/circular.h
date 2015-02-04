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
		(MP_MEM_CHUNK-MP_MEM_SPACING-(sizeof(unsigned short)*2)-sizeof(mp_circular_buffer_t *)-4)

	typedef struct mp_circular_s mp_circular_t;
	typedef struct mp_circular_buffer_s mp_circular_buffer_t;

	typedef void (*mp_circular_int_t)(mp_circular_t *cir);

	struct mp_circular_buffer_s {
		unsigned char data[MP_CIRCULAR_BUFFER_SIZE];
		unsigned short size;
		unsigned short pos;
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

	mp_ret_t mp_circular_init(mp_kernel_t *kernel, mp_circular_t *cir, mp_circular_int_t enable, mp_circular_int_t disable);
	void mp_circular_fini(mp_circular_t *cir);
	mp_circular_buffer_t *mp_circular_read(mp_circular_t *cir);
	mp_ret_t mp_circular_write(mp_circular_t *cir, unsigned char *data, int size);
	void mp_circular_rxInterrupt(mp_circular_t *cir, unsigned char c);
	unsigned char mp_circular_txInterrupt(mp_circular_t *cir, mp_bool_t *done);
	int mp_circular_bufferSize();

#endif
