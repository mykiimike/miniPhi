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
@defgroup mpArchSTM32FSMC FSMC

@ingroup mpArchSTM32

Mode A with separate read and write timings (EXTMOD): LCD controllers
read much slower than they write.

@{
*/

/* D0 to D15, then NOE and NWE */
static const char *_mp_fsmc_bus[] = {
	"pd14", "pd15", "pd0", "pd1", "pe7", "pe8", "pe9", "pe10",
	"pe11", "pe12", "pe13", "pe14", "pe15", "pd8", "pd9", "pd10",
	"pd4", "pd5",
};

static const char *_mp_fsmc_ne[] = { "pd7", "pg9", "pg10", "pg12" };

/* A0 to A25 */
static const char *_mp_fsmc_address[] = {
	"pf0", "pf1", "pf2", "pf3", "pf4", "pf5", "pf12", "pf13",
	"pf14", "pf15", "pg0", "pg1", "pg2", "pg3", "pg4", "pg5",
	"pd11", "pd12", "pd13", "pe3", "pe4", "pe5", "pe6", "pe2",
	"pg13", "pg14",
};

#define _MP_FSMC_COUNT(table) ((int)(sizeof(table) / sizeof((table)[0])))

static int _mp_fsmc_find(const char **table, int count, char *pin) {
	int index;

	if(!pin)
		return(-1);
	for(index = 0; index < count; index++)
		if(mp_options_cmp((char *)table[index], pin) == TRUE)
			return(index);
	return(-1);
}

static unsigned long _mp_fsmc_ns(mp_options_t *options, char *key, unsigned long fallback) {
	char *value = mp_options_get(options, key);
	return(value ? strtoul(value, NULL, 10) : fallback);
}

static mp_list_t __fsmc;

/* ADDSET and DATAST for a cycle of ns at hclk, about a third of it for the address */
static uint32_t _mp_fsmc_timing(unsigned long ns, unsigned long hclk) {
	unsigned long mhz = hclk / 1000000;
	unsigned long cycles = (ns * mhz + 999) / 1000;
	unsigned long addset = cycles / 3, datast;

	if(addset > 15)
		addset = 15;
	datast = cycles > addset ? cycles - addset : 1;
	if(datast > 255)
		datast = 255;
	if(datast < 1)
		datast = 1;
	return((uint32_t)addset | ((uint32_t)datast << 8));
}

static mp_bool_t _mp_fsmc_pin(mp_fsmc_t *fsmc, int slot, const char *pin, char *who) {
	fsmc->pins[slot] = mp_gpio_text_handle((char *)pin, who);
	if(!fsmc->pins[slot])
		return(FALSE);
	mp_gpio_mode(fsmc->pins[slot], MP_GPIO_MODE_AF);
	return(TRUE);
}

mp_ret_t mp_fsmc_open(mp_kernel_t *kernel, mp_fsmc_t *fsmc, mp_options_t *options, char *who) {
	int index, rs, slot = 0;

	memset(fsmc, 0, sizeof(*fsmc));

	fsmc->bank = _mp_fsmc_find(_mp_fsmc_ne, _MP_FSMC_COUNT(_mp_fsmc_ne), mp_options_get(options, "cs"));
	rs = _mp_fsmc_find(_mp_fsmc_address, _MP_FSMC_COUNT(_mp_fsmc_address), mp_options_get(options, "rs"));
	if(fsmc->bank < 0 || rs < 0) {
		mp_printk("FSMC %s: cs must be an NE pin and rs an address pin", who);
		return(FALSE);
	}

	fsmc->gate = mp_gate_handle(mp_options_get(options, "gate"), who);
	if(!fsmc->gate)
		return(FALSE);

	for(index = 0; index < _MP_FSMC_COUNT(_mp_fsmc_bus); index++)
		if(_mp_fsmc_pin(fsmc, slot++, _mp_fsmc_bus[index], who) == FALSE)
			goto failed;
	if(_mp_fsmc_pin(fsmc, slot++, _mp_fsmc_ne[fsmc->bank], who) == FALSE)
		goto failed;
	if(_mp_fsmc_pin(fsmc, slot++, _mp_fsmc_address[rs], who) == FALSE)
		goto failed;

	fsmc->readNs = _mp_fsmc_ns(options, "readCycle", 500);
	fsmc->writeNs = _mp_fsmc_ns(options, "writeCycle", 100);
	FSMC_Bank1->BTCR[fsmc->bank * 2] = FSMC_BCR_MWID_16 | FSMC_BCR_WREN | FSMC_BCR_EXTMOD;
	FSMC_Bank1->BTCR[fsmc->bank * 2 + 1] = _mp_fsmc_timing(fsmc->readNs, mp_clock_get_speed());
	FSMC_Bank1E->BWTR[fsmc->bank * 2] = _mp_fsmc_timing(fsmc->writeNs, mp_clock_get_speed());
	FSMC_Bank1->BTCR[fsmc->bank * 2] |= FSMC_BCR_MBKEN;

	fsmc->item.next = fsmc->item.prev = NULL;
	mp_list_add_last(&__fsmc, &fsmc->item, fsmc);
	fsmc->listed = YES;

	/* a 16-bit bank drives A[n] from HADDR[n + 1] */
	fsmc->cmd = (volatile uint16_t *)(FSMC_BANK1_MEMORY + 0x04000000UL * fsmc->bank);
	fsmc->data = (volatile uint16_t *)((uintptr_t)fsmc->cmd + (2UL << rs));

	mp_printk("FSMC NE%d for %s, RS on A%d", fsmc->bank + 1, who, rs);
	return(TRUE);

failed:
	mp_printk("FSMC %s: a pin is not available", who);
	mp_fsmc_close(fsmc);
	return(FALSE);
}

mp_ret_t mp_fsmc_close(mp_fsmc_t *fsmc) {
	int index;

	if(!fsmc->gate)
		return(FALSE);

	if(fsmc->cmd)
		FSMC_Bank1->BTCR[fsmc->bank * 2] &= ~FSMC_BCR_MBKEN;

	if(fsmc->listed == YES) {
		mp_list_remove(&__fsmc, &fsmc->item);
		fsmc->listed = NO;
	}

	for(index = 0; index < MP_FSMC_PINS; index++) {
		if(!fsmc->pins[index])
			continue;
		mp_gpio_mode(fsmc->pins[index], MP_GPIO_MODE_INPUT);
		mp_gpio_release(fsmc->pins[index]);
		fsmc->pins[index] = NULL;
	}

	mp_gate_release(fsmc->gate);
	fsmc->gate = NULL;
	fsmc->cmd = fsmc->data = NULL;
	return(TRUE);
}

/**
 * @brief Time every bank for hclk
 *
 * The clock calls it before a change with the faster of both clocks,
 * and after it with the new one: a bank timed for a faster clock is
 * only slower.
 *
 * @param[in] hclk AHB clock in Hz
 */
void mp_fsmc_retime(unsigned long hclk) {
	mp_list_item_t *item;
	mp_fsmc_t *fsmc;

	for(item = __fsmc.first; item; item = item->next) {
		fsmc = item->user;
		FSMC_Bank1->BTCR[fsmc->bank * 2 + 1] = _mp_fsmc_timing(fsmc->readNs, hclk);
		FSMC_Bank1E->BWTR[fsmc->bank * 2] = _mp_fsmc_timing(fsmc->writeNs, hclk);
	}
}

/**@}*/
