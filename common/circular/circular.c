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

#include <mp.h>

#ifdef SUPPORT_COMMON_CIRCULAR

static void _mp_circular_dummyInt(mp_circular_t *cir);

/**
@defgroup mpCommonCircular Circular buffer

@ingroup mpCommon

@brief Circular queueing system

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

This library supports circular buffering used into generaly in communication
systems.
Common circular buffering respects the heap organisation and the interrupt model.

A RX circular is filled by mp_circular_rxInterrupt() and drained by
mp_circular_read(), which hands the chunk over: the caller frees it with
mp_mem_free(). A TX circular is filled by mp_circular_write() and drained by
mp_circular_txInterrupt().

The enable and disable callbacks only mask the interrupt line of this
circular. The circular remembers whether it left the line on, so a locked
section restores it instead of forcing it on.

Example 1: Initiate context for RX
@code
// Circular context for RX
mp_circular_init(
	kernel, &serial->rxCir,
	PHY_rxIntEnable, PHY_rxIntDisable
);

// place my user pointer, the callbacks need it
serial->rxCir.user = serial;

// listen
mp_circular_start(&serial->rxCir);
@endcode

@{
*/

static void _mp_circular_lock(mp_circular_t *cir) {
	cir->disable(cir);
}

static void _mp_circular_unlock(mp_circular_t *cir) {
	if(cir->enabled == YES)
		cir->enable(cir);
}

/**
 * @brief Initiate circular buffer context
 *
 * This initiates a circular buffer context. The interrupt line is
 * left untouched until mp_circular_start() or mp_circular_write().
 *
 * @param[in] kernel Kernel handler
 * @param[in] cir Circular context
 * @param[in] enable Enable interrupt
 * @param[in] disable Disable interrupt
 */
mp_ret_t mp_circular_init(mp_kernel_t *kernel, mp_circular_t *cir, mp_circular_int_t enable, mp_circular_int_t disable) {
	memset(cir, 0, sizeof(*cir));

	cir->kernel = kernel;
	cir->maxSize = MP_CIRCULAR_MAX_SIZE;
	cir->first = NULL;
	cir->last = NULL;
	cir->enabled = NO;
	cir->writing = NO;

	cir->enable = enable ? enable : _mp_circular_dummyInt;
	cir->disable = disable ? disable : _mp_circular_dummyInt;

	return(TRUE);
}

/**
 * @brief Turn the interrupt line on
 *
 * A RX circular calls it once cir->user is set.
 *
 * @param[in] cir Circular context
 */
void mp_circular_start(mp_circular_t *cir) {
	cir->enabled = YES;
	cir->enable(cir);
}

/**
 * @brief Terminate circular buffer context
 *
 * This turns the interrupt line off and frees all circular buffer.
 * The context can be terminated twice.
 *
 * @param[in] cir Circular context
 */
void mp_circular_fini(mp_circular_t *cir) {
	mp_circular_buffer_t *cur;
	mp_circular_buffer_t *next;

	cir->enabled = NO;
	_mp_circular_lock(cir);
	cur = cir->first;
	cir->first = NULL;
	cir->last = NULL;
	cir->totalSize = 0;

	while(cur) {
		next = cur->next;
		mp_mem_free(cir->kernel, cur);
		cur = next;
	}
}

/**
 * @brief Pop the oldest buffer
 *
 * The returned buffer is detached and belongs to the caller,
 * which frees it with mp_mem_free().
 *
 * @param[in] cir Circular context
 * @return The buffer or NULL when there is no data
 */
mp_circular_buffer_t *mp_circular_read(mp_circular_t *cir) {
	mp_circular_buffer_t *buffer;

	_mp_circular_lock(cir);
	buffer = cir->first;
	if(buffer) {
		cir->first = buffer->next;
		if(!cir->first)
			cir->last = NULL;
		cir->totalSize -= buffer->size;
	}
	_mp_circular_unlock(cir);

	if(buffer)
		buffer->next = NULL;

	return(buffer);
}

/**
 * @brief Queue data and turn the interrupt line on
 *
 * Nothing is queued when the pool is short, when the circular would
 * exceed cir->maxSize, or when the call is nested into
 * another write (printk() from an allocation failure).
 *
 * @param[in] cir Circular context
 * @param[in] data Data to queue
 * @param[in] size Size of data
 * @return TRUE when the whole data is queued
 */
mp_ret_t mp_circular_write(mp_circular_t *cir, unsigned char *data, int size) {
	mp_circular_buffer_t *buffer;
	mp_circular_buffer_t *first;
	mp_circular_buffer_t *last;
	int rest;
	int pos;

	if(size == 0)
		return(TRUE);
	if(size < 0 || !data)
		return(FALSE);

	if(cir->writing == YES)
		return(FALSE);
	cir->writing = YES;

	if(size > cir->maxSize - cir->totalSize) {
		cir->writing = NO;
		return(FALSE);
	}

	/* build the chain away from the interrupt */
	first = NULL;
	last = NULL;
	rest = size;
	pos = 0;
	while(rest > 0) {
		buffer = mp_mem_alloc(cir->kernel, sizeof(mp_circular_buffer_t));
		if(!buffer) {
			while(first) {
				buffer = first->next;
				mp_mem_free(cir->kernel, first);
				first = buffer;
			}
			cir->writing = NO;
			return(FALSE);
		}
		buffer->size = rest > MP_CIRCULAR_BUFFER_SIZE ? MP_CIRCULAR_BUFFER_SIZE : rest;
		buffer->pos = 0;
		buffer->next = NULL;

		memcpy(buffer->data, data+pos, buffer->size);
		rest -= buffer->size;
		pos += buffer->size;

		if(last)
			last->next = buffer;
		else
			first = buffer;
		last = buffer;
	}

	/* then move buffers */
	_mp_circular_lock(cir);
	if(cir->last)
		cir->last->next = first;
	else
		cir->first = first;
	cir->last = last;
	cir->totalSize += size;
	cir->enabled = YES;
	_mp_circular_unlock(cir);

	cir->writing = NO;

	return(TRUE);
}

/**
 * @brief Circular buffer RX interrupt handler
 *
 * This control RX buffer interrupt. The byte is dropped when the
 * circular is full or the pool is empty.
 *
 * @param[in] cir Circular context
 * @param[in] c Received byte
 */
void mp_circular_rxInterrupt(mp_circular_t *cir, unsigned char c) {
	mp_circular_buffer_t *buffer;

	if(cir->totalSize >= cir->maxSize)
		return;

	buffer = cir->last;
	if(!buffer || buffer->size >= MP_CIRCULAR_BUFFER_SIZE) {
		buffer = mp_mem_alloc(cir->kernel, sizeof(mp_circular_buffer_t));
		if(!buffer)
			return;
		buffer->size = 0;
		buffer->pos = 0;
		buffer->next = NULL;

		if(cir->last)
			cir->last->next = buffer;
		else
			cir->first = buffer;
		cir->last = buffer;
	}

	buffer->data[buffer->size++] = c;
	cir->totalSize++;
}

/**
 * @brief Circular buffer TX interrupt handler
 *
 * This control TX buffer interrupt.
 * When there is no more buffer the *done pointer is set to YES, the
 * interrupt line is turned off and the returned char is not sent.
 * If *done is set to NO then the function returns the unsigned char
 * which must be sent by the PHY driver. A buffer goes back to the pool
 * with its last byte, so totalSize reaches 0 with the queue empty.
 *
 * @param[in] cir Circular context
 * @param[out] done Interrupt status
 * @return unsigned char has to be sent
 */
unsigned char mp_circular_txInterrupt(mp_circular_t *cir, mp_bool_t *done) {
	mp_circular_buffer_t *buffer;
	unsigned char ret;

	buffer = cir->first;
	if(!buffer) {
		*done = YES;
		cir->enabled = NO;
		cir->disable(cir);
		return(0);
	}

	*done = NO;
	ret = buffer->data[buffer->pos++];
	cir->totalSize--;

	if(buffer->pos >= buffer->size) {
		cir->first = buffer->next;
		if(!cir->first)
			cir->last = NULL;
		mp_mem_free(cir->kernel, buffer);
	}

	return(ret);
}

/**
 * @brief Returns the circular buffer size
 *
 * This returns the circular buffer size
 */
int mp_circular_bufferSize() {
	return(MP_CIRCULAR_BUFFER_SIZE);
}

/**@}*/

static void _mp_circular_dummyInt(mp_circular_t *cir) { }

#endif
