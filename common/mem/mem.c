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
	/** linear allocation tab, word typed so chunks keep the alignment of their struct */
	static unsigned long __line[(MP_MEM_SIZE + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
#else
	static unsigned char *__line = NULL;
#endif

static mp_mem_chunk_t *__mem_free = NULL;
static int __mem_allocated = 0;
static int __mem_total = 0;

/* 0 until mp_mem_rekey() draws one from the RNG */
static unsigned long __mem_key = 0;

#define _MP_MEM_CANARY(chunk, state, key) ((state) ^ (key) ^ (unsigned long)(uintptr_t)(chunk))
#define _MP_MEM_NEXT(value, key) ((value) ^ (uintptr_t)(key))

/**
@defgroup mpCommonMem Memory allocator

@ingroup mpCommon

@brief Fixed-size chunk allocator

@version 1.1.0

@author @htmlonly &copy; @endhtmlonly 2014
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

The pool is cut into chunks of @ref MP_MEM_CHUNK bytes (50 by default)
out of @ref MP_MEM_SIZE bytes (1024 by default). Every allocation takes
one chunk whatever its size, so there is no fragmentation. Allocation and
release are interrupt safe.

The pool is a static array in RAM, or one malloc() block when
@ref MP_COMMON_MEM_USE_MALLOC is defined.

A canary after the data of each chunk says whether it is handed out or
free. mp_mem_free() refuses a pointer outside the pool or inside a
chunk, a chunk already free and a chunk whose canary was overwritten by
a write past its data. mp_mem_alloc() refuses a free chunk that was
written over, or whose next pointer leads out of the pool.

Canaries are mixed with the chunk address, so one copied from another
chunk does not pass, and with a key; the next pointer of a free chunk
is masked with the same key. The key is 0 at boot: with
@ref SUPPORT_COMMON_CRYPTO_RNG the RNG task calls mp_mem_rekey() once
seeded, which draws a secret key and rewrites every chunk with it, so a
write that forges a canary has to guess it.

<strong>@ref SUPPORT_COMMON_MEM must be defined to use this module</strong>
@{
*/

/* the chunk ptr points at, NULL when it is no chunk of the pool */
static mp_mem_chunk_t *_mp_mem_chunk(void *ptr) {
	unsigned char *base = (unsigned char *)__line;
	unsigned char *p = ptr;
	unsigned long offset;

	if(__mem_total == 0 || p < base || p >= base + MP_MEM_CHUNKS * sizeof(mp_mem_chunk_t))
		return(NULL);
	offset = (unsigned long)(p - base);
	if(offset % sizeof(mp_mem_chunk_t) != 0)
		return(NULL);
	return((mp_mem_chunk_t *)p);
}

/**
  * HEAP memory chunk allocation
  * @param kernel The kernel context
  * @param size Bytes needed, up to MP_MEM_CHUNK
  * @return A zeroed chunk, NULL when size is out of range, the pool is empty or corrupted
  */
void *mp_mem_alloc(mp_kernel_t *kernel, int size) {
	mp_mem_chunk_t *chunk;
	mp_bool_t corrupted = NO;

	if(size < 0 || size > MP_MEM_CHUNK) {
		mp_kernel_panic(kernel, KPANIC_MEM_SIZE);
		mp_printk("Memory: %d bytes asked, chunks hold %d", size, MP_MEM_CHUNK);
		return(NULL);
	}

	/* pop chunk, interrupt handlers allocate too */
	MP_INTERRUPT_SAFE_BEGIN
	chunk = __mem_free;
	if(chunk) {
		mp_mem_chunk_t *next = (mp_mem_chunk_t *)_MP_MEM_NEXT(chunk->next, __mem_key);

		if(chunk->canary != _MP_MEM_CANARY(chunk, MEM_CANARY_FREE, __mem_key) ||
				(next && !_mp_mem_chunk(next))) {
			/* its next pointer can not be trusted: give up the free list */
			__mem_free = NULL;
			chunk = NULL;
			corrupted = YES;
		}
		else {
			__mem_free = next;
			chunk->canary = _MP_MEM_CANARY(chunk, MEM_CANARY_USED, __mem_key);
			chunk->next = 0;
			__mem_allocated++;
		}
	}
	MP_INTERRUPT_SAFE_END

	if(corrupted == YES) {
		mp_kernel_panic(kernel, KPANIC_MEM_CORRUPT);
		mp_printk("Memory: a free chunk was written over");
		return(NULL);
	}

	if(!chunk) {
		mp_kernel_panic(kernel, KPANIC_MEM_OOM);
		mp_printk("Memory: pool empty, increase MP_MEM_SIZE");
		return(NULL);
	}

	memset(chunk->data, 0, sizeof(chunk->data));

	return((void *)chunk);
}

/**
  * free HEAP memory chunk
  * @param kernel The kernel context
  * @param ptr Pointer from mp_mem_alloc(), NULL is ignored
  */
void mp_mem_free(mp_kernel_t *kernel, void *ptr) {
	mp_mem_chunk_t *chunk;
	unsigned long state;

	if(!ptr)
		return;

	chunk = _mp_mem_chunk(ptr);
	if(!chunk) {
		mp_kernel_panic(kernel, KPANIC_MEM_CORRUPT);
		mp_printk("Memory: freeing a pointer out of the pool");
		return;
	}

	/* the canary unmixed: MEM_CANARY_USED, MEM_CANARY_FREE or garbage */
	MP_INTERRUPT_SAFE_BEGIN
	state = _MP_MEM_CANARY(chunk, chunk->canary, __mem_key);
	if(state == MEM_CANARY_USED) {
		chunk->canary = _MP_MEM_CANARY(chunk, MEM_CANARY_FREE, __mem_key);
		chunk->next = _MP_MEM_NEXT((uintptr_t)__mem_free, __mem_key);
		__mem_free = chunk;
		__mem_allocated--;
	}
	MP_INTERRUPT_SAFE_END

	if(state == MEM_CANARY_FREE) {
		mp_kernel_panic(kernel, KPANIC_MEM_CORRUPT);
		mp_printk("Memory: chunk freed twice");
	}
	else if(state != MEM_CANARY_USED) {
		mp_kernel_panic(kernel, KPANIC_MEM_CORRUPT);
		mp_printk("Memory: chunk written past its %d bytes", MP_MEM_CHUNK);
	}
}

void mp_mem_state(mp_mem_state_t *state) {
	if(!state)
		return;

	state->total = __mem_total;
	MP_INTERRUPT_SAFE_BEGIN
	state->used = __mem_allocated;
	MP_INTERRUPT_SAFE_END
}

/**
  * erase HEAP memory
  *
  * Every chunk goes back to the free list: the pointers handed out before
  * are no longer valid.
  *
  * @param kernel The kernel context
  * @return TRUE or FALSE
  */
mp_ret_t mp_mem_erase(mp_kernel_t *kernel) {
	mp_mem_chunk_t *chunk;
	int a;

	MP_INTERRUPT_SAFE_BEGIN
	__mem_free = NULL;
	__mem_allocated = 0;
	__mem_total = 0;
	MP_INTERRUPT_SAFE_END

#ifdef MP_COMMON_MEM_USE_MALLOC
	if(!__line)
		__line = malloc(MP_MEM_SIZE);
	if(!__line) {
		mp_printk("Can not allocate %d using malloc()", MP_MEM_SIZE);
		mp_kernel_panic(kernel, KPANIC_MEM_OOM);
		return(FALSE);
	}
#endif

	memset(__line, 0, MP_MEM_SIZE);

	/* chained back to front so the list starts with the first chunk */
	chunk = (mp_mem_chunk_t *)__line;
	MP_INTERRUPT_SAFE_BEGIN
	for(a=MP_MEM_CHUNKS-1; a>=0; a--) {
		chunk[a].canary = _MP_MEM_CANARY(&chunk[a], MEM_CANARY_FREE, __mem_key);
		chunk[a].next = _MP_MEM_NEXT((uintptr_t)__mem_free, __mem_key);
		__mem_free = &chunk[a];
	}
	__mem_total = MP_MEM_CHUNKS;
	MP_INTERRUPT_SAFE_END

	return(TRUE);
}

/**
  * draw a new canary key from the RNG
  *
  * Every chunk, handed out or free, is rewritten with the new key in one
  * critical section. A chunk whose canary was already bad keeps it, and
  * the next free or alloc of it panics as before.
  *
  * @param kernel The kernel context
  * @return TRUE once rekeyed; FALSE without RNG, before it is seeded or before mp_mem_erase()
  */
mp_ret_t mp_mem_rekey(mp_kernel_t *kernel) {
#ifdef SUPPORT_COMMON_CRYPTO_RNG
	mp_mem_chunk_t *chunk = (mp_mem_chunk_t *)__line;
	unsigned long key, old, state;
	int a, bad = 0;

	if(__mem_total == 0)
		return(FALSE);

	do {
		if(mp_rng_bytes(&key, sizeof(key)) != TRUE)
			return(FALSE);
	} while(key == __mem_key);

	MP_INTERRUPT_SAFE_BEGIN
	old = __mem_key;
	for(a=0; a<__mem_total; a++) {
		state = _MP_MEM_CANARY(&chunk[a], chunk[a].canary, old);
		if(state == MEM_CANARY_USED)
			chunk[a].canary = _MP_MEM_CANARY(&chunk[a], MEM_CANARY_USED, key);
		else if(state == MEM_CANARY_FREE) {
			chunk[a].canary = _MP_MEM_CANARY(&chunk[a], MEM_CANARY_FREE, key);
			chunk[a].next = _MP_MEM_NEXT(_MP_MEM_NEXT(chunk[a].next, old), key);
		}
		else
			bad++;
	}
	__mem_key = key;
	MP_INTERRUPT_SAFE_END

	key = 0;
	old = 0;

	if(bad) {
		mp_kernel_panic(kernel, KPANIC_MEM_CORRUPT);
		mp_printk("Memory: %d chunks found corrupted while rekeying", bad);
	}
	return(TRUE);
#else
	(void)kernel;
	return(FALSE);
#endif
}

/**@}*/

#endif
