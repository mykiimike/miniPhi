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

#ifdef SUPPORT_COMMON_MEM

#define _MP_HEAP_CKSIZE MP_MEM_SIZE/sizeof(mp_mem_chunk_t)

/** linear allocation tab */
static unsigned char __line[MP_MEM_SIZE];

/** linear allocation index */
static int __line_size = 0;

/** structured allocation tab */
static mp_mem_chunk_t *__allocated_heap = NULL;

/** linear allocation index */
static unsigned int __allocated_heap_idx = 0;

/** last free __allocated_heap */
static mp_mem_chunk_t *__allocated_heap_last = NULL;

/**
  * HEAP memory chunk allocation
  * @param kernel The kernel context
  * @param size Size of chunk, used as informational
  * @return Point to a free space
  */
void *mp_mem_alloc(mp_kernel_t *kernel, int size) {
	mp_mem_chunk_t *chunk;
	mp_mem_chunk_t **prev;

	/* sanatize */
	if(size >= MP_MEM_CHUNK-4) {
		//mp_kernel_kpanic(kernel, KPANIC_MEM_SIZE);
		return(NULL);
	}
	if(__line_size >= MP_MEM_SIZE) {
		//mp_kernel_kpanic(kernel, KPANIC_MEM_OOM);
		//_DP("chunk has no reach the limit at %p increase MP_MEM_SIZE", __allocated_heap);
		return(NULL);
	}
	chunk = (mp_mem_chunk_t *)__allocated_heap_last;
	prev = (mp_mem_chunk_t **)chunk+1;
	if(*prev)
		__allocated_heap_last = *prev;
	__allocated_heap_idx++;
	memset(chunk, 0, sizeof(*chunk));
	return((void *)chunk);
}

/**
  * free HEAP memory chunk
  * @param kernel The kernel context
  * @param ptr pointer to free
  * @return Nothing
  */
void mp_mem_free(mp_kernel_t *kernel, void *ptr) {
	mp_mem_chunk_t *chunk = ptr;
	mp_mem_chunk_t **prev;
	prev = (mp_mem_chunk_t **)chunk+1;
	if(__allocated_heap_last)
		*prev = __allocated_heap_last;
	__allocated_heap_last = chunk;
	__allocated_heap_idx--;
}

/**
  * erase HEAP memory
  * @param kernel The kernel context
  * @return TRUE or FALSE
  */
mp_ret_t mp_mem_erase(mp_kernel_t *kernel) {
	mp_mem_chunk_t *chunk;
	mp_mem_chunk_t **prev;

	/* control modulo */
	if(_MP_HEAP_CKSIZE % sizeof(unsigned int)) {
		//mp_kernel_kpanic(kernel, KPANIC_MEM_MODULO);
		return(FALSE);
	}

	/* erase memory */
	unsigned char *ptr = __line;
	int a;
	for(a=0; a<MP_MEM_SIZE; a++, ptr++)
		*ptr = 0;

	/* move heap allocation address */
	__allocated_heap = (mp_mem_chunk_t *)__line;
	__allocated_heap_last = NULL;
	__allocated_heap_idx = 0;

	/* prepare linked list */
	for(a=0; a<MP_MEM_SIZE-sizeof(mp_mem_chunk_t)-sizeof(mp_mem_chunk_t *);) {
		chunk = (mp_mem_chunk_t *)&__line[a];
		a += sizeof(mp_mem_chunk_t);

		prev = (mp_mem_chunk_t **)chunk+1;
		a += sizeof(*prev);

		if(__allocated_heap_last)
			*prev = __allocated_heap_last;
		__allocated_heap_last = chunk;
	}

	return(TRUE);
}

#endif
