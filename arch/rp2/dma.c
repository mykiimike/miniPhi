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

#include <arch/dma.h>

/**
@defgroup mpArchRP2350DMA DMA controller

@ingroup mpArchRP2350

Sixteen channels behind one claim. A request name owns a free channel
until release. Two claims of the same name cannot both be held. The
transfer carries one segment; cyclic is refused. map is the identity.
Completion is DMA_IRQ_0. The weak handler in the vector table already
calls mp_interrupt_dispatch().

@{
*/

#define _MP_DMA_CHANS DMA_CHANNELS

typedef struct {
	mp_dma_engine_t engine;
	const char *request;
	volatile void *periph;
	void (*arm)(void *user, mp_bool_t on);
	void (*prep)(void *user);
	void *user;
	char *who;
	mp_dma_t *xfer;
	DMA_Channel_t *hw;
	mp_dma_dir_t dir;
	unsigned char width;
	unsigned char index;
	unsigned char dreq;
	mp_bool_t claimed;
} _mp_dma_slot_t;

typedef struct {
	const char *name;
	unsigned char dreq;
} _mp_dma_req_t;

/* pico-sdk src/rp2350/hardware_regs/include/hardware/regs/dreq.h.
 * These are not the RP2040 numbers (SPI0 TX is 16 there). */
static const _mp_dma_req_t _mp_dma_table[] = {
	{ "spi0.tx", 24 },
	{ "spi0.rx", 25 },
	{ "spi1.tx", 26 },
	{ "spi1.rx", 27 },
};

static _mp_dma_slot_t _mp_dma_slots[_MP_DMA_CHANS];
static int _mp_dma_ready;
static unsigned char _mp_dma_irq_users;

static int _mp_dma_reset(void) {
	unsigned int spins = 100000;

	if(_mp_dma_ready)
		return(1);
	RESETS_RESET &= ~RESETS_DMA;
	while(spins-- && !(RESETS_RESET_DONE & RESETS_DMA))
		;
	if(!(RESETS_RESET_DONE & RESETS_DMA))
		return(0);
	_mp_dma_ready = 1;
	return(1);
}

/* Clearing EN while BUSY only pauses the channel. Abort flushes it. */
static int _mp_dma_idle(DMA_Channel_t *hw, unsigned char index) {
	uint32_t bit = 1UL << index;
	unsigned long spin;

	if((hw->CTRL_TRIG & (DMA_CTRL_EN | DMA_CTRL_BUSY)) == 0)
		return(1);
	DMA_CHAN_ABORT = bit;
	spin = 100000UL;
	while(spin-- && (DMA_CHAN_ABORT & bit))
		;
	if(DMA_CHAN_ABORT & bit)
		return(0);
	spin = 100000UL;
	while(spin-- && (hw->CTRL_TRIG & DMA_CTRL_BUSY))
		;
	if(hw->CTRL_TRIG & DMA_CTRL_BUSY)
		return(0);
	return(1);
}

static void _mp_dma_force_off(_mp_dma_slot_t *slot) {
	uint32_t bit = 1UL << slot->index;

	(void)_mp_dma_idle(slot->hw, slot->index);
	/* AL1_CTRL writes the control word without triggering the channel.
	 * A 1 in a write-1-to-clear error bit drops that error. */
	slot->hw->AL1_CTRL = DMA_CTRL_READ_ERROR | DMA_CTRL_WRITE_ERROR;
	DMA_INTS0 = bit;
	/* the peripheral request stays off while nothing is shifting, so the clock may change */
	if(slot->arm)
		slot->arm(slot->user, NO);
}

static mp_ret_t _mp_dma_program(_mp_dma_slot_t *slot, mp_dma_t *dma, mp_dma_seg_t *seg) {
	DMA_Channel_t *hw = slot->hw;
	uint32_t ctrl;
	uint32_t read;
	uint32_t write;
	mp_ret_t ok = FALSE;

	/* high priority, so another channel does not stall a shift register.
	 * CHAIN_TO is this channel: the reset value is 0, which would start
	 * channel 0 when this one finishes. IRQ_QUIET stays clear, so the
	 * block raises DMA_IRQ_0. Mode 0 of TRANS_COUNT counts down once. */
	ctrl = DMA_CTRL_EN | DMA_CTRL_HIGH_PRIORITY;
	ctrl |= (uint32_t)slot->index << DMA_CTRL_CHAIN_LSB;
	ctrl |= (uint32_t)slot->dreq << DMA_CTRL_TREQ_LSB;
	if(slot->width == 2)
		ctrl |= 1UL << DMA_CTRL_SIZE_LSB;
	else if(slot->width == 4)
		ctrl |= 2UL << DMA_CTRL_SIZE_LSB;

	if(slot->dir == MP_DMA_TO_DEVICE) {
		read = (uint32_t)seg->bus;
		write = (uint32_t)(unsigned long)slot->periph;
		if(seg->fixed == NO)
			ctrl |= DMA_CTRL_INCR_READ;
	}
	else {
		read = (uint32_t)(unsigned long)slot->periph;
		write = (uint32_t)seg->bus;
		if(seg->fixed == NO)
			ctrl |= DMA_CTRL_INCR_WRITE;
	}

	MP_INTERRUPT_SAFE_BEGIN
	if(_mp_dma_idle(hw, slot->index)) {
		hw->AL1_CTRL = DMA_CTRL_READ_ERROR | DMA_CTRL_WRITE_ERROR;
		DMA_INTS0 = 1UL << slot->index;
		hw->READ_ADDR = read;
		hw->WRITE_ADDR = write;
		hw->TRANS_COUNT = (uint32_t)seg->len;
		if(slot->prep)
			slot->prep(slot->user);
		if(slot->arm)
			slot->arm(slot->user, YES);
		slot->xfer = dma;
		hw->CTRL_TRIG = ctrl;
		ok = TRUE;
	}
	MP_INTERRUPT_SAFE_END
	return(ok);
}

static void _mp_dma_irq(void *user) {
	uint32_t pending;
	unsigned char index;

	(void)user;
	pending = DMA_INTS0;
	for(index = 0; index < _MP_DMA_CHANS; index++) {
		_mp_dma_slot_t *slot = &_mp_dma_slots[index];
		mp_dma_t *dma = NULL;
		mp_bool_t fail = NO;
		uint32_t bit = 1UL << index;

		if((pending & bit) == 0)
			continue;

		MP_INTERRUPT_SAFE_BEGIN
		if(slot->claimed == YES && slot->xfer && slot->hw) {
			fail = (slot->hw->CTRL_TRIG & (DMA_CTRL_READ_ERROR | DMA_CTRL_WRITE_ERROR)) ? YES : NO;
			dma = slot->xfer;
			slot->xfer = NULL;
			_mp_dma_force_off(slot);
		}
		else
			DMA_INTS0 = bit;
		MP_INTERRUPT_SAFE_END

		if(dma)
			mp_dma_done(dma, fail == YES ? MP_DMA_FAIL : MP_DMA_DONE);
	}
}

static _mp_dma_slot_t *_mp_dma_holder(const char *name) {
	int i;

	for(i = 0; i < _MP_DMA_CHANS; i++) {
		if(_mp_dma_slots[i].claimed == NO)
			continue;
		if(mp_options_cmp((char *)_mp_dma_slots[i].request, (char *)name) == TRUE)
			return(&_mp_dma_slots[i]);
	}
	return(NULL);
}

static _mp_dma_slot_t *_mp_dma_free(void) {
	int i;

	for(i = 0; i < _MP_DMA_CHANS; i++)
		if(_mp_dma_slots[i].claimed == NO)
			return(&_mp_dma_slots[i]);
	return(NULL);
}

/* ".tx" is memory to device. Anything else the table names is device to memory. */
static mp_dma_dir_t _mp_dma_req_dir(const char *name) {
	const char *dot = name;

	while(*dot && *dot != '.')
		dot++;
	if(dot[0] == '.' && dot[1] == 't')
		return(MP_DMA_TO_DEVICE);
	return(MP_DMA_FROM_DEVICE);
}

static mp_dma_seg_t *_mp_dma_seg(mp_dma_t *dma, _mp_dma_slot_t *slot) {
	if(slot->dir == MP_DMA_FROM_DEVICE) {
		if(dma->dir != MP_DMA_FROM_DEVICE || dma->nin != 1 || dma->nout != 0 || !dma->in)
			return(NULL);
		return(&dma->in[0]);
	}
	if(slot->dir == MP_DMA_TO_DEVICE) {
		if(dma->dir != MP_DMA_TO_DEVICE || dma->nout != 1 || dma->nin != 0 || !dma->out)
			return(NULL);
		return(&dma->out[0]);
	}
	return(NULL);
}

static mp_ret_t _mp_dma_map(mp_dma_t *dma) {
	unsigned short i;

	for(i = 0; i < dma->nin; i++)
		dma->in[i].bus = (mp_dma_addr_t)(unsigned long)dma->in[i].cpu;
	for(i = 0; i < dma->nout; i++)
		dma->out[i].bus = (mp_dma_addr_t)(unsigned long)dma->out[i].cpu;
	return(TRUE);
}

static void _mp_dma_sync(mp_dma_t *dma, unsigned short seg) {
	(void)dma;
	(void)seg;
}

static void _mp_dma_unmap(mp_dma_t *dma) {
	(void)dma;
}

static mp_ret_t _mp_dma_start(mp_dma_t *dma) {
	_mp_dma_slot_t *slot = (_mp_dma_slot_t *)dma->engine;
	mp_dma_seg_t *seg;

	if(dma->cyclic == YES)
		return(FALSE);
	seg = _mp_dma_seg(dma, slot);
	if(!seg || seg->len > DMA_TRANS_MAX || seg->bus == 0)
		return(FALSE);
	if(((unsigned long)seg->cpu & (slot->width - 1UL)) != 0)
		return(FALSE);
	return(_mp_dma_program(slot, dma, seg));
}

static void _mp_dma_stop(mp_dma_t *dma) {
	_mp_dma_slot_t *slot = (_mp_dma_slot_t *)dma->engine;

	MP_INTERRUPT_SAFE_BEGIN
	if(slot->xfer == dma) {
		slot->xfer = NULL;
		_mp_dma_force_off(slot);
	}
	MP_INTERRUPT_SAFE_END
}

static const mp_dma_ops_t _mp_dma_ops = {
	_mp_dma_map,
	_mp_dma_sync,
	_mp_dma_unmap,
	_mp_dma_start,
	_mp_dma_stop,
};

mp_dma_engine_t *mp_dma_claim(
	const char *request,
	mp_dma_dir_t dir,
	volatile void *periph,
	unsigned char width,
	void (*arm)(void *user, mp_bool_t on),
	void (*prep)(void *user),
	void *user,
	char *who
) {
	int i;
	int n;
	int matched = 0;
	const _mp_dma_req_t *req = NULL;
	_mp_dma_slot_t *slot = NULL;
	_mp_dma_slot_t *held = NULL;
	uint32_t bit;

	if(!request || !periph || (width != 1 && width != 2 && width != 4))
		return(NULL);
	if(dir != MP_DMA_FROM_DEVICE && dir != MP_DMA_TO_DEVICE)
		return(NULL);

	n = (int)(sizeof(_mp_dma_table) / sizeof(_mp_dma_table[0]));
	for(i = 0; i < n; i++) {
		if(mp_options_cmp((char *)_mp_dma_table[i].name, (char *)request) == FALSE)
			continue;
		if(_mp_dma_req_dir(_mp_dma_table[i].name) != dir)
			continue;
		matched = 1;
		held = _mp_dma_holder(_mp_dma_table[i].name);
		if(held)
			continue;
		req = &_mp_dma_table[i];
		slot = _mp_dma_free();
		break;
	}
	if(!slot) {
		if(!matched)
			mp_printk("DMA has no %s", request);
		else if(held)
			mp_printk("DMA %s is used by %s, %s can not have it", request, held->who ? held->who : "?", who ? who : "?");
		else
			mp_printk("DMA claim pool is full, %s can not have %s", who ? who : "?", request);
		return(NULL);
	}

	slot->request = req->name;
	slot->index = (unsigned char)(slot - _mp_dma_slots);
	slot->dreq = req->dreq;
	slot->hw = DMA_CH(slot->index);
	slot->periph = periph;
	slot->width = width;
	slot->dir = dir;
	slot->arm = arm;
	slot->prep = prep;
	slot->user = user;
	slot->who = who;
	slot->xfer = NULL;
	slot->claimed = YES;
	slot->engine.ops = &_mp_dma_ops;

	if(!_mp_dma_reset()) {
		slot->claimed = NO;
		slot->arm = NULL;
		mp_printk("DMA block stayed in reset");
		return(NULL);
	}

	_mp_dma_force_off(slot);
	bit = 1UL << slot->index;
	DMA_INTS0 = bit;
	DMA_INTE0 |= bit;

	if(_mp_dma_irq_users == 0 && !mp_interrupt_set(DMA_IRQ_0, _mp_dma_irq, NULL, "DMA")) {
		mp_printk("DMA %s: can not arm the interrupt", request);
		mp_dma_release(&slot->engine);
		return(NULL);
	}
	_mp_dma_irq_users++;
	return(&slot->engine);
}

void mp_dma_release(mp_dma_engine_t *engine) {
	_mp_dma_slot_t *slot;

	if(!engine)
		return;
	slot = (_mp_dma_slot_t *)engine;
	if(slot->claimed == NO)
		return;
	MP_INTERRUPT_SAFE_BEGIN
	slot->xfer = NULL;
	_mp_dma_force_off(slot);
	DMA_INTE0 &= ~(1UL << slot->index);
	MP_INTERRUPT_SAFE_END
	if(slot->arm)
		slot->arm(slot->user, NO);
	if(_mp_dma_irq_users)
		_mp_dma_irq_users--;
	if(_mp_dma_irq_users == 0)
		mp_interrupt_unset(DMA_IRQ_0);
	slot->claimed = NO;
	slot->who = NULL;
	slot->arm = NULL;
	slot->prep = NULL;
}

/**
 * @brief No claimed channel is shifting
 *
 * EN is CTRL bit 0. BUSY is bit 26, and it stays set if EN is cleared
 * without an abort.
 *
 * @return YES when the clock may change
 */
mp_bool_t mp_dma_quiet(void) {
	int i;

	for(i = 0; i < _MP_DMA_CHANS; i++) {
		_mp_dma_slot_t *slot = &_mp_dma_slots[i];
		uint32_t ctrl;

		if(slot->claimed == NO || !slot->hw)
			continue;
		if(slot->xfer)
			return(NO);
		ctrl = slot->hw->CTRL_TRIG;
		if(ctrl & (DMA_CTRL_EN | DMA_CTRL_BUSY))
			return(NO);
	}
	return(YES);
}

/**@}*/

#endif
