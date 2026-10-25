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

#include <mp.h>

#ifdef SUPPORT_COMMON_DMA

/**
@defgroup mpCommonDmaCore DMA transfer life

@ingroup mpCommonDma

@{
*/

#define _MP_DMA_IDLE    0
#define _MP_DMA_MAPPED  1
#define _MP_DMA_RUNNING 2

typedef struct {
	mp_dma_engine_t engine;
	mp_kernel_t *kernel;
	mp_task_t *task;
	mp_dma_t *xfer;
} _mp_dma_mem_t;

static _mp_dma_mem_t _mp_dma_mem;

static mp_ret_t _mp_dma_identity(mp_dma_t *dma) {
	unsigned short i;

	for(i = 0; i < dma->nin; i++)
		dma->in[i].bus = (mp_dma_addr_t)(unsigned long)dma->in[i].cpu;
	for(i = 0; i < dma->nout; i++)
		dma->out[i].bus = (mp_dma_addr_t)(unsigned long)dma->out[i].cpu;
	return(TRUE);
}

static int _mp_dma_segs(mp_dma_seg_t *seg, unsigned short n) {
	unsigned short i;

	if(!seg)
		return(0);
	for(i = 0; i < n; i++)
		if(!seg[i].cpu || seg[i].len == 0)
			return(0);
	return(1);
}

mp_ret_t mp_dma_bind(mp_dma_t *dma, mp_dma_engine_t *engine) {
	if(!dma || dma->state != _MP_DMA_IDLE)
		return(FALSE);
	if(!engine || !engine->ops)
		return(FALSE);
	dma->ops = engine->ops;
	dma->engine = engine;
	return(TRUE);
}

mp_ret_t mp_dma_map(mp_dma_t *dma) {
	if(!dma || dma->state != _MP_DMA_IDLE || !dma->ops || !dma->ops->map)
		return(FALSE);
	if(dma->dir == MP_DMA_FROM_DEVICE) {
		if(dma->nin < 1 || !_mp_dma_segs(dma->in, dma->nin))
			return(FALSE);
	}
	else if(dma->dir == MP_DMA_TO_DEVICE) {
		if(dma->nout < 1 || !_mp_dma_segs(dma->out, dma->nout))
			return(FALSE);
	}
	else if(dma->dir == MP_DMA_BOTH) {
		if(dma->nin < 1 || dma->nout < 1)
			return(FALSE);
		if(!_mp_dma_segs(dma->in, dma->nin) || !_mp_dma_segs(dma->out, dma->nout))
			return(FALSE);
	}
	else
		return(FALSE);
	if(dma->ops->map(dma) == FALSE)
		return(FALSE);
	dma->state = _MP_DMA_MAPPED;
	return(TRUE);
}

void mp_dma_sync(mp_dma_t *dma, unsigned short seg) {
	if(!dma || !dma->ops || !dma->ops->sync)
		return;
	if(dma->state != _MP_DMA_MAPPED && dma->state != _MP_DMA_RUNNING)
		return;
	dma->ops->sync(dma, seg);
}

mp_ret_t mp_dma_start(mp_dma_t *dma) {
	mp_bool_t ready = NO;

	if(!dma || !dma->ops || !dma->ops->start)
		return(FALSE);

	MP_INTERRUPT_SAFE_BEGIN
	if(dma->state == _MP_DMA_MAPPED) {
		dma->state = _MP_DMA_RUNNING;
		ready = YES;
	}
	MP_INTERRUPT_SAFE_END

	if(ready == NO)
		return(FALSE);
	if(dma->ops->start(dma) == FALSE) {
		MP_INTERRUPT_SAFE_BEGIN
		if(dma->state == _MP_DMA_RUNNING)
			dma->state = _MP_DMA_MAPPED;
		MP_INTERRUPT_SAFE_END
		return(FALSE);
	}
	return(TRUE);
}

void mp_dma_stop(mp_dma_t *dma) {
	if(!dma || dma->state != _MP_DMA_RUNNING)
		return;
	if(dma->ops && dma->ops->stop)
		dma->ops->stop(dma);
	MP_INTERRUPT_SAFE_BEGIN
	if(dma->state == _MP_DMA_RUNNING)
		dma->state = _MP_DMA_MAPPED;
	MP_INTERRUPT_SAFE_END
}

void mp_dma_unmap(mp_dma_t *dma) {
	if(!dma)
		return;
	if(dma->state == _MP_DMA_RUNNING)
		mp_dma_stop(dma);
	if(dma->state != _MP_DMA_MAPPED)
		return;
	if(dma->ops && dma->ops->unmap)
		dma->ops->unmap(dma);
	dma->state = _MP_DMA_IDLE;
}

void mp_dma_done(mp_dma_t *dma, mp_dma_result_t result) {
	mp_dma_done_t done = NULL;

	if(!dma)
		return;

	MP_INTERRUPT_SAFE_BEGIN
	if(dma->state == _MP_DMA_RUNNING) {
		if(!(dma->cyclic == YES && result != MP_DMA_FAIL))
			dma->state = _MP_DMA_MAPPED;
		done = dma->done;
	}
	MP_INTERRUPT_SAFE_END

	if(done)
		done(dma, result);
}

static void _mp_dma_copy(mp_dma_seg_t *dst, mp_dma_seg_t *src) {
	unsigned char *in = dst->cpu;
	unsigned char *out = src->cpu;
	unsigned long n;
	unsigned long i;

	if(src->fixed == YES) {
		n = dst->len;
		for(i = 0; i < n; i++)
			in[i] = out[0];
	}
	else if(dst->fixed == YES) {
		n = src->len;
		for(i = 0; i < n; i++)
			in[0] = out[i];
	}
	else
		memcpy(in, out, src->len);
}

static MP_TASK(_mp_dma_mem_task) {
	_mp_dma_mem_t *mem = task->user;
	mp_dma_t *dma;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	MP_INTERRUPT_SAFE_BEGIN
	dma = mem->xfer;
	mem->xfer = NULL;
	MP_INTERRUPT_SAFE_END

	if(dma) {
		_mp_dma_copy(&dma->in[0], &dma->out[0]);
		mp_dma_done(dma, MP_DMA_DONE);
	}
	mp_task_signal(task, MP_TASK_SIG_SLEEP);
}

static void _mp_dma_mem_sync(mp_dma_t *dma, unsigned short seg) {
	(void)dma;
	(void)seg;
}

static void _mp_dma_mem_unmap(mp_dma_t *dma) {
	(void)dma;
}

static mp_ret_t _mp_dma_mem_start(mp_dma_t *dma) {
	_mp_dma_mem_t *mem = (_mp_dma_mem_t *)dma->engine;
	mp_dma_seg_t *dst;
	mp_dma_seg_t *src;

	if(dma->cyclic == YES || dma->dir != MP_DMA_BOTH || dma->nin != 1 || dma->nout != 1)
		return(FALSE);
	if(!dma->in || !dma->out)
		return(FALSE);
	dst = &dma->in[0];
	src = &dma->out[0];
	if(src->fixed == NO && dst->fixed == NO && src->len != dst->len)
		return(FALSE);
	if(!mem->kernel)
		return(FALSE);

	MP_INTERRUPT_SAFE_BEGIN
	if(!mem->xfer)
		mem->xfer = dma;
	else
		dma = NULL;
	MP_INTERRUPT_SAFE_END
	if(!dma)
		return(FALSE);

	if(!mem->task) {
		mem->task = mp_task_create(&mem->kernel->tasks, "DMA", _mp_dma_mem_task, mem, 1);
		if(!mem->task) {
			mem->xfer = NULL;
			mp_printk("DMA: no task slot");
			return(FALSE);
		}
		mp_task_signal(mem->task, MP_TASK_SIG_SLEEP);
	}
	mp_task_wake(mem->task);
	return(TRUE);
}

static void _mp_dma_mem_stop(mp_dma_t *dma) {
	_mp_dma_mem_t *mem = (_mp_dma_mem_t *)dma->engine;

	MP_INTERRUPT_SAFE_BEGIN
	if(mem->xfer == dma)
		mem->xfer = NULL;
	MP_INTERRUPT_SAFE_END
}

static const mp_dma_ops_t _mp_dma_mem_ops = {
	_mp_dma_identity,
	_mp_dma_mem_sync,
	_mp_dma_mem_unmap,
	_mp_dma_mem_start,
	_mp_dma_mem_stop,
};

mp_dma_engine_t *mp_dma_memory(mp_kernel_t *kernel) {
	if(!kernel)
		return(NULL);
	_mp_dma_mem.engine.ops = &_mp_dma_mem_ops;
	_mp_dma_mem.kernel = kernel;
	return(&_mp_dma_mem.engine);
}

void mp_dma_fini(void) {
	_mp_dma_mem.xfer = NULL;
}

/** @} */

#endif
