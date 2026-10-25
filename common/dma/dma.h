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

#ifndef _HAVE_MP_COMMON_DMA_H
	#define _HAVE_MP_COMMON_DMA_H

	#ifdef SUPPORT_COMMON_DMA

	/**
	 * @defgroup mpCommonDma DMA transfers
	 * @ingroup mpCommon
	 *
	 * A transfer is the life of a buffer. The engine that moves it is opaque:
	 * a chip driver asks the bus it already holds (mp_spi_engine()), or
	 * mp_dma_memory() for a copy. map fills bus, start hands the bytes over,
	 * done gives them back. Between map and done the CPU leaves the buffer
	 * alone; sync is the exception, for the half a cyclic engine just finished.
	 *
	 * in[] is memory the device writes, or the destination of a copy.
	 * out[] is memory the device reads, or the source of a copy.
	 * fixed means that address is used len times and does not advance.
	 * An engine may refuse cyclic, several segments, or a length it cannot carry.
	 * The mp_dma_t starts at zero. done may run in an interrupt.
	 * @{
	 */

	typedef unsigned long mp_dma_addr_t;

	typedef enum {
		/** memory is read by the engine */
		MP_DMA_TO_DEVICE,
		/** memory is written by the engine */
		MP_DMA_FROM_DEVICE,
		/** out[] is copied into in[] */
		MP_DMA_BOTH,
	} mp_dma_dir_t;

	typedef enum {
		/** cyclic: the first half may be read */
		MP_DMA_HALF,
		/** one-shot finished, or a cyclic buffer wrapped */
		MP_DMA_DONE,
		MP_DMA_FAIL,
	} mp_dma_result_t;

	typedef struct mp_dma_s mp_dma_t;
	typedef struct mp_dma_engine_s mp_dma_engine_t;
	typedef struct mp_dma_ops_s mp_dma_ops_t;
	typedef struct mp_dma_seg_s mp_dma_seg_t;

	typedef void (*mp_dma_done_t)(mp_dma_t *dma, mp_dma_result_t result);

	struct mp_dma_seg_s {
		void *cpu;
		/** set by map; the driver does not dereference it */
		mp_dma_addr_t bus;
		unsigned long len;
		/** YES: the engine touches cpu, len times, without advancing */
		mp_bool_t fixed;
	};

	struct mp_dma_ops_s {
		mp_ret_t (*map)(mp_dma_t *dma);
		void (*sync)(mp_dma_t *dma, unsigned short seg);
		void (*unmap)(mp_dma_t *dma);
		mp_ret_t (*start)(mp_dma_t *dma);
		void (*stop)(mp_dma_t *dma);
	};

	struct mp_dma_engine_s {
		const mp_dma_ops_t *ops;
	};

	struct mp_dma_s {
		mp_dma_dir_t dir;
		mp_dma_seg_t *in;
		unsigned short nin;
		mp_dma_seg_t *out;
		unsigned short nout;
		mp_bool_t cyclic;
		mp_dma_done_t done;
		void *user;

		const mp_dma_ops_t *ops;
		mp_dma_engine_t *engine;
		/** 0 idle, 1 mapped, 2 running */
		unsigned char state;
	};

	/** FALSE when dma is not idle, or engine has no ops */
	mp_ret_t mp_dma_bind(mp_dma_t *dma, mp_dma_engine_t *engine);
	mp_ret_t mp_dma_map(mp_dma_t *dma);
	void mp_dma_sync(mp_dma_t *dma, unsigned short seg);
	mp_ret_t mp_dma_start(mp_dma_t *dma);
	void mp_dma_stop(mp_dma_t *dma);
	void mp_dma_unmap(mp_dma_t *dma);

	/**
	 * The engine calls this from its completion, including a NULL done.
	 * One-shot returns the transfer to mapped before the callback.
	 * Cyclic stays running except on MP_DMA_FAIL.
	 */
	void mp_dma_done(mp_dma_t *dma, mp_dma_result_t result);

	/** CPU copy, deferred to a task. NULL when kernel is NULL. */
	mp_dma_engine_t *mp_dma_memory(mp_kernel_t *kernel);

	/** The kernel calls this once tasks exist. The copy engine stays idle until mp_dma_memory(). */
	void mp_dma_init(mp_kernel_t *kernel);
	/** Drop a copy still queued. The kernel calls this before the tasks stop. */
	void mp_dma_fini(mp_kernel_t *kernel);

	/** @} */

	#endif

#endif
