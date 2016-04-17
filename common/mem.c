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

#ifndef MP_COMMON_MEM_USE_MALLOC
	/** linear allocation tab */
	static unsigned char __line[MP_MEM_SIZE];
#else
	static unsigned char *__line = NULL;
#endif

static mp_list_t __mem_free_root;

/**
@defgroup mpCommonMem Memory allocator

@ingroup mpCommon

@brief Tiny memory allocation system

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2014
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

miniPhi common memory is a library to manage memory allocations.
It is a tiny version of malloc() and then without "size hashing" algo.
This allocation system doesn't consume too much energy.

Each block has the same size and the size of a block is defined
in config.h by @ref MP_MEM_CHUNK (set to 1024 by default)

The predefine memory space is set my the define @ref MP_MEM_SIZE
(set to 50 by default) in config.h

It is also possible to specify the section used for the linear memory
if you wish (for example) to have an allocator in the Flash.
If @ref MP_COMMON_MEM_USE_MALLOC is defined then the whole memory will be
allocated using malloc() and if it is not defined the whole memory
will be allocated in the Flash.

You can overwrite those values by defining your own config.h file.

<strong>@ref SUPPORT_COMMON_MEM must be defined to use this module<strong>
@{
*/


/**
  * HEAP memory chunk allocation
  * @param kernel The kernel context
  * @param size Size of chunk, used as informational
  * @return Point to a free space
  */
void *mp_mem_alloc(mp_kernel_t *kernel, int size) {
	mp_list_item_t *item;
	mp_mem_chunk_t *chunk;

	/* sanatize */
	if(size >= MP_MEM_CHUNK) {
		mp_kernel_panic(kernel, KPANIC_MEM_SIZE);
		mp_printk("chunk is too low %d you are asking for %d", MP_MEM_CHUNK, size);
		return(NULL);
	}

	/* pop chunk */
	item = __mem_free_root.first;
	if(!item) {
		mp_kernel_panic(kernel, KPANIC_MEM_OOM);
		mp_printk("chunk has no reach the limit increase MP_MEM_SIZE");
		return(NULL);
	}

	/* get chunk */
	chunk = item->user;

	/* remove chunk from free and push it into used list */
	mp_list_remove(&__mem_free_root, item);
	memset(chunk->data, 0, sizeof(chunk->data));

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
	if(chunk->canary != MEM_CANARY) {
		mp_kernel_panic(kernel, KPANIC_MEM_OOM);
		mp_printk("Memory currupted");
		return;
	}
	memset(&chunk->item, 0, sizeof(chunk->item));
	mp_list_add_first(&__mem_free_root, &chunk->item, chunk);
}



/**
  * erase HEAP memory
  * @param kernel The kernel context
  * @return TRUE or FALSE
  */
mp_ret_t mp_mem_erase(mp_kernel_t *kernel) {
	mp_mem_chunk_t *chunk;
	int a;

	/* erase list */
	memset(&__mem_free_root, 0, sizeof(__mem_free_root));
	mp_list_init(&__mem_free_root);

#ifdef MP_COMMON_MEM_USE_MALLOC
	__line = malloc(MP_MEM_SIZE);
	if(!__line) {
		mp_printk("Can not allocate %d using malloc()", MP_MEM_SIZE);
		mp_kernel_panic(kernel, KPANIC_MEM_OOM);
		return(FALSE);
	}

	/* erase memory */
	memset(__line, 0, MP_MEM_SIZE);

	/* initialize free chunks */
	chunk = (mp_mem_chunk_t *)__line;
#else
	/* erase memory */
	memset(&__line, 0, MP_MEM_SIZE);

	/* initialize free chunks */
	chunk = (mp_mem_chunk_t *)&__line;
#endif

	/* prepare linked list */
	for(a=0; a<MP_MEM_SIZE/sizeof(mp_mem_chunk_t); chunk++, a++) {
		chunk->canary = MEM_CANARY;
		mp_list_add_last(&__mem_free_root, &chunk->item, chunk);

	}

	return(TRUE);
}

/**@}*/

#endif
