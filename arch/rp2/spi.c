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
#endif

/**
@defgroup mpArchRP2350SPI SPI

@ingroup mpArchRP2350

PrimeCell SSP, master, mode 0, 8-bit. common/spi owns mp_spi_open and
mp_spi_tx when the bit-bang module is linked, so the hardware master is
compiled only without that module.

@{
*/

static mp_list_t __spis;

void mp_spi_init() {
	mp_list_init(&__spis);
}

void mp_spi_fini() {
}

void mp_spi_retime(void) {
}

mp_bool_t mp_spi_quiet(void) {
	return(YES);
}

#ifdef SUPPORT_COMMON_DMA

/* PrimeCell SSPDMACR, byte 0x24. SPI_TypeDef stops at CPSR.
 * RXDMAE is bit 0, TXDMAE is bit 1. */
#define _MP_SPI_DMACR  0x24UL
#define _MP_SPI_RXDMAE (1UL << 0)
#define _MP_SPI_TXDMAE (1UL << 1)

static volatile uint32_t *_mp_spi_dmacr(mp_spi_t *spi) {
	return((volatile uint32_t *)((unsigned long)spi->regs + _MP_SPI_DMACR));
}

static void _mp_spi_dma_rx(void *user, mp_bool_t on) {
	mp_spi_t *spi = user;
	volatile uint32_t *dmacr = _mp_spi_dmacr(spi);

	if(on == YES)
		*dmacr |= _MP_SPI_RXDMAE;
	else
		*dmacr &= ~_MP_SPI_RXDMAE;
}

static void _mp_spi_dma_tx(void *user, mp_bool_t on) {
	mp_spi_t *spi = user;
	volatile uint32_t *dmacr = _mp_spi_dmacr(spi);

	if(on == YES)
		*dmacr |= _MP_SPI_TXDMAE;
	else
		*dmacr &= ~_MP_SPI_TXDMAE;
}

/* a leftover byte in the receive FIFO would be the next DMA word */
static void _mp_spi_dma_prep(void *user) {
	mp_spi_t *spi = user;
	unsigned int n = 8;

	while(n-- && (spi->regs->SR & SPI_SR_RNE))
		(void)spi->regs->DR;
}

static const char *_mp_spi_dma_name(mp_spi_t *spi, mp_dma_dir_t dir) {
	mp_bool_t rx;

	if(!spi->regs)
		return(NULL);
	rx = dir == MP_DMA_FROM_DEVICE ? YES : NO;
	if(spi->regs == SPI0)
		return(rx == YES ? "spi0.rx" : "spi0.tx");
	if(spi->regs == SPI1)
		return(rx == YES ? "spi1.rx" : "spi1.tx");
	return(NULL);
}

mp_dma_engine_t *mp_spi_engine(mp_spi_t *spi, mp_dma_dir_t dir) {
	mp_dma_engine_t **held;
	const char *request;
	void (*arm)(void *user, mp_bool_t on);
	char *who;

	if(!spi || !spi->regs)
		return(NULL);
	if(dir == MP_DMA_FROM_DEVICE) {
		held = &spi->dmaRx;
		arm = _mp_spi_dma_rx;
	}
	else if(dir == MP_DMA_TO_DEVICE) {
		held = &spi->dmaTx;
		arm = _mp_spi_dma_tx;
	}
	else
		return(NULL);
	if(*held)
		return(*held);

	request = _mp_spi_dma_name(spi, dir);
	if(!request)
		return(NULL);
	who = spi->gate ? spi->gate->byWho : NULL;
	*held = mp_dma_claim(request, dir, &spi->regs->DR, 1, arm, _mp_spi_dma_prep, spi, who);
	return(*held);
}

void mp_spi_engine_drop(mp_spi_t *spi) {
	if(!spi)
		return;
	mp_dma_release(spi->dmaRx);
	mp_dma_release(spi->dmaTx);
	spi->dmaRx = NULL;
	spi->dmaTx = NULL;
}

#ifdef SUPPORT_COMMON_SPI
/* SPI0 SCK is GPIO18 and SPI0 TX is GPIO19. The shared bus asks with the
 * pins it already holds; this is the master the glass takes over. */
static mp_spi_t _mp_spi_bus_hw;

mp_dma_engine_t *mp_spi_bus_engine(mp_spi_bus_t *bus, mp_dma_dir_t dir) {
	if(!bus || !bus->sclk || !bus->mosi)
		return(NULL);
	if(bus->sclk->pin != 18 || bus->mosi->pin != 19)
		return(NULL);
	if(!_mp_spi_bus_hw.regs) {
		memset(&_mp_spi_bus_hw, 0, sizeof(_mp_spi_bus_hw));
		_mp_spi_bus_hw.regs = SPI0;
	}
	return(mp_spi_engine(&_mp_spi_bus_hw, dir));
}

void mp_spi_bus_engine_drop(mp_spi_bus_t *bus) {
	(void)bus;
	if(!_mp_spi_bus_hw.regs)
		return;
	mp_spi_engine_drop(&_mp_spi_bus_hw);
	_mp_spi_bus_hw.regs = NULL;
}
#endif

#endif

#ifdef SUPPORT_COMMON_SPI
/* SPI0 SCK is GP18 and SPI0 TX is GP19. The PrimeCell shifts a byte per
 * FIFO write; the shared bus did it in software. clk_peri stays on PLL_USB
 * so the governor can move clk_sys without retiming the device. */
mp_bool_t mp_spi_bus_hw_open(mp_spi_bus_t *bus) {
	unsigned int spins = 1000000;
	unsigned long peri;
	unsigned long div;

	if(bus->sclk->pin != 18 || bus->mosi->pin != 19)
		return(NO);
	RESETS_RESET &= ~RESETS_SPI0;
	while(spins-- && !(RESETS_RESET_DONE & RESETS_SPI0));
	CLOCKS_CLK_PERI_CTRL = CLOCKS_PERI_ENABLE | (CLOCKS_PERI_AUX_USB << 5);

	IO_BANK0_GPIO_CTRL(bus->sclk->pin) = IO_GPIO_FUNCSEL_SPI;
	IO_BANK0_GPIO_CTRL(bus->mosi->pin) = IO_GPIO_FUNCSEL_SPI;

	SPI0->CR1 = 0;
	peri = mp_clock_peri();
	if(peri < 2000000UL)
		peri = 48000000UL;
	div = (peri + 24000000UL - 1UL) / 24000000UL;
	if(div < 2)
		div = 2;
	if(div & 1UL)
		div++;
	SPI0->CPSR = (uint32_t)div;
	SPI0->CR0 = SPI_CR0_DSS_8;
	SPI0->CR1 = SPI_CR1_SSE;
	return(YES);
}

void mp_spi_bus_hw_flush(mp_spi_bus_t *bus) {
	unsigned int spins = 1000000;

	(void)bus;
	while(spins-- && (SPI0->SR & SPI_SR_BSY));
	while(SPI0->SR & SPI_SR_RNE)
		(void)SPI0->DR;
}

int mp_spi_bus_hw_tx(mp_spi_bus_t *bus, const unsigned char *data, int len) {
	int sent = 0;
	int got = 0;
	unsigned int spins = 1000000;

	while(got < len && spins) {
		/* at most 8 bytes in flight: the RX FIFO holds 8, an overrun
		 * drops a byte and this loop waits out its spins */
		if(sent < len && sent - got < 8 && (SPI0->SR & SPI_SR_TNF))
			SPI0->DR = data[sent++];
		if(SPI0->SR & SPI_SR_RNE) {
			(void)SPI0->DR;
			got++;
			spins = 1000000;
		}
		else
			spins--;
	}
	mp_spi_bus_hw_flush(bus);
	return(1);
}

void mp_spi_bus_hw_close(mp_spi_bus_t *bus) {
	(void)bus;
	SPI0->CR1 = 0;
}
#endif

#ifndef SUPPORT_COMMON_SPI

static void _mp_spi_release(mp_gpio_port_t **pin) {
	if(*pin) {
		mp_gpio_release(*pin);
		*pin = NULL;
	}
}

static unsigned char _mp_spi_xfer(mp_spi_t *spi, unsigned char data) {
	unsigned int spins = 1000000;

	while(spins-- && !(spi->regs->SR & SPI_SR_TNF));
	spi->regs->DR = data;
	spins = 1000000;
	while(spins-- && !(spi->regs->SR & SPI_SR_RNE));
	return((unsigned char)spi->regs->DR);
}

mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_t *spi, mp_options_t *options, char *who) {
	char *value;

	(void)kernel;
	memset(spi, 0, sizeof(*spi));
	value = mp_options_get(options, "gate");
	spi->gate = mp_gate_handle(value, who);
	if(!spi->gate)
		return(FALSE);
	spi->regs = spi->gate->base;

	value = mp_options_get(options, "simo");
	spi->simo = mp_gpio_text_handle(value ? value : (char *)spi->gate->pin0, who);
	value = mp_options_get(options, "somi");
	spi->somi = mp_gpio_text_handle(value ? value : (char *)spi->gate->pin1, who);
	value = mp_options_get(options, "clk");
	spi->clk = mp_gpio_text_handle(value ? value : (char *)spi->gate->pin2, who);
	if(!spi->simo || !spi->somi || !spi->clk) {
		mp_printk("SPI %s: a pin is not available", spi->gate->portDevice);
		mp_spi_close(spi);
		return(FALSE);
	}

	mp_gpio_af(spi->simo, IO_GPIO_FUNCSEL_SPI);
	mp_gpio_af(spi->somi, IO_GPIO_FUNCSEL_SPI);
	mp_gpio_af(spi->clk, IO_GPIO_FUNCSEL_SPI);
	mp_gpio_mode(spi->simo, MP_GPIO_MODE_AF);
	mp_gpio_mode(spi->somi, MP_GPIO_MODE_AF);
	mp_gpio_mode(spi->clk, MP_GPIO_MODE_AF);

	spi->item.next = spi->item.prev = NULL;
	mp_list_add_last(&__spis, &spi->item, spi);
	return(mp_spi_setup(spi, options));
}

mp_ret_t mp_spi_setup(mp_spi_t *spi, mp_options_t *options) {
	unsigned long peri = mp_gate_pclk(spi->gate);
	unsigned long want;
	unsigned long div;
	char *value;

	value = mp_options_get(options, "frequency");
	want = value ? strtoul(value, NULL, 10) : 1000000;
	if(want == 0)
		want = 1000000;
	div = (peri + want - 1) / want;
	if(div < 2)
		div = 2;
	if(div & 1)
		div++;
	if(div > 254)
		div = 254;

	spi->frequency = peri / div;
	spi->regs->CR1 = 0;
	spi->regs->CPSR = div;
	spi->regs->CR0 = SPI_CR0_DSS_8;
	spi->regs->CR1 = SPI_CR1_SSE;
	return(TRUE);
}

mp_ret_t mp_spi_close(mp_spi_t *spi) {
#ifdef SUPPORT_COMMON_DMA
	mp_spi_engine_drop(spi);
#endif
	if(spi->regs)
		spi->regs->CR1 = 0;
	if(spi->item.next || spi->item.prev || __spis.first == &spi->item)
		mp_list_remove(&__spis, &spi->item);
	_mp_spi_release(&spi->simo);
	_mp_spi_release(&spi->somi);
	_mp_spi_release(&spi->clk);
	if(spi->gate) {
		mp_gate_release(spi->gate);
		spi->gate = NULL;
	}
	return(TRUE);
}

void mp_spi_tx(mp_spi_t *spi, unsigned char data) {
	(void)_mp_spi_xfer(spi, data);
}

unsigned char mp_spi_rx(mp_spi_t *spi) {
	return(_mp_spi_xfer(spi, 0xff));
}

void mp_spi_write(mp_spi_t *spi, unsigned char *input, int size) {
	int a;

	for(a = 0; a < size; a++)
		_mp_spi_xfer(spi, input[a]);
}

#endif

/**@}*/
