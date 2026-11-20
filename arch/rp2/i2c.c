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
@defgroup mpArchRP2350I2C I2C master

@ingroup mpArchRP2350

DW APB I2C. The byte calls follow the STM32 phase machine so regMaster
links. A write holds the last byte until the next write or the STOP, so
STOP can be attached to it. A read is one DATA_CMD at a time. Every wait
gives up, so a missing slave cannot wedge the core. common/i2c uses the
same names, so the master is compiled only without that module.

@{
*/

#define _I2C_STALL_MS 100
#define _I2C_CLR_TX_ABRT 0x54
#define _I2C_CLR_STOP_DET 0x60
#define _I2C_INTR_TX_ABRT (1UL << 6)
#define _I2C_INTR_STOP (1UL << 9)

static mp_list_t __i2cs;

static volatile uint32_t *_mp_i2c_reg(mp_i2c_t *i2c, unsigned off) {
	return((volatile uint32_t *)((uintptr_t)i2c->regs + off));
}

static int _mp_i2c_wait(volatile uint32_t *reg, uint32_t mask, int set) {
	unsigned int spins = 1000000;

	while(spins--) {
		int has = (*reg & mask) != 0;

		if(set ? has : !has)
			return(1);
	}
	return(0);
}

static void _mp_i2c_mask(mp_i2c_t *i2c) {
	uint32_t mask = 0;

	if(i2c->phase == MP_I2C_PHASE_DATA && i2c->wire == 1 && i2c->txEnabled == YES)
		mask |= I2C_INTR_TX_EMPTY;
	if(i2c->phase == MP_I2C_PHASE_DATA && i2c->wire == 0 && i2c->rxEnabled == YES)
		mask |= I2C_INTR_RX_FULL;
	if(i2c->phase != MP_I2C_PHASE_IDLE)
		mask |= _I2C_INTR_TX_ABRT | _I2C_INTR_STOP;
	*_mp_i2c_reg(i2c, I2C_INTR_MASK) = mask;
}

static void _mp_i2c_counts(mp_i2c_t *i2c) {
	unsigned long peri = mp_gate_pclk(i2c->gate);
	unsigned long baud = i2c->frequency ? i2c->frequency : 100000;
	unsigned long period = (peri + baud / 2) / baud;
	unsigned long lcnt = period * 3 / 5;
	unsigned long hcnt = period - lcnt;

	if(hcnt < 8)
		hcnt = 8;
	if(lcnt < 8)
		lcnt = 8;
	if(baud > 100000) {
		*_mp_i2c_reg(i2c, I2C_FS_SCL_HCNT) = hcnt;
		*_mp_i2c_reg(i2c, I2C_FS_SCL_LCNT) = lcnt;
	}
	else {
		*_mp_i2c_reg(i2c, I2C_SS_SCL_HCNT) = hcnt;
		*_mp_i2c_reg(i2c, I2C_SS_SCL_LCNT) = lcnt;
	}
}

static void _mp_i2c_disable(mp_i2c_t *i2c) {
	*_mp_i2c_reg(i2c, I2C_ENABLE) = 0;
	_mp_i2c_wait(_mp_i2c_reg(i2c, I2C_ENABLE_STATUS), 1, 0);
}

static void _mp_i2c_program(mp_i2c_t *i2c) {
	uint32_t con = I2C_CON_MASTER | I2C_CON_RESTART | I2C_CON_SLAVE_DISABLE;

	_mp_i2c_disable(i2c);
	con |= i2c->frequency > 100000 ? I2C_CON_SPEED_FAST : I2C_CON_SPEED_STD;
	*_mp_i2c_reg(i2c, I2C_CON) = con;
	*_mp_i2c_reg(i2c, I2C_TAR) = i2c->slave & 0x7F;
	*_mp_i2c_reg(i2c, I2C_RX_TL) = 0;
	*_mp_i2c_reg(i2c, I2C_TX_TL) = 0;
	_mp_i2c_counts(i2c);
	*_mp_i2c_reg(i2c, I2C_ENABLE) = 1;
}

static void _mp_i2c_push(mp_i2c_t *i2c, uint32_t cmd) {
	if(!_mp_i2c_wait(_mp_i2c_reg(i2c, I2C_STATUS), I2C_STATUS_TFNF, 1))
		return;
	*_mp_i2c_reg(i2c, I2C_DATA_CMD) = cmd;
	i2c->since = mp_clock_ticks();
}

static void _mp_i2c_queue_read(mp_i2c_t *i2c) {
	uint32_t cmd = I2C_DATA_CMD_READ;

	if(i2c->restartArmed == YES) {
		cmd |= I2C_DATA_CMD_RESTART;
		i2c->restartArmed = NO;
	}
	if(i2c->stopArmed == YES) {
		cmd |= I2C_DATA_CMD_STOP;
		i2c->stopArmed = NO;
	}
	_mp_i2c_push(i2c, cmd);
}

static void _mp_i2c_event(void *user);

mp_ret_t mp_i2c_init() {
	mp_list_init(&__i2cs);
	return(TRUE);
}

mp_ret_t mp_i2c_fini() {
	return(TRUE);
}

void mp_i2c_retime(void) {
	mp_list_item_t *item;

	for(item = __i2cs.first; item; item = item->next) {
		mp_i2c_t *i2c = item->user;

		if(i2c->phase != MP_I2C_PHASE_IDLE)
			continue;
		_mp_i2c_program(i2c);
	}
}

mp_bool_t mp_i2c_quiet(void) {
	mp_list_item_t *item;

	for(item = __i2cs.first; item; item = item->next) {
		mp_i2c_t *i2c = item->user;

		if(i2c->phase != MP_I2C_PHASE_IDLE)
			return(NO);
	}
	return(YES);
}

#ifndef SUPPORT_COMMON_I2C

mp_ret_t mp_i2c_open(mp_kernel_t *kernel, mp_i2c_t *i2c, mp_options_t *options, char *who) {
	char *value;

	(void)kernel;
	memset(i2c, 0, sizeof(*i2c));
	value = mp_options_get(options, "gate");
	i2c->gate = mp_gate_handle(value, who);
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

	mp_gpio_af(i2c->sda, IO_GPIO_FUNCSEL_I2C);
	mp_gpio_af(i2c->clk, IO_GPIO_FUNCSEL_I2C);
	mp_gpio_mode(i2c->sda, MP_GPIO_MODE_AF_OD);
	mp_gpio_mode(i2c->clk, MP_GPIO_MODE_AF_OD);

	i2c->frequency = 100000;
	i2c->phase = MP_I2C_PHASE_IDLE;
	i2c->item.next = i2c->item.prev = NULL;
	mp_list_add_last(&__i2cs, &i2c->item, i2c);
	_mp_i2c_program(i2c);
	mp_interrupt_set(i2c->gate->irq, _mp_i2c_event, i2c, i2c->gate->portDevice);
	return(TRUE);
}

mp_ret_t mp_i2c_setup(mp_i2c_t *i2c, mp_options_t *options) {
	char *value = mp_options_get(options, "frequency");

	if(value)
		i2c->frequency = strtoul(value, NULL, 10);
	if(i2c->frequency == 0)
		i2c->frequency = 100000;
	_mp_i2c_program(i2c);
	return(TRUE);
}

mp_ret_t mp_i2c_close(mp_i2c_t *i2c) {
	if(i2c->gate) {
		mp_interrupt_unset(i2c->gate->irq);
		_mp_i2c_disable(i2c);
	}
	if(i2c->item.next || i2c->item.prev || __i2cs.first == &i2c->item)
		mp_list_remove(&__i2cs, &i2c->item);
	if(i2c->sda)
		mp_gpio_release(i2c->sda);
	if(i2c->clk)
		mp_gpio_release(i2c->clk);
	i2c->sda = i2c->clk = NULL;
	if(i2c->gate) {
		mp_gate_release(i2c->gate);
		i2c->gate = NULL;
	}
	return(TRUE);
}

void mp_i2c_reset(mp_i2c_t *i2c) {
	i2c->phase = MP_I2C_PHASE_IDLE;
	i2c->stopArmed = NO;
	i2c->restartArmed = NO;
	i2c->txEnabled = NO;
	i2c->rxEnabled = NO;
	_mp_i2c_program(i2c);
	_mp_i2c_mask(i2c);
}

void mp_i2c_enable_rx(mp_i2c_t *i2c) {
	i2c->rxEnabled = YES;
	if(i2c->phase == MP_I2C_PHASE_DATA && i2c->wire == 0)
		_mp_i2c_queue_read(i2c);
	_mp_i2c_mask(i2c);
}

void mp_i2c_disable_rx(mp_i2c_t *i2c) {
	i2c->rxEnabled = NO;
	_mp_i2c_mask(i2c);
}

void mp_i2c_enable_tx(mp_i2c_t *i2c) {
	i2c->txEnabled = YES;
	_mp_i2c_mask(i2c);
}

void mp_i2c_disable_tx(mp_i2c_t *i2c) {
	i2c->txEnabled = NO;
	_mp_i2c_mask(i2c);
}

void mp_i2c_mode(mp_i2c_t *i2c, char mode) {
	i2c->mode = mode ? 1 : 0;
}

void mp_i2c_txStart(mp_i2c_t *i2c) {
	MP_INTERRUPT_SAFE_BEGIN

	if(i2c->phase == MP_I2C_PHASE_DATA && i2c->wire == i2c->mode)
		goto done;

	/* the last write is still held, so a repeated START can follow it */
	if(i2c->phase == MP_I2C_PHASE_DATA && i2c->holdValid == YES) {
		_mp_i2c_push(i2c, i2c->held);
		i2c->holdValid = NO;
		i2c->restartArmed = YES;
		i2c->wire = i2c->mode;
		i2c->stopArmed = NO;
		i2c->since = mp_clock_ticks();
		_mp_i2c_mask(i2c);
		goto done;
	}

	_mp_i2c_program(i2c);
	i2c->wire = i2c->mode;
	i2c->phase = MP_I2C_PHASE_DATA;
	i2c->stopArmed = NO;
	i2c->restartArmed = NO;
	i2c->holdValid = NO;
	i2c->since = mp_clock_ticks();
	_mp_i2c_mask(i2c);

done:
	MP_INTERRUPT_SAFE_END
}

void mp_i2c_waitStart(mp_i2c_t *i2c) {
	(void)i2c;
}

void mp_i2c_txStop(mp_i2c_t *i2c) {
	MP_INTERRUPT_SAFE_BEGIN
	if(i2c->holdValid == YES) {
		_mp_i2c_push(i2c, i2c->held | I2C_DATA_CMD_STOP);
		i2c->holdValid = NO;
		i2c->stopArmed = NO;
	}
	else
		i2c->stopArmed = YES;
	MP_INTERRUPT_SAFE_END
}

void mp_i2c_txNACK(mp_i2c_t *i2c) {
	i2c->stopArmed = YES;
}

void mp_i2c_waitStop(mp_i2c_t *i2c) {
	_mp_i2c_wait(_mp_i2c_reg(i2c, I2C_STATUS), I2C_STATUS_ACTIVITY, 0);
}

void mp_i2c_clearFlags(mp_i2c_t *i2c) {
	(void)*_mp_i2c_reg(i2c, I2C_CLR_INTR);
}

void mp_i2c_tx(mp_i2c_t *i2c, unsigned char data) {
	if(i2c->holdValid == YES)
		_mp_i2c_push(i2c, i2c->held);
	i2c->held = data;
	i2c->holdValid = YES;
}

unsigned char mp_i2c_rx(mp_i2c_t *i2c) {
	if(!_mp_i2c_wait(_mp_i2c_reg(i2c, I2C_STATUS), I2C_STATUS_RFNE, 1))
		return(0xff);
	return((unsigned char)*_mp_i2c_reg(i2c, I2C_DATA_CMD));
}

void mp_i2c_waitRX(mp_i2c_t *i2c) {
	_mp_i2c_wait(_mp_i2c_reg(i2c, I2C_STATUS), I2C_STATUS_RFNE, 1);
}

void mp_i2c_waitTX(mp_i2c_t *i2c) {
	_mp_i2c_wait(_mp_i2c_reg(i2c, I2C_STATUS), I2C_STATUS_TFE, 1);
}

static void _mp_i2c_event(void *user) {
	mp_i2c_t *i2c = user;
	uint32_t stat = *_mp_i2c_reg(i2c, I2C_INTR_STAT);

	i2c->since = mp_clock_ticks();

	if(stat & _I2C_INTR_TX_ABRT) {
		(void)*_mp_i2c_reg(i2c, _I2C_CLR_TX_ABRT);
		i2c->phase = MP_I2C_PHASE_IDLE;
		i2c->holdValid = NO;
		_mp_i2c_mask(i2c);
		if(i2c->intDispatch)
			i2c->intDispatch(i2c, MP_I2C_FL_NACK);
		return;
	}

	if(stat & _I2C_INTR_STOP) {
		(void)*_mp_i2c_reg(i2c, _I2C_CLR_STOP_DET);
		i2c->phase = MP_I2C_PHASE_IDLE;
		_mp_i2c_mask(i2c);
	}

	if((stat & I2C_INTR_TX_EMPTY) && i2c->txEnabled == YES && i2c->wire == 1 && i2c->phase == MP_I2C_PHASE_DATA) {
		if(i2c->intDispatch)
			i2c->intDispatch(i2c, MP_I2C_FL_TX);
		if(i2c->txEnabled == YES && i2c->holdValid == NO && (*_mp_i2c_reg(i2c, I2C_STATUS) & I2C_STATUS_TFE))
			i2c->txEnabled = NO;
		_mp_i2c_mask(i2c);
	}

	if((stat & I2C_INTR_RX_FULL) && i2c->rxEnabled == YES && i2c->wire == 0 && i2c->phase == MP_I2C_PHASE_DATA) {
		if(i2c->intDispatch)
			i2c->intDispatch(i2c, MP_I2C_FL_RX);
		if(i2c->rxEnabled == YES && i2c->phase == MP_I2C_PHASE_DATA)
			_mp_i2c_queue_read(i2c);
		if(*_mp_i2c_reg(i2c, I2C_STATUS) & I2C_STATUS_RFNE)
			i2c->rxEnabled = NO;
		_mp_i2c_mask(i2c);
	}
}

#endif

void mp_i2c_watch(void) {
	mp_list_item_t *item;

	for(item = __i2cs.first; item; item = item->next) {
		mp_i2c_t *i2c = item->user;

		if(i2c->phase == MP_I2C_PHASE_IDLE)
			continue;
		if((unsigned long)(mp_clock_ticks() - i2c->since) < _I2C_STALL_MS)
			continue;
#ifndef SUPPORT_COMMON_I2C
		mp_i2c_reset(i2c);
#endif
	}
}

/**@}*/
