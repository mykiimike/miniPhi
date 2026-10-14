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
 * @return YES when the clock may change or stop
 */
mp_bool_t mp_spi_quiet(void) {
	mp_list_item_t *item;
	mp_spi_t *spi;

	for(item = __spi.first; item; item = item->next) {
		spi = item->user;
		if(spi->ie || (spi->regs->SR & SPI_SR_BSY))
			return(NO);
	}
	return(YES);
}

mp_ret_t mp_spi_close(mp_spi_t *spi) {
	mp_bool_t listed = NO;
	mp_list_item_t *item;

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
