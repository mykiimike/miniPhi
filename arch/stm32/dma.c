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
@defgroup mpArchSTM32DMA DMA controller

@ingroup mpArchSTM32

F1 channels and F4 streams behind one claim. A request name owns its
line until release. Two names that share a line cannot both be claimed.
Four claims live in RAM; the request table stays in flash. The transfer
carries one segment; cyclic is refused. map is the identity.

@{
*/

#define _MP_DMA_MAXLEN 65535UL
#define _MP_DMA_CLAIMS 4

typedef struct {
	mp_dma_engine_t engine;
	const char *request;
	volatile void *periph;
	void (*arm)(void *user, mp_bool_t on);
	void (*prep)(void *user);
	void *user;
	char *who;
	mp_dma_t *xfer;
	void *hw;
	void *ctl;
	int irq;
	mp_dma_dir_t dir;
	unsigned char width;
	unsigned char unit;
	unsigned char index;
	unsigned char chsel;
	mp_bool_t claimed;
} _mp_dma_slot_t;

typedef struct {
	const char *name;
	unsigned char unit;
	unsigned char index;
	unsigned char chsel;
	int irq;
} _mp_dma_req_t;

static void _mp_dma_irq(void *user);
static void _mp_dma_force_off(_mp_dma_slot_t *slot);
static uint32_t _mp_dma_take(_mp_dma_slot_t *slot, mp_bool_t *fail);
static mp_ret_t _mp_dma_program(_mp_dma_slot_t *slot, mp_dma_t *dma, mp_dma_seg_t *seg);
static void *_mp_dma_ctl(unsigned char unit);
static void *_mp_dma_hw(unsigned char unit, unsigned char index);

#if defined(STM32F407xx)

	#define _MP_DMA1 0x40026000UL
	#define _MP_DMA2 0x40026400UL

	typedef struct {
		__IO uint32_t LISR, HISR, LIFCR, HIFCR;
	} _mp_dma_ctl_t;

	typedef struct {
		__IO uint32_t CR, NDTR, PAR, M0AR, M1AR, FCR;
	} _mp_dma_hw_t;

	/* RM0090: several streams can serve one request. The first free one is taken. */
	static const _mp_dma_req_t _mp_dma_table[] = {
		{ "spi1.rx", 1, 0, 3, DMA2_Stream0_IRQn },
		{ "spi1.rx", 1, 2, 3, DMA2_Stream2_IRQn },
		{ "spi1.tx", 1, 3, 3, DMA2_Stream3_IRQn },
		{ "spi1.tx", 1, 5, 3, DMA2_Stream5_IRQn },
		{ "spi2.rx", 0, 3, 0, DMA1_Stream3_IRQn },
		{ "spi2.tx", 0, 4, 0, DMA1_Stream4_IRQn },
		{ "spi3.rx", 0, 0, 0, DMA1_Stream0_IRQn },
		{ "spi3.rx", 0, 2, 0, DMA1_Stream2_IRQn },
		{ "spi3.tx", 0, 5, 0, DMA1_Stream5_IRQn },
		{ "spi3.tx", 0, 7, 0, DMA1_Stream7_IRQn },
		{ "usart1.rx", 1, 2, 4, DMA2_Stream2_IRQn },
		{ "usart1.rx", 1, 5, 4, DMA2_Stream5_IRQn },
		{ "usart1.tx", 1, 7, 4, DMA2_Stream7_IRQn },
		{ "usart2.rx", 0, 5, 4, DMA1_Stream5_IRQn },
		{ "usart2.tx", 0, 6, 4, DMA1_Stream6_IRQn },
	};

	static void *_mp_dma_ctl(unsigned char unit) {
		return((void *)(unit ? _MP_DMA2 : _MP_DMA1));
	}

	static void *_mp_dma_hw(unsigned char unit, unsigned char index) {
		return((void *)((unsigned long)_mp_dma_ctl(unit) + 0x10UL + (unsigned long)index * 0x18UL));
	}

	static uint32_t _mp_dma_bits(unsigned char index, uint32_t low) {
		static const unsigned char base[4] = { 0, 6, 16, 22 };

		return(low << base[index & 3]);
	}

	static uint32_t _mp_dma_take(_mp_dma_slot_t *slot, mp_bool_t *fail) {
		_mp_dma_ctl_t *ctl = slot->ctl;
		uint32_t isr = slot->index < 4 ? ctl->LISR : ctl->HISR;
		uint32_t all = _mp_dma_bits(slot->index, 0x3DUL);
		uint32_t flags = isr & all;

		if(slot->index < 4)
			ctl->LIFCR = all;
		else
			ctl->HIFCR = all;
		*fail = (flags & _mp_dma_bits(slot->index, 1UL << 3)) ? YES : NO;
		if((flags & _mp_dma_bits(slot->index, (1UL << 5) | (1UL << 3))) == 0)
			return(0);
		return(flags);
	}

	static void _mp_dma_force_off(_mp_dma_slot_t *slot) {
		_mp_dma_hw_t *hw = slot->hw;
		_mp_dma_ctl_t *ctl = slot->ctl;
		uint32_t all = _mp_dma_bits(slot->index, 0x3DUL);

		hw->CR &= ~1UL;
		if(slot->index < 4)
			ctl->LIFCR = all;
		else
			ctl->HIFCR = all;
		/* the peripheral request stays off while nothing is shifting, so the clock may change */
		if(slot->arm)
			slot->arm(slot->user, NO);
	}

	static mp_ret_t _mp_dma_program(_mp_dma_slot_t *slot, mp_dma_t *dma, mp_dma_seg_t *seg) {
		_mp_dma_hw_t *hw = slot->hw;
		uint32_t cr;
		unsigned long spin = 0;

		cr = (1UL << 4) | (1UL << 2) | (2UL << 16);
		cr |= (uint32_t)slot->chsel << 25;
		if(slot->dir == MP_DMA_TO_DEVICE)
			cr |= 1UL << 6;
		if(seg->fixed == NO)
			cr |= 1UL << 10;
		if(slot->width == 2)
			cr |= (1UL << 11) | (1UL << 13);
		else if(slot->width == 4)
			cr |= (2UL << 11) | (2UL << 13);

		MP_INTERRUPT_SAFE_BEGIN
		if(hw->CR & 1UL) {
			hw->CR &= ~1UL;
			spin = 10000UL;
			while((hw->CR & 1UL) && --spin)
				;
		}
		if((hw->CR & 1UL) == 0) {
			hw->FCR = 0x21UL;
			if(slot->index < 4)
				((_mp_dma_ctl_t *)slot->ctl)->LIFCR = _mp_dma_bits(slot->index, 0x3DUL);
			else
				((_mp_dma_ctl_t *)slot->ctl)->HIFCR = _mp_dma_bits(slot->index, 0x3DUL);
			hw->NDTR = (uint32_t)seg->len;
			hw->PAR = (uint32_t)(unsigned long)slot->periph;
			hw->M0AR = (uint32_t)seg->bus;
			if(slot->prep)
				slot->prep(slot->user);
			if(slot->arm)
				slot->arm(slot->user, YES);
			slot->xfer = dma;
			hw->CR = cr | 1UL;
			spin = 1UL;
		}
		else
			spin = 0;
		MP_INTERRUPT_SAFE_END
		return(spin ? TRUE : FALSE);
	}

	static void _mp_dma_clock(unsigned char unit, mp_bool_t on) {
		static unsigned char depth[2];
		uint32_t bit = unit ? RCC_AHB1_DMA2 : RCC_AHB1_DMA1;

		if(on == YES) {
			if(depth[unit]++ == 0) {
				RCC->AHB1ENR |= bit;
				(void)RCC->AHB1ENR;
			}
		}
		else if(depth[unit] && --depth[unit] == 0)
			RCC->AHB1ENR &= ~bit;
	}

#elif defined(STM32F10X_LD) || defined(STM32F10X_MD) || defined(STM32F10X_HD) || defined(STM32F10X_XL) || defined(STM32F10X_CL)

	#define _MP_DMA1 0x40020000UL
	#define _MP_DMA2 0x40020400UL

	typedef struct {
		__IO uint32_t ISR, IFCR;
	} _mp_dma_ctl_t;

	typedef struct {
		__IO uint32_t CCR, CNDTR, CPAR, CMAR, RESERVED;
	} _mp_dma_hw_t;

	static const _mp_dma_req_t _mp_dma_table[] = {
		{ "adc1", 0, 0, 0, DMA1_Channel1_IRQn },
		{ "spi1.rx", 0, 1, 0, DMA1_Channel2_IRQn },
		{ "spi1.tx", 0, 2, 0, DMA1_Channel3_IRQn },
		{ "usart3.tx", 0, 1, 0, DMA1_Channel2_IRQn },
		{ "usart3.rx", 0, 2, 0, DMA1_Channel3_IRQn },
		{ "spi2.rx", 0, 3, 0, DMA1_Channel4_IRQn },
		{ "usart1.tx", 0, 3, 0, DMA1_Channel4_IRQn },
		{ "i2c2.tx", 0, 3, 0, DMA1_Channel4_IRQn },
		{ "spi2.tx", 0, 4, 0, DMA1_Channel5_IRQn },
		{ "usart1.rx", 0, 4, 0, DMA1_Channel5_IRQn },
		{ "i2c2.rx", 0, 4, 0, DMA1_Channel5_IRQn },
		{ "usart2.rx", 0, 5, 0, DMA1_Channel6_IRQn },
		{ "i2c1.tx", 0, 5, 0, DMA1_Channel6_IRQn },
		{ "usart2.tx", 0, 6, 0, DMA1_Channel7_IRQn },
		{ "i2c1.rx", 0, 6, 0, DMA1_Channel7_IRQn },
#if defined(STM32F10X_HD) || defined(STM32F10X_XL)
		{ "spi3.rx", 1, 0, 0, DMA2_Channel1_IRQn },
		{ "spi3.tx", 1, 1, 0, DMA2_Channel2_IRQn },
#endif
	};

	static void *_mp_dma_ctl(unsigned char unit) {
		return((void *)(unit ? _MP_DMA2 : _MP_DMA1));
	}

	static void *_mp_dma_hw(unsigned char unit, unsigned char index) {
		return((void *)((unsigned long)_mp_dma_ctl(unit) + 0x08UL + (unsigned long)index * 0x14UL));
	}

	static uint32_t _mp_dma_take(_mp_dma_slot_t *slot, mp_bool_t *fail) {
		_mp_dma_ctl_t *ctl = slot->ctl;
		uint32_t flags = (ctl->ISR >> (slot->index * 4)) & 0xFUL;

		ctl->IFCR = 0xFUL << (slot->index * 4);
		*fail = (flags & 0x8UL) ? YES : NO;
		if((flags & 0xAUL) == 0)
			return(0);
		return(flags);
	}

	static void _mp_dma_force_off(_mp_dma_slot_t *slot) {
		_mp_dma_hw_t *hw = slot->hw;
		_mp_dma_ctl_t *ctl = slot->ctl;

		hw->CCR &= ~1UL;
		ctl->IFCR = 0xFUL << (slot->index * 4);
		/* the peripheral request stays off while nothing is shifting, so the clock may change */
		if(slot->arm)
			slot->arm(slot->user, NO);
	}

	static mp_ret_t _mp_dma_program(_mp_dma_slot_t *slot, mp_dma_t *dma, mp_dma_seg_t *seg) {
		_mp_dma_hw_t *hw = slot->hw;
		_mp_dma_ctl_t *ctl = slot->ctl;
		uint32_t ccr;
		unsigned long spin = 0;

		/* high priority, so another channel does not stall a shift register */
		ccr = (1UL << 1) | (1UL << 3) | (2UL << 12);
		if(slot->dir == MP_DMA_TO_DEVICE)
			ccr |= 1UL << 4;
		if(seg->fixed == NO)
			ccr |= 1UL << 7;
		if(slot->width == 2)
			ccr |= (1UL << 8) | (1UL << 10);
		else if(slot->width == 4)
			ccr |= (2UL << 8) | (2UL << 10);

		MP_INTERRUPT_SAFE_BEGIN
		hw->CCR &= ~1UL;
		spin = 1000UL;
		while((hw->CCR & 1UL) && --spin)
			;
		if((hw->CCR & 1UL) == 0) {
			ctl->IFCR = 0xFUL << (slot->index * 4);
			hw->CNDTR = (uint32_t)seg->len;
			hw->CPAR = (uint32_t)(unsigned long)slot->periph;
			hw->CMAR = (uint32_t)seg->bus;
			if(slot->prep)
				slot->prep(slot->user);
			if(slot->arm)
				slot->arm(slot->user, YES);
			slot->xfer = dma;
			hw->CCR = ccr | 1UL;
			spin = 1UL;
		}
		else
			spin = 0;
		MP_INTERRUPT_SAFE_END
		return(spin ? TRUE : FALSE);
	}

	static void _mp_dma_clock(unsigned char unit, mp_bool_t on) {
		static unsigned char depth[2];
		uint32_t bit = unit ? RCC_AHB_DMA2 : RCC_AHB_DMA1;

		if(on == YES) {
			if(depth[unit]++ == 0) {
				RCC->AHBENR |= bit;
				(void)RCC->AHBENR;
			}
		}
		else if(depth[unit] && --depth[unit] == 0)
			RCC->AHBENR &= ~bit;
	}

#else

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
		(void)request;
		(void)dir;
		(void)periph;
		(void)width;
		(void)arm;
		(void)prep;
		(void)user;
		(void)who;
		return(NULL);
	}

	void mp_dma_release(mp_dma_engine_t *engine) {
		(void)engine;
	}

#endif

#if defined(STM32F407xx) || defined(STM32F10X_LD) || defined(STM32F10X_MD) || defined(STM32F10X_HD) || defined(STM32F10X_XL) || defined(STM32F10X_CL)

	static _mp_dma_slot_t _mp_dma_slots[_MP_DMA_CLAIMS];

	static _mp_dma_slot_t *_mp_dma_holder(unsigned char unit, unsigned char index) {
		int i;

		for(i = 0; i < _MP_DMA_CLAIMS; i++) {
			if(_mp_dma_slots[i].claimed == NO)
				continue;
			if(_mp_dma_slots[i].unit == unit && _mp_dma_slots[i].index == index)
				return(&_mp_dma_slots[i]);
		}
		return(NULL);
	}

	static _mp_dma_slot_t *_mp_dma_free(void) {
		int i;

		for(i = 0; i < _MP_DMA_CLAIMS; i++)
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

	static void _mp_dma_irq(void *user) {
		_mp_dma_slot_t *slot = user;
		mp_dma_t *dma;
		mp_bool_t fail = NO;
		uint32_t flags;

		dma = NULL;
		MP_INTERRUPT_SAFE_BEGIN
		flags = _mp_dma_take(slot, &fail);
		if(flags) {
			dma = slot->xfer;
			if(dma)
				slot->xfer = NULL;
			_mp_dma_force_off(slot);
		}
		MP_INTERRUPT_SAFE_END

		if(dma)
			mp_dma_done(dma, fail == YES ? MP_DMA_FAIL : MP_DMA_DONE);
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
		if(!seg || seg->len > _MP_DMA_MAXLEN || seg->bus == 0)
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
			held = _mp_dma_holder(_mp_dma_table[i].unit, _mp_dma_table[i].index);
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
		slot->unit = req->unit;
		slot->index = req->index;
		slot->chsel = req->chsel;
		slot->irq = req->irq;
		slot->ctl = _mp_dma_ctl(req->unit);
		slot->hw = _mp_dma_hw(req->unit, req->index);
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
		_mp_dma_clock(slot->unit, YES);
		_mp_dma_force_off(slot);
		if(!mp_interrupt_set(slot->irq, _mp_dma_irq, slot, who)) {
			mp_printk("DMA %s: can not arm the interrupt", request);
			mp_dma_release(&slot->engine);
			return(NULL);
		}
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
		MP_INTERRUPT_SAFE_END
		if(slot->arm)
			slot->arm(slot->user, NO);
		mp_interrupt_unset(slot->irq);
		slot->claimed = NO;
		slot->who = NULL;
		slot->arm = NULL;
		slot->prep = NULL;
		_mp_dma_clock(slot->unit, NO);
	}

#endif

/** @} */

#endif
