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
@defgroup mpArchSTM32SPI SPI

@ingroup mpArchSTM32

Options of mp_spi_open(): gate, simo, somi, clk (remap 0 pins when
missing). Options of mp_spi_setup(): frequency (the highest PCLK / 2^n
under it), phase, polarity, first, role, as on the MSP430: phase
"change" shifts on the first edge (CPHA 1), polarity "low" idles low.
Frames are always 8 bits.

@{
*/

static mp_list_t __spi;
static int __spi_count = 0;

static void _mp_spi_interrupt(void *user);

void mp_spi_init() {
	mp_list_init(&__spi);
	__spi_count = 0;
}

void mp_spi_fini() {
	mp_spi_t *spi;

	while(__spi.first) {
		spi = __spi.first->user;
		mp_spi_close(spi);
	}
}

static mp_gpio_port_t *_mp_spi_pin(mp_options_t *options, char *key, const char *fallback, char *who) {
	char *value = mp_options_get(options, key);
	return(mp_gpio_text_handle(value ? value : (char *)fallback, who));
}

mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_t *spi, mp_options_t *options, char *who) {
	memset(spi, 0, sizeof(*spi));

	spi->gate = mp_gate_handle(mp_options_get(options, "gate"), who);
	if(!spi->gate)
		return(FALSE);
	spi->regs = spi->gate->base;

	spi->simo = _mp_spi_pin(options, "simo", spi->gate->pin0, who);
	spi->somi = _mp_spi_pin(options, "somi", spi->gate->pin1, who);
	spi->clk = _mp_spi_pin(options, "clk", spi->gate->pin2, who);
	if(!spi->simo || !spi->somi || !spi->clk) {
		mp_printk("SPI %s: a pin is not available", spi->gate->portDevice);
		mp_spi_close(spi);
		return(FALSE);
	}

	return(TRUE);
}

/* fPCLK / 2^(br + 1), the highest under the frequency */
static uint32_t _mp_spi_br(mp_spi_t *spi) {
	unsigned long pclk = mp_gate_pclk(spi->gate);
	uint32_t br = 0;

	while(br < 7 && (pclk >> (br + 1)) > spi->frequency)
		br++;
	return(br << 3);
}

mp_ret_t mp_spi_setup(mp_spi_t *spi, mp_options_t *options) {
	uint32_t cr1 = SPI_CR1_SSM | SPI_CR1_SSI;
	char *value;

	value = mp_options_get(options, "frequency");
	if(!value) {
		mp_printk("SPI - No frequency specify");
		mp_spi_close(spi);
		return(FALSE);
	}
	spi->frequency = strtoul(value, NULL, 10);
	cr1 |= _mp_spi_br(spi);

	value = mp_options_get(options, "phase");
	if(value && mp_options_cmp(value, "change") == TRUE)
		cr1 |= SPI_CR1_CPHA;

	value = mp_options_get(options, "polarity");
	if(!value || mp_options_cmp(value, "low") == FALSE)
		cr1 |= SPI_CR1_CPOL;

	value = mp_options_get(options, "first");
	if(value && mp_options_cmp(value, "lsb") == TRUE)
		cr1 |= SPI_CR1_LSBFIRST;

	value = mp_options_get(options, "role");
	if(value && mp_options_cmp(value, "slave") == TRUE)
		cr1 &= ~SPI_CR1_SSI;
	else
		cr1 |= SPI_CR1_MSTR;

	MP_INTERRUPT_SAFE_BEGIN

#if defined(STM32F407xx)
	mp_gpio_mode(spi->simo, MP_GPIO_MODE_AF);
	mp_gpio_mode(spi->clk, MP_GPIO_MODE_AF);
	mp_gpio_mode(spi->somi, MP_GPIO_MODE_AF);
	mp_gpio_af(spi->simo, 5);
	mp_gpio_af(spi->clk, 5);
	mp_gpio_af(spi->somi, 5);
#else
	if(cr1 & SPI_CR1_MSTR) {
		mp_gpio_mode(spi->simo, MP_GPIO_MODE_AF);
		mp_gpio_mode(spi->clk, MP_GPIO_MODE_AF);
		mp_gpio_mode(spi->somi, MP_GPIO_MODE_INPUT);
	}
	else {
		mp_gpio_mode(spi->simo, MP_GPIO_MODE_INPUT);
		mp_gpio_mode(spi->clk, MP_GPIO_MODE_INPUT);
		mp_gpio_mode(spi->somi, MP_GPIO_MODE_AF);
	}
#endif

	spi->regs->CR1 = 0;
	spi->regs->CR2 = 0;
	spi->regs->CR1 = cr1;
	spi->regs->CR1 = cr1 | SPI_CR1_SPE;
	spi->ie = 0;

	mp_interrupt_set(spi->gate->irq, _mp_spi_interrupt, spi, spi->gate->portDevice);

	MP_INTERRUPT_SAFE_END

	mp_list_add_last(&__spi, &spi->item, spi);
	__spi_count++;

	return(TRUE);
}

/**
 * @brief Recompute the baud rate dividers after the bus clocks changed
 *
 * The clock only changes while mp_spi_quiet() holds.
 */
void mp_spi_retime(void) {
	mp_list_item_t *item;
	mp_spi_t *spi;
	uint32_t cr1;

	for(item = __spi.first; item; item = item->next) {
		spi = item->user;
		MP_INTERRUPT_SAFE_BEGIN
		cr1 = spi->regs->CR1;
		spi->regs->CR1 = cr1 & ~SPI_CR1_SPE;
		spi->regs->CR1 = ((cr1 & ~SPI_CR1_BR) | _mp_spi_br(spi)) & ~SPI_CR1_SPE;
		spi->regs->CR1 |= cr1 & SPI_CR1_SPE;
		MP_INTERRUPT_SAFE_END
	}
}

/**
 * @brief No frame is shifting and no interrupt waits on one
 *
 * DMA stays requested between packets, and BSY is clear in that gap.
 * Retiming there drops a byte and the rest of the frame is shifted.
 *
 * @return YES when the clock may change or stop
 */
mp_bool_t mp_spi_quiet(void) {
	mp_list_item_t *item;
	mp_spi_t *spi;

	for(item = __spi.first; item; item = item->next) {
		spi = item->user;
		if(spi->ie || (spi->regs->SR & SPI_SR_BSY))
			return(NO);
		if(spi->regs->CR2 & (SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN))
			return(NO);
	}
	return(YES);
}

#ifdef SUPPORT_COMMON_DMA

static void _mp_spi_dma_rx(void *user, mp_bool_t on) {
	mp_spi_t *spi = user;

	if(on == YES)
		spi->regs->CR2 |= SPI_CR2_RXDMAEN;
	else
		spi->regs->CR2 &= ~SPI_CR2_RXDMAEN;
}

static void _mp_spi_dma_tx(void *user, mp_bool_t on) {
	mp_spi_t *spi = user;

	if(on == YES)
		spi->regs->CR2 |= SPI_CR2_TXDMAEN;
	else
		spi->regs->CR2 &= ~SPI_CR2_TXDMAEN;
}

/* a leftover byte is clocked out as the next header */
static void _mp_spi_dma_prep(void *user) {
	mp_spi_t *spi = user;

	spi->regs->CR2 &= ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE);
	while(spi->regs->SR & (SPI_SR_RXNE | SPI_SR_OVR)) {
		(void)spi->regs->DR;
		(void)spi->regs->SR;
	}
}

static const char *_mp_spi_dma_name(mp_spi_t *spi, mp_dma_dir_t dir) {
	char *name;
	mp_bool_t rx;

	if(!spi->gate)
		return(NULL);
	name = spi->gate->portDevice;
	rx = dir == MP_DMA_FROM_DEVICE ? YES : NO;
	if(mp_options_cmp(name, "SPI1") == TRUE)
		return(rx == YES ? "spi1.rx" : "spi1.tx");
	if(mp_options_cmp(name, "SPI2") == TRUE)
		return(rx == YES ? "spi2.rx" : "spi2.tx");
	if(mp_options_cmp(name, "SPI3") == TRUE)
		return(rx == YES ? "spi3.rx" : "spi3.tx");
	return(NULL);
}

mp_dma_engine_t *mp_spi_engine(mp_spi_t *spi, mp_dma_dir_t dir) {
	mp_dma_engine_t **held;
	const char *request;
	void (*arm)(void *user, mp_bool_t on);

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
	*held = mp_dma_claim(request, dir, &spi->regs->DR, 1, arm, _mp_spi_dma_prep, spi, spi->gate->byWho);
	return(*held);
}

void mp_spi_engine_drop(mp_spi_t *spi) {
	if(!spi)
		return;
	mp_dma_release(spi->dmaRx);
	mp_dma_release(spi->dmaTx);
	spi->dmaRx = NULL;
	spi->dmaTx = NULL;
	if(spi->regs)
		spi->regs->CR2 = (spi->regs->CR2 & ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE)) | spi->ie;
}

#endif

mp_ret_t mp_spi_close(mp_spi_t *spi) {
	mp_bool_t listed = NO;
	mp_list_item_t *item;

#ifdef SUPPORT_COMMON_DMA
	mp_spi_engine_drop(spi);
#endif

	for(item = __spi.first; item; item = item->next)
		if(item->user == spi)
			listed = YES;

	if(spi->gate) {
		mp_spi_disable_both(spi);
		while(spi->regs->SR & SPI_SR_BSY);
		spi->regs->CR1 = 0;
		mp_interrupt_unset(spi->gate->irq);
	}

	if(spi->simo)
		mp_gpio_release(spi->simo);
	if(spi->somi)
		mp_gpio_release(spi->somi);
	if(spi->clk)
		mp_gpio_release(spi->clk);
	spi->simo = spi->somi = spi->clk = NULL;

	if(spi->gate)
		mp_gate_release(spi->gate);
	spi->gate = NULL;

	if(listed == YES) {
		mp_list_remove(&__spi, &spi->item);
		__spi_count--;
	}

	return(TRUE);
}

unsigned char mp_spi_rx(mp_spi_t *spi) {
	return(spi->regs->DR);
}

void mp_spi_tx(mp_spi_t *spi, unsigned char data) {
	while(!(spi->regs->SR & SPI_SR_TXE));
	spi->regs->DR = data;
}

void mp_spi_write(mp_spi_t *spi, unsigned char *input, int size) {
	int a;

	for(a=0; a<size; a++) {
		mp_spi_tx(spi, input[a]);
		while(!(spi->regs->SR & SPI_SR_RXNE));
		(void)spi->regs->DR;
	}
}

static void _mp_spi_interrupt(void *user) {
	mp_spi_t *spi = user;
	uint32_t sr = spi->regs->SR;
	uint32_t cr2 = spi->regs->CR2;

	if(!spi->intDispatch) {
		spi->regs->CR2 = cr2 & ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE);
		return;
	}

	if((cr2 & SPI_CR2_RXNEIE) && (sr & SPI_SR_RXNE))
		spi->intDispatch(spi, MP_SPI_IV_RX);
	else if((cr2 & SPI_CR2_TXEIE) && (sr & SPI_SR_TXE))
		spi->intDispatch(spi, MP_SPI_IV_TX);
}

/**@}*/
