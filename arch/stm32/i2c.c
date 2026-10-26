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
@defgroup mpArchSTM32I2C I2C master

@ingroup mpArchSTM32

Phases of a transfer:
- IDLE: nothing on the wire, or a STOP on its way out.
- ADDRESS: START asked, the event interrupt answers SB with the address
  and ADDR by entering DATA. mp_i2c_waitStart() does the same by polling.
- DATA: one TX or RX dispatch per TXE or RXNE, only while the matching
  direction is enabled. The event lines stay masked otherwise, so a
  pending TXE or BTF can not loop while regMaster prepares the next step.

mp_i2c_txStart() does nothing while a transfer in the same direction
runs (the regMaster ASR calls it on each pass), and sends a repeated
START when the direction changes. In receive mode mp_i2c_txStop() clears
ACK and asks for the STOP: the byte on the wire is the last one.

Every byte is served by an interrupt, so SCL never runs faster than the
core can follow: at most 9 bits per _I2C_BYTE_CYCLES core cycles, 100 kHz
at 8 MHz. A late STOP lets one more byte in, and the F1 block then keeps
BUSY; a START over a pending STOP never raises SB. A START therefore
resets a bus left in that state, and mp_i2c_watch() frees a transfer that
saw no event for _I2C_STALL_MS.

@{
*/

#define _I2C_TIMEOUT 100000UL
#define _I2C_BYTE_CYCLES 720UL
#define _I2C_STALL_MS 100UL

static mp_list_t __i2c;

static void _mp_i2c_event(void *user);
static void _mp_i2c_error(void *user);

static void _mp_i2c_irq_update(mp_i2c_t *i2c) {
	uint32_t cr2;
	mp_bool_t serve;

	MP_INTERRUPT_SAFE_BEGIN

	cr2 = (i2c->regs->CR2 & ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN)) | I2C_CR2_ITERREN;
	if(i2c->phase == MP_I2C_PHASE_ADDRESS)
		cr2 |= I2C_CR2_ITEVTEN;
	else if(i2c->phase == MP_I2C_PHASE_DATA) {
		serve = i2c->wire == 1 ? i2c->txEnabled : i2c->rxEnabled;
		if(serve == YES)
			cr2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN;
	}
	i2c->regs->CR2 = cr2;

	MP_INTERRUPT_SAFE_END
}

static mp_bool_t _mp_i2c_poll(volatile uint32_t *reg, uint32_t mask, mp_bool_t set) {
	unsigned long timeout = _I2C_TIMEOUT;

	while(((*reg & mask) != 0) != (set == YES)) {
		if(--timeout == 0)
			return(NO);
	}
	return(YES);
}

/* one edge, long enough for the analog filter to follow the pin */
static void _mp_i2c_edge(mp_gpio_port_t *pin, int high) {
	if(high)
		pin->base->BSRR = 1UL << pin->pin;
	else
		mp_gpio_pin_reset(pin->base, pin->pin);
	mp_clock_nanoDelay(5000);
}

/*
 * A slave cut mid-byte needs clocks to release SDA. On the F1 the analog
 * filter can also stick, and then START never raises SB: SWRST does not
 * clear it, a transition on each pin does.
 */
static void _mp_i2c_recover(mp_i2c_t *i2c) {
	int a;

	if(i2c->regs)
		i2c->regs->CR1 &= ~I2C_CR1_PE;

	_mp_i2c_edge(i2c->sda, 1);
	_mp_i2c_edge(i2c->clk, 1);
	mp_gpio_mode(i2c->sda, MP_GPIO_MODE_OUTPUT_OD);
	mp_gpio_mode(i2c->clk, MP_GPIO_MODE_OUTPUT_OD);
	_mp_i2c_edge(i2c->sda, 1);
	_mp_i2c_edge(i2c->clk, 1);

	for(a=0; a<9 && !mp_gpio_raw_read(i2c->sda); a++) {
		_mp_i2c_edge(i2c->clk, 0);
		_mp_i2c_edge(i2c->clk, 1);
	}

	_mp_i2c_edge(i2c->sda, 0);
	_mp_i2c_edge(i2c->clk, 0);
	_mp_i2c_edge(i2c->clk, 1);
	_mp_i2c_edge(i2c->sda, 1);
}

mp_ret_t mp_i2c_init() {
	mp_list_init(&__i2c);
	return(TRUE);
}

mp_ret_t mp_i2c_fini() {
	while(__i2c.first)
		mp_i2c_close(__i2c.first->user);
	return(TRUE);
}

mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_t *i2c, mp_options_t *options, char *who) {
	char *value;

	memset(i2c, 0, sizeof(*i2c));

	i2c->gate = mp_gate_handle(mp_options_get(options, "gate"), who);
	if(!i2c->gate)
		return(FALSE);
	i2c->regs = i2c->gate->base;

	value = mp_options_get(options, "sda");
	i2c->sda = mp_gpio_text_handle(value ? value : (char *)i2c->gate->pin0, who);
	value = mp_options_get(options, "clk");
	i2c->clk = mp_gpio_text_handle(value ? value : (char *)i2c->gate->pin1, who);
	if(!i2c->sda || !i2c->clk) {
		mp_printk("I2C %s: a pin is not available", i2c->gate->portDevice);
		mp_i2c_close(i2c);
		return(FALSE);
	}

	return(TRUE);
}

/* rounded up, so SCL never runs faster than asked nor than the core serves */
static void _mp_i2c_timing(mp_i2c_t *i2c) {
	I2C_TypeDef *regs = i2c->regs;
	unsigned long pclk = mp_gate_pclk(i2c->gate);
	unsigned long mhz = pclk / 1000000UL;
	unsigned long scl = i2c->frequency;
	unsigned long most = mp_clock_hclk() / _I2C_BYTE_CYCLES * 9UL;
	uint32_t ccr;

	if(scl > most)
		scl = most;
	if(scl < 10000UL)
		scl = 10000UL;

	regs->CR2 = (regs->CR2 & ~I2C_CR2_FREQ) | mhz;
	if(scl <= 100000UL) {
		ccr = (pclk + 2 * scl - 1) / (2 * scl);
		if(ccr < 4)
			ccr = 4;
		regs->TRISE = mhz + 1;
	}
	else {
		ccr = (pclk + 3 * scl - 1) / (3 * scl);
		if(ccr < 1)
			ccr = 1;
		ccr |= I2C_CCR_FS;
		regs->TRISE = mhz * 300 / 1000 + 1;
	}
	regs->CCR = ccr;
}

mp_ret_t mp_i2c_setup(mp_i2c_t *i2c, mp_options_t *options) {
	I2C_TypeDef *regs = i2c->regs;
	unsigned long pclk = mp_gate_pclk(i2c->gate);
	unsigned long mhz = pclk / 1000000UL;
	unsigned long frequency = 100000UL;
	char *value;

	value = mp_options_get(options, "frequency");
	if(value)
		frequency = strtoul(value, NULL, 10);
	if(frequency == 0 || frequency > 400000UL)
		frequency = 400000UL;
	i2c->frequency = frequency;

	value = mp_options_get(options, "role");
	if(value && mp_options_cmp(value, "slave") == TRUE) {
		mp_printk("I2C %s: slave role is not supported", i2c->gate->portDevice);
		mp_i2c_close(i2c);
		return(FALSE);
	}

	if(mhz < 2) {
		mp_printk("I2C %s: APB1 at %lu Hz is too slow", i2c->gate->portDevice, pclk);
		mp_i2c_close(i2c);
		return(FALSE);
	}

	_mp_i2c_recover(i2c);

	MP_INTERRUPT_SAFE_BEGIN

	mp_gpio_mode(i2c->sda, MP_GPIO_MODE_AF_OD);
	mp_gpio_mode(i2c->clk, MP_GPIO_MODE_AF_OD);
#if defined(STM32F407xx)
	mp_gpio_af(i2c->sda, 4);
	mp_gpio_af(i2c->clk, 4);
#endif

	regs->CR1 = I2C_CR1_SWRST;
	regs->CR1 = 0;
	regs->CR2 = 0;
	_mp_i2c_timing(i2c);
	regs->OAR1 = I2C_OAR1_BIT14;
	regs->CR1 = I2C_CR1_PE;

	i2c->phase = MP_I2C_PHASE_IDLE;
	i2c->stopArmed = NO;
	i2c->txEnabled = NO;
	i2c->rxEnabled = NO;
	i2c->mode = 1;
	i2c->wire = 1;

	mp_interrupt_set(i2c->gate->irq, _mp_i2c_event, i2c, i2c->gate->portDevice);
	mp_interrupt_set(i2c->gate->irq2, _mp_i2c_error, i2c, i2c->gate->portDevice);

	MP_INTERRUPT_SAFE_END

	_mp_i2c_irq_update(i2c);

	mp_list_add_last(&__i2c, &i2c->item, i2c);

	return(TRUE);
}

void mp_i2c_reset(mp_i2c_t *i2c) {
	I2C_TypeDef *regs;

	if(!i2c || !i2c->regs || !i2c->sda || !i2c->clk)
		return;
	regs = i2c->regs;
	_mp_i2c_recover(i2c);

	MP_INTERRUPT_SAFE_BEGIN

	mp_gpio_mode(i2c->sda, MP_GPIO_MODE_AF_OD);
	mp_gpio_mode(i2c->clk, MP_GPIO_MODE_AF_OD);
#if defined(STM32F407xx)
	mp_gpio_af(i2c->sda, 4);
	mp_gpio_af(i2c->clk, 4);
#endif

	regs->CR1 = I2C_CR1_SWRST;
	regs->CR1 = 0;
	regs->CR2 = 0;
	_mp_i2c_timing(i2c);
	regs->OAR1 = I2C_OAR1_BIT14;
	regs->CR1 = I2C_CR1_PE;

	i2c->phase = MP_I2C_PHASE_IDLE;
	i2c->stopArmed = NO;
	i2c->txEnabled = NO;
	i2c->rxEnabled = NO;
	i2c->mode = 1;
	i2c->wire = 1;

	MP_INTERRUPT_SAFE_END

	_mp_i2c_irq_update(i2c);
}

/**
 * @brief Recompute the bus timing after APB1 changed
 *
 * The clock only changes while mp_i2c_quiet() holds.
 */
void mp_i2c_retime(void) {
	mp_list_item_t *item;
	mp_i2c_t *i2c;
	I2C_TypeDef *regs;
	mp_bool_t stuck;

	for(item = __i2c.first; item; item = item->next) {
		i2c = item->user;
		regs = i2c->regs;
		stuck = NO;
		MP_INTERRUPT_SAFE_BEGIN
		/* PE off/on alone leaves BUSY stuck on F1; SWRST clears it */
		regs->CR1 &= ~I2C_CR1_PE;
		regs->CR1 = I2C_CR1_SWRST;
		regs->CR1 = 0;
		regs->CR2 = 0;
		_mp_i2c_timing(i2c);
		regs->OAR1 = I2C_OAR1_BIT14;
		regs->CR1 = I2C_CR1_PE;
		if(i2c->phase == MP_I2C_PHASE_IDLE && (regs->SR2 & I2C_SR2_BUSY))
			stuck = YES;
		MP_INTERRUPT_SAFE_END
		if(stuck == YES)
			mp_i2c_reset(i2c);
		else
			_mp_i2c_irq_update(i2c);
	}
}

/**
 * @brief No transfer runs on any bus
 *
 * @return YES when the clock may change or stop
 */
mp_bool_t mp_i2c_quiet(void) {
	mp_list_item_t *item;
	mp_i2c_t *i2c;

	for(item = __i2c.first; item; item = item->next) {
		i2c = item->user;
		if(i2c->phase != MP_I2C_PHASE_IDLE || (i2c->regs->SR2 & I2C_SR2_BUSY))
			return(NO);
	}
	return(YES);
}

/**
 * @brief Free the buses a transfer left hanging
 *
 * A START that never raised SB, or a slave that stopped clocking, leaves
 * no interrupt to end the transfer, and the bus would refuse every clock
 * change. After _I2C_STALL_MS without an event the bus is reset and the
 * transfer ends as a NACK. The arch calls it before each sleep.
 */
void mp_i2c_watch(void) {
	mp_list_item_t *item;
	mp_i2c_t *i2c;
	unsigned long stall = _I2C_STALL_MS * mp_clock_tick_hz() / 1000UL;
	mp_bool_t stalled;

	for(item = __i2c.first; item; item = item->next) {
		i2c = item->user;
		MP_INTERRUPT_SAFE_BEGIN
		stalled = i2c->phase != MP_I2C_PHASE_IDLE && mp_clock_ticks() - i2c->since >= stall ? YES : NO;
		MP_INTERRUPT_SAFE_END
		if(stalled == NO)
			continue;

		mp_printk("I2C %s: stalled, reset", i2c->gate->portDevice);
		mp_i2c_reset(i2c);
		MP_INTERRUPT_SAFE_BEGIN
		if(i2c->intDispatch)
			i2c->intDispatch(i2c, MP_I2C_FL_NACK);
		MP_INTERRUPT_SAFE_END
	}
}

mp_ret_t mp_i2c_close(mp_i2c_t *i2c) {
	mp_list_item_t *item;

	for(item = __i2c.first; item; item = item->next) {
		if(item->user == i2c) {
			mp_list_remove(&__i2c, &i2c->item);
			break;
		}
	}

	if(i2c->gate) {
		mp_interrupt_unset(i2c->gate->irq);
		mp_interrupt_unset(i2c->gate->irq2);
		i2c->regs->CR2 = 0;
		i2c->regs->CR1 = 0;
	}

	if(i2c->sda)
		mp_gpio_release(i2c->sda);
	if(i2c->clk)
		mp_gpio_release(i2c->clk);
	i2c->sda = i2c->clk = NULL;

	if(i2c->gate)
		mp_gate_release(i2c->gate);
	i2c->gate = NULL;

	return(TRUE);
}

void mp_i2c_enable_rx(mp_i2c_t *i2c) {
	i2c->rxEnabled = YES;
	_mp_i2c_irq_update(i2c);
}

void mp_i2c_disable_rx(mp_i2c_t *i2c) {
	i2c->rxEnabled = NO;
	_mp_i2c_irq_update(i2c);
}

void mp_i2c_enable_tx(mp_i2c_t *i2c) {
	i2c->txEnabled = YES;
	_mp_i2c_irq_update(i2c);
}

void mp_i2c_disable_tx(mp_i2c_t *i2c) {
	i2c->txEnabled = NO;
	_mp_i2c_irq_update(i2c);
}

void mp_i2c_mode(mp_i2c_t *i2c, char mode) {
	i2c->mode = mode ? 1 : 0;
}

void mp_i2c_txStart(mp_i2c_t *i2c) {
	I2C_TypeDef *regs = i2c->regs;

	MP_INTERRUPT_SAFE_BEGIN

	if(i2c->phase == MP_I2C_PHASE_ADDRESS)
		goto done;
	if(i2c->phase == MP_I2C_PHASE_DATA && i2c->wire == i2c->mode)
		goto done;

	/* a STOP from the previous transfer may still be on its way out */
	if(i2c->phase == MP_I2C_PHASE_IDLE) {
		_mp_i2c_poll(&regs->CR1, I2C_CR1_STOP, NO);
		_mp_i2c_poll(&regs->SR2, I2C_SR2_BUSY, NO);
		if((regs->CR1 & (I2C_CR1_STOP | I2C_CR1_START)) || (regs->SR2 & I2C_SR2_BUSY) || (regs->SR1 & I2C_SR1_RXNE)) {
			char mode = i2c->mode;
			mp_bool_t tx = i2c->txEnabled;
			mp_bool_t rx = i2c->rxEnabled;

			mp_i2c_reset(i2c);
			i2c->mode = mode;
			i2c->txEnabled = tx;
			i2c->rxEnabled = rx;
		}
	}
	i2c->since = mp_clock_ticks();
	/* STOPF left set blocks the next START and holds the event line */
	if(regs->SR1 & I2C_SR1_STOPF) {
		(void)regs->SR1;
		regs->CR1 |= I2C_CR1_PE;
	}

	i2c->wire = i2c->mode;
	i2c->stopArmed = NO;
	i2c->phase = MP_I2C_PHASE_ADDRESS;

	if(i2c->wire == 0)
		regs->CR1 |= I2C_CR1_ACK;
	regs->CR1 |= I2C_CR1_START;

	_mp_i2c_irq_update(i2c);

done:
	MP_INTERRUPT_SAFE_END
}

void mp_i2c_waitStart(mp_i2c_t *i2c) {
	I2C_TypeDef *regs = i2c->regs;

	MP_INTERRUPT_SAFE_BEGIN

	if(i2c->phase != MP_I2C_PHASE_ADDRESS)
		goto done;

	if(_mp_i2c_poll(&regs->SR1, I2C_SR1_SB, YES) == NO)
		goto done;
	regs->DR = (i2c->slave << 1) | (i2c->wire == 0 ? 1 : 0);

	if(_mp_i2c_poll(&regs->SR1, I2C_SR1_ADDR | I2C_SR1_AF, YES) == NO)
		goto done;
	if(regs->SR1 & I2C_SR1_AF) {
		/* the error interrupt reports the NACK */
		goto done;
	}

	(void)regs->SR2;
	i2c->phase = MP_I2C_PHASE_DATA;
	_mp_i2c_irq_update(i2c);

done:
	MP_INTERRUPT_SAFE_END
}

void mp_i2c_txStop(mp_i2c_t *i2c) {
	I2C_TypeDef *regs = i2c->regs;

	MP_INTERRUPT_SAFE_BEGIN

	if(i2c->phase == MP_I2C_PHASE_IDLE || i2c->stopArmed == YES)
		goto done;

	if(i2c->wire == 0) {
		regs->CR1 &= ~I2C_CR1_ACK;
		regs->CR1 |= I2C_CR1_STOP;
		i2c->stopArmed = YES;
	}
	else {
		regs->CR1 |= I2C_CR1_STOP;
		i2c->phase = MP_I2C_PHASE_IDLE;
	}

	_mp_i2c_irq_update(i2c);

done:
	MP_INTERRUPT_SAFE_END
}

void mp_i2c_txNACK(mp_i2c_t *i2c) {
	i2c->regs->CR1 &= ~I2C_CR1_ACK;
}

void mp_i2c_waitStop(mp_i2c_t *i2c) {
	_mp_i2c_poll(&i2c->regs->CR1, I2C_CR1_STOP, NO);
}

void mp_i2c_clearFlags(mp_i2c_t *i2c) {
	(void)i2c->regs->SR1;
	(void)i2c->regs->SR2;
	i2c->regs->SR1 = 0;
}

static void _mp_i2c_event(void *user) {
	mp_i2c_t *i2c = user;
	I2C_TypeDef *regs = i2c->regs;
	uint32_t sr1 = regs->SR1;
	mp_bool_t armed;

	i2c->since = mp_clock_ticks();

	/* a leftover STOPF re-enters this interrupt forever and starves the DMA */
	if(sr1 & I2C_SR1_STOPF) {
		regs->CR1 = (regs->CR1 | I2C_CR1_PE) & ~I2C_CR1_START;
		if(i2c->phase == MP_I2C_PHASE_ADDRESS)
			regs->CR1 |= I2C_CR1_START;
		return;
	}

	if(i2c->phase == MP_I2C_PHASE_ADDRESS) {
		if(sr1 & I2C_SR1_SB)
			regs->DR = (i2c->slave << 1) | (i2c->wire == 0 ? 1 : 0);
		else if(sr1 & I2C_SR1_ADDR) {
			(void)regs->SR2;
			i2c->phase = MP_I2C_PHASE_DATA;
			_mp_i2c_irq_update(i2c);
		}
		return;
	}

	if(i2c->phase == MP_I2C_PHASE_DATA) {
		if(i2c->wire == 1 && (sr1 & I2C_SR1_TXE)) {
			if(i2c->txEnabled == YES && i2c->intDispatch)
				i2c->intDispatch(i2c, MP_I2C_FL_TX);
			else
				_mp_i2c_irq_update(i2c);
			return;
		}

		if(i2c->wire == 0 && (sr1 & I2C_SR1_RXNE)) {
			armed = i2c->stopArmed;
			if(i2c->rxEnabled == YES && i2c->intDispatch)
				i2c->intDispatch(i2c, MP_I2C_FL_RX);
			else
				(void)regs->DR;

			if(armed == YES) {
				i2c->phase = MP_I2C_PHASE_IDLE;
				i2c->stopArmed = NO;
				_mp_i2c_irq_update(i2c);
			}
			return;
		}
	}

	/* nothing to serve here: mask until the next START */
	_mp_i2c_irq_update(i2c);
}

static void _mp_i2c_error(void *user) {
	mp_i2c_t *i2c = user;
	I2C_TypeDef *regs = i2c->regs;
	uint32_t sr1 = regs->SR1;
	uint32_t errors = sr1 & I2C_SR1_ERRORS;

	/* rc_w0: writing 0 clears only the flags seen */
	regs->SR1 = ~errors & 0xFFFF;

	if(errors & (I2C_SR1_AF | I2C_SR1_BERR))
		regs->CR1 |= I2C_CR1_STOP;

	if(errors & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) {
		i2c->phase = MP_I2C_PHASE_IDLE;
		i2c->stopArmed = NO;
		_mp_i2c_irq_update(i2c);

		if(i2c->intDispatch)
			i2c->intDispatch(i2c, MP_I2C_FL_NACK);
	}
}

/**@}*/
