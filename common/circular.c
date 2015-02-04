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

static void _mp_circular_dummyInt(mp_circular_t *cir);

/**
@defgroup mpCommonCircular Circular buffer

@brief Circular queueing system

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

This library supports circular buffering used into generaly in communication
systems.
Common circular buffering respects the heap organisation and the interrupt model.

Example 1: Initiate context for RX
@code
// Circular context for RX
ret = mp_circular_init(
	kernel, &hdl->rxCir,
	PHY_rxIntEnable, PHY_rxIntDisable
);
if(!ret) {
	mp_printk("PHY: Can not create RX circular");
	return(FALSE);
}

// place my user pointer
serial->txCir.user = serial;
@endcode

@{
*/

/**
 * @brief Initiate circular buffer context
 *
 * This initiates a circular buffer context
 *
 * @param[in] kernel Kernel handler
 * @param[in] cir Circular context
 * @param[in] enable Enable interrupt
 * @param[in] disable Disable interrupt
 */
mp_ret_t mp_circular_init(mp_kernel_t *kernel, mp_circular_t *cir, mp_circular_int_t enable, mp_circular_int_t disable) {
	memset(cir, 0, sizeof(*cir));

	cir->kernel = kernel;
	cir->first = NULL;
	cir->last = NULL;

	cir->enable = enable ? enable : _mp_circular_dummyInt;
	cir->disable = disable ? disable : _mp_circular_dummyInt;

	cir->disable(cir);

	return(TRUE);
}

/**
 * @brief Terminate circular buffer context
 *
 * This terminate a circular buffer context by
 * freeing all circular buffer.
 *
 * @param[in] cir Circular context
 */
void mp_circular_fini(mp_circular_t *cir) {
	mp_circular_buffer_t *cur;
	mp_circular_buffer_t *next;

	/* free all circular buffer */
	cur = cir->first;
	while(cur) {
		next = cur->next;
		mp_mem_free(cir->kernel, cur);
		cur = next;
	}

}

mp_circular_buffer_t *mp_circular_read(mp_circular_t *cir) {
	mp_circular_buffer_t *newBuffer;
	mp_circular_buffer_t *buffer;

	/* no data */
	cir->disable(cir);
	if(cir->totalSize == 0) {
		cir->enable(cir);
		return(NULL);
	}
	cir->enable(cir);

	/* allocate new buffer now in order to reduce overrun */
	newBuffer = mp_mem_alloc(cir->kernel, sizeof(mp_circular_buffer_t));
	newBuffer->size = 0;
	newBuffer->next = NULL;

	/* pop circular and rotate buffers */
	cir->disable(cir);

	buffer = cir->first;
	if(buffer == cir->last) { /* last buffer */
		cir->first = newBuffer;
		cir->last = newBuffer;
	}
	else { /* not the last */
		cir->first = buffer->next;

		/* it's more efficient to do that */
		mp_mem_free(cir->kernel, newBuffer);
	}
	cir->totalSize -= buffer->size;
	cir->enable(cir);

	return(buffer);
}

mp_ret_t mp_circular_write(mp_circular_t *cir, unsigned char *data, int size) {
	mp_circular_buffer_t *buffer;
	mp_circular_buffer_t *first;
	mp_circular_buffer_t *prev;
	int block;
	int rest;
	int pos;

	rest = size;
	pos = 0;
	prev = NULL;
	block = size/MP_CIRCULAR_BUFFER_SIZE;
	block += size%MP_CIRCULAR_BUFFER_SIZE ? 1 : 0;

	while(block) {
		/* allocate the buffer */
		buffer = mp_mem_alloc(cir->kernel, sizeof(mp_circular_buffer_t));
		buffer->size = rest > MP_CIRCULAR_BUFFER_SIZE ? MP_CIRCULAR_BUFFER_SIZE : rest;
		buffer->pos = 0;
		buffer->next = NULL;

		/* copy and positionning buffer */
		memcpy(buffer->data, data+pos, buffer->size);
		rest -= buffer->size;
		pos += buffer->size;

		/* manage prev buffer */
		if(prev)
			prev->next = buffer;
		else
			first = buffer;
		prev = buffer;

		/* next block */
		block--;
	}

	/* then move buffers */
	cir->disable(cir);

	if(cir->last)
		cir->last->next = first;
	else
		cir->first = first;
	cir->last = buffer;
	cir->totalSize += size;

	cir->enable(cir);

	return(TRUE);
}

/**
 * @brief Circular buffer RX interrupt handler
 *
 * This control RX buffer interrupt.
 *
 * @param[in] cir Circular context
 */
void mp_circular_rxInterrupt(mp_circular_t *cir, unsigned char c) {
	mp_circular_buffer_t *buffer;

	/* get current buffer */
	buffer = cir->last;

	/* need allocation */
	if(buffer->size >= MP_CIRCULAR_BUFFER_SIZE-1) {
		/* allocate the buffer */
		buffer = mp_mem_alloc(cir->kernel, sizeof(mp_circular_buffer_t));
		buffer->size = 0;
		buffer->next = NULL;

		cir->last->next = buffer;
		cir->last = buffer;
	}

	buffer->data[buffer->size++] = c;
	cir->totalSize++;
}

/**
 * @brief Circular buffer TX interrupt handler
 *
 * This control TX buffer interrupt.
 * When there is no more buffer the *done pointer is set to YES and
 * the returned char is not sent. If *done is set to NO then the
 * function returns the unsigned char which must be sent by the
 * PHY driver.
 *
 * @param[in] cir Circular context
 * @param[out] done Interrupt status
 * @return unsigned char has to be sent
 */
unsigned char mp_circular_txInterrupt(mp_circular_t *cir, mp_bool_t *done) {
	mp_circular_buffer_t *buffer;
	unsigned char ret;

	*done = YES;

	/* get current buffer */
	buffer = cir->first;
	if(!buffer) {
		cir->disable(cir);
		return(0);
	}

	if(buffer->size == buffer->pos) {
		if(buffer->next) {
			/* swap free buffer */
			cir->first = buffer->next;
			mp_mem_free(cir->kernel, buffer);
			buffer = cir->first;

			/* tx new buffer */
			*done = NO;
			ret = buffer->data[buffer->pos++];
			cir->totalSize--;

			return(ret);
		}
		else {
			/* free last buffer */
			mp_mem_free(cir->kernel, buffer);

			cir->first = NULL;
			cir->last = NULL;
			cir->totalSize = 0;

			cir->disable(cir);
			return(0);
		}
	}

	/* retrieve char */
	*done = NO;
	ret = buffer->data[buffer->pos++];
	cir->totalSize--;

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


