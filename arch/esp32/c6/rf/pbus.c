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
#include "rf_priv.h"

/**
@addtogroup mpArchESP32RF

PBUS memory tables, address decode and the transmit and receive power-up sequences.

@{
*/

#define _pbus(block, half, value) \
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS)((block), (half), (value))

#define _REG_CTRL  0x600A090CUL
#define _REG_MODE  0x600A0904UL
#define _REG_AGC   0x600A702CUL
#define _REG_CLK   0x600A981CUL
#define _REG_MEM   0x600A08C8UL
#define _REG_WORD  0x600A08CCUL
#define _REG_SAVE  0x600A08E0UL
#define _PAR_RX    0x02
#define _PAR_TXBB  0x14
#define _PAR_CAL   0x12
#define _PAR_DCO   0xa8
#define _PAR_SAVE  0x30

static const uint32_t _dco[2] = { 0x00010001, 0x00010001 };

static const uint32_t _set_a[5] = {
	0x000401ff, 0x000801ff, 0x001401ff, 0x001801ff, 0x004801ff
};
static const uint32_t _set_b[8] = {
	0x000501ff, 0x0014f9ff, 0x004801ff, 0x004417ff,
	0x00f00000, 0x00f10000, 0x00f20000, 0x00f40000
};
static const uint32_t _set_c[5] = {
	0x000401ff, 0x004801ff, 0x001401ff, 0x001801ff, 0x004401ff
};
static const uint32_t _set_d[3] = {
	0x0014f9ff, 0x005781ff, 0x00f30000
};

/**
 * Write one PBUS row group and advance the packed field base.
 * @param words Source words
 * @param count Number of words
 * @param shift 0 or 16, selecting the half of the control register
 * @param reg Control register
 * @param base Running field index, advanced by count
 */
static void _rows(const uint32_t *words, int count, int shift, unsigned long reg, uint32_t *base) {
	uint32_t packed;
	uint32_t cursor;
	unsigned long word;
	int i;

	packed = ((*base + (uint32_t)count - 1) << 8) | *base;
	word = mp_arch_rf_read(reg);
	word &= ~((uint32_t)0xffff << shift);
	word |= (unsigned long)(packed & 0xffff) << shift;
	mp_arch_rf_write(reg, word);
	cursor = (*base + 0x200) * 0x800;
	for(i = 0; i < count; i++) {
		mp_arch_rf_write(_REG_WORD, words[i]);
		word = mp_arch_rf_read(_REG_MEM);
		mp_arch_rf_write(_REG_MEM, (word & 0xffe007ffUL) | (cursor & 0x1ff800));
		cursor += 0x800;
	}
	*base += (uint32_t)count;
}

/** Clear the PBUS rows used by calibration. */
void pbus_clear_reg(void) {
	pbus_debugmode();
	_pbus(4, 1, 0);
	_pbus(4, 2, 0);
	_pbus(5, 1, 0);
	_pbus(5, 2, 0);
	_pbus(0, 1, 0);
	_pbus(0, 2, 0);
	_pbus(1, 1, 0);
	_pbus(1, 2, 0);
	_pbus(2, 1, 0x100);
	_pbus(3, 1, 0x100);
	_pbus(2, 2, 0x100);
	_pbus(3, 2, 0x100);
	pbus_workmode();
}

/**
 * Force the PBUS into or out of manual mode.
 * @param on Non-zero selects manual mode
 */
void ram_pbus_force_mode(int on) {
	unsigned long word;

	if(on != 0) {
		mp_arch_rf_clr(_REG_CTRL, 0x08000000UL);
		mp_arch_rf_set(_REG_MODE, 1UL);
		return;
	}
	mp_arch_rf_clr(_REG_MODE, 1UL);
	mp_arch_rf_set(_REG_CTRL, 0x08000000UL);
	if((mp_arch_rf_read(_REG_CLK) & 2UL) == 0)
		return;
	mp_arch_rf_delay_us(1);
	word = mp_arch_rf_read(_REG_AGC);
	mp_arch_rf_write(_REG_AGC, (word & 0x00ffffffUL) | 0x32000000UL);
	mp_arch_rf_set(_REG_AGC, 0x80000UL);
	mp_arch_rf_delay_us(2);
	mp_arch_rf_clr(_REG_AGC, 0x800000UL);
}

/**
 * Address of one PBUS read register.
 * @param block Block number
 * @param half Non-zero selects the second half
 * @returns Register address
 */
uint32_t ram_pbus_rd_addr(uint32_t block, int half) {
	if(block == 1)
		return(0x600a0914);
	if(block == 2)
		return(half == 1 ? 0x600a0918 : 0x600a091c);
	if(block == 3)
		return(0x600a091c);
	if(block == 0 || (block == 4 && half == 1))
		return(0x600a0920);
	return(0x600a0924);
}

/**
 * Bit shift of one PBUS read field.
 * @param block Block number
 * @param half Non-zero selects the second half
 * @returns Shift, or 0 when the block has no field
 */
uint32_t ram_pbus_rd_shift(uint32_t block, int half) {
	if(block > 5)
		return(0);
	if(block == 0)
		return(half == 1 ? 0x12 : 9);
	if(block == 2 || block == 4)
		return(half == 1 ? 0 : 0x12);
	if(half == 1)
		return(9);
	return(0);
}

/**
 * Power up the receive PBUS path.
 * @param gain Gain written to block 1
 * @param b Unused
 * @param c Unused
 * @param d Unused
 * @param e Unused
 */
void ram_pbus_xpd_rx_on(uint32_t gain, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
	(void)b;
	(void)c;
	(void)d;
	(void)e;
	_pbus(4, 1, 0);
	_pbus(4, 2, 1);
	_pbus(5, 1, 0);
	_pbus(0, 1, 0x40);
	_pbus(0, 2, phy_param[_PAR_RX]);
	_pbus(1, 1, 0x189);
	_pbus(1, 2, gain);
}

/**
 * Power up the transmit PBUS path.
 * @param gain Gain added to the last field
 * @param second Value written to block 1
 * @param c Unused
 * @param d Unused
 */
void ram_pbus_xpd_tx_on(short gain, uint32_t second, uint32_t c, uint32_t d) {
	uint32_t dco[2];

	(void)c;
	(void)d;
	dco[0] = _dco[0];
	dco[1] = _dco[1];
	_pbus(0, 1, 0x80);
	_pbus(0, 2, 0);
	_pbus(4, 2, 0);
	pbus_set_dco(dco);
	_pbus(1, 1, 0x7c);
	pbus_set_dco(dco);
	_pbus(1, 2, second);
	_pbus(4, 1, 0xb);
	_pbus(5, 1, 0x1c0);
	mp_arch_rf_delay_us(20);
	_pbus(5, 1, (uint32_t)(uint16_t)((int)gain + 0x1c0));
	mp_arch_rf_delay_us(20);
}

/**
 * Program the loopback gain.
 * @param gain Gain added to the block-5 field
 * @param first Value written to block 0
 * @param second Value written to block 1
 * @param d Unused
 * @param e Unused
 */
void ram_set_loopback_gain(short gain, uint32_t first, uint32_t second, uint32_t d, uint32_t e) {
	(void)d;
	(void)e;
	_pbus(4, 2, 1);
	_pbus(4, 1, 0x83);
	_pbus(5, 1, (uint32_t)(uint16_t)((int)gain + 0x1c0));
	_pbus(0, 1, first);
	_pbus(0, 2, phy_param[_PAR_RX]);
	_pbus(1, 1, 0x1f9);
	_pbus(1, 2, second);
}

/** Put the PBUS into the transmit-calibration debug mode. */
void ram_txcal_debuge_mode(void) {
	uint32_t index;

	pbus_debugmode();
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_TX)(phy_param[_PAR_CAL], 0);
	index = txbbgain_to_index(0);
	pbus_set_dco(phy_param + _PAR_DCO + index * 8);
	set_txclk_en(1);
	MP_ARCH_RF_FN(0x1c)();
}

/**
 * Reset the receive PBUS gain field.
 * @param wide Non-zero selects the wide code
 */
void rx_pbus_reset(int wide) {
	_pbus(1, 1, 0);
	_pbus(1, 1, wide == 0 ? 0x1c9 : 0x1cb);
	_pbus(1, 1, wide == 0 ? 0x189 : 0x18b);
}

/** Copy the six PBUS control registers into the parameter block. */
void save_pbus_reg(void) {
	int i;

	for(i = 0; i < 6; i++)
		mp_arch_rf_set_u32(_PAR_SAVE + (unsigned)i * 4,
			mp_arch_rf_read(_REG_SAVE + (unsigned)i * 4));
}

/** Build the PBUS tables and write them. */
void set_pbus_mem(void) {
	uint32_t head[16];
	uint32_t low[8];
	uint32_t high[8];
	uint32_t copy_a[5];
	uint32_t copy_c[5];
	uint32_t set_a[5];
	uint32_t set_c[5];
	uint32_t set_d[3];
	uint32_t edge[3];
	uint32_t one;
	uint32_t two;
	int i;

	head[0] = ((uint32_t)phy_param[_PAR_RX] << 9) | 0x801ff;
	head[1] = 0x4401ff;
	head[2] = 0x1713ff;
	head[3] = 0x4803ff;
	head[4] = 0x5401ff;
	head[5] = 0x487ff;
	head[6] = 0xf50000;
	head[7] = 0xf60000;
	memcpy(set_a, _set_a, sizeof set_a);
	memcpy(head + 8, _set_b, sizeof _set_b);
	memcpy(set_c, _set_c, sizeof set_c);
	memcpy(set_d, _set_d, sizeof _set_d);
	for(i = 0; i < 8; i++)
		low[i] = head[i];
	memcpy(copy_a, set_a, sizeof copy_a);
	for(i = 0; i < 8; i++)
		high[i] = head[8 + i];
	memcpy(copy_c, set_c, sizeof copy_c);
	one = 0x5401ff;
	two = 0x5401ff;
	low[2] = 0x1717ff;
	edge[0] = 0x14fdff;
	edge[1] = 0x5781ff;
	edge[2] = 0xf30000;
	high[1] = 0x14fdff;
	high[2] = ((uint32_t)phy_param[_PAR_TXBB] << 12) | 0x4801ff;
	write_pbus_mem(head, set_a, head + 8, set_c, set_d, &one,
		low, copy_a, high, copy_c, edge, &two);
	save_pbus_reg();
}

/**
 * Write the twelve PBUS groups.
 * @param a First group
 * @param b Second group
 * @param c Third group
 * @param d Fourth group
 * @param e Fifth group
 * @param f Sixth group
 * @param g Seventh group
 * @param h Eighth group
 * @param i Ninth group
 * @param j Tenth group
 * @param k Eleventh group
 * @param l Twelfth group
 */
void write_pbus_mem(const uint32_t *a, const uint32_t *b, const uint32_t *c,
		const uint32_t *d, const uint32_t *e, const uint32_t *f,
		const uint32_t *g, const uint32_t *h, const uint32_t *i,
		const uint32_t *j, const uint32_t *k, const uint32_t *l) {
	static const struct {
		const int count;
		const int shift;
		const unsigned long reg;
	} step[12] = {
		{ 8, 0,    0x600A08E0UL },
		{ 5, 0x10, 0x600A08E0UL },
		{ 8, 0,    0x600A08E4UL },
		{ 5, 0x10, 0x600A08E4UL },
		{ 3, 0,    0x600A08E8UL },
		{ 1, 0x10, 0x600A08E8UL },
		{ 8, 0,    0x600A08ECUL },
		{ 5, 0x10, 0x600A08ECUL },
		{ 8, 0,    0x600A08F0UL },
		{ 5, 0x10, 0x600A08F0UL },
		{ 3, 0,    0x600A08F4UL },
		{ 1, 0x10, 0x600A08F4UL }
	};
	const uint32_t *group[12];
	uint32_t base;
	int n;

	group[0] = a;
	group[1] = b;
	group[2] = c;
	group[3] = d;
	group[4] = e;
	group[5] = f;
	group[6] = g;
	group[7] = h;
	group[8] = i;
	group[9] = j;
	group[10] = k;
	group[11] = l;
	base = 0;
	for(n = 0; n < 12; n++)
		_rows(group[n], step[n].count, step[n].shift, step[n].reg, &base);
}

/**@}*/
