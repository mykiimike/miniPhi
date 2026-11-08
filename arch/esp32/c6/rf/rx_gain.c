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

Receive-gain tables, the IQ calibration that fills them, and the temperature track.

@{
*/

#define _REG_FE    0x600A0910UL
#define _REG_IQ    0x600A0450UL
#define _REG_AGC   0x600A702CUL
#define _REG_WIN   0x600A713CUL
#define _REG_CLK   0x600A981CUL
#define _PAR_STAT  0xa4
#define _PAR_TEMP  0x198
#define _PAR_CHAN  0x11c
#define _PAR_MODE  0x11f
#define _PAR_FLAG  0x10
#define _PAR_HI    0x121
#define _PAR_LO    0x120
#define _PAR_DC    0xd4
#define _PAR_PAIR  0x14e
#define _PAR_ALT   0x16e
#define _PAR_WIDE  0x172
#define _PAR_NAR   0x132
#define _PAR_RX    0x02
#define _ROWS      85
#define _CAP       79

static const uint16_t _bb_gain[7] = {
	0x0000, 0x0020, 0x0030, 0x0038, 0x003c, 0x003e, 0x003f
};
static const uint16_t _code8[8] = {
	0x0040, 0x0041, 0x0043, 0x006b, 0x0077, 0x0079, 0x007b, 0x007f
};
static const uint16_t _code7[7] = {
	0x0041, 0x0043, 0x006b, 0x0077, 0x0079, 0x007b, 0x007f
};
static const uint8_t _synth_step[8] = {
	0x0b, 0x0a, 0x06, 0x06, 0x06, 0x06, 0x08, 0x00
};
static const int8_t _synth_curve[8] = { 3, 8, 8, 8, 8, 8, 8, 8 };
static const uint8_t _synth_code[8] = {
	0x40, 0x41, 0x43, 0x6b, 0x77, 0x79, 0x7b, 0x7f
};
static const int8_t _hi_step[8] = { 0x0b, 0x0a, 0x06, 0x06, 0x06, 0x06, 0x08, 0x00 };
static const uint8_t _hi_curve[8] = { 0x03, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08 };
static const int8_t _lo_step[8] = { 0x0b, 0x0e, 0x06, 0x05, 0x06, 0x08, 0x00, 0x00 };
static const uint8_t _lo_curve[8] = { 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00 };

/**
 * Build one gain table and return how many rows were stored, at most 85.
 * @param out Row buffer
 * @param limit Highest gain that is still stored
 * @param codes Code halfwords, one per step
 * @param steps Signed step bytes
 * @param curve Gain bytes
 * @param count Number of steps
 * @param verbose Non-zero prints each row
 * @returns Row count, or 85 when the buffer fills
 */
uint32_t gen_rx_gain_table_new(uint32_t *out, uint32_t limit, const uint16_t *codes,
		const int8_t *steps, const uint8_t *curve, int count, int verbose) {
	uint32_t gain;
	uint32_t index;
	uint32_t walked;
	uint32_t word;
	uint32_t part;
	int sum;

	gain = curve[0];
	index = 0;
	walked = 0;
	for(;;) {
		sum = (int)steps[walked] + (int)(int8_t)curve[walked];
		if((int)gain == sum && (int)walked < count - 1) {
			do {
				walked = (walked + 1) & 0xff;
				if(steps[walked] != 0)
					break;
			} while((int)walked < count - 1);
			gain = curve[walked];
		}
		part = (uint32_t)_bb_gain[gain / 6];
		word = (uint32_t)codes[walked] * 0x1000 + ((part << 4) & 0xffff) + gain % 6;
		out[index] = word;
		if(verbose != 0) {
			phy_printf("index: %d value: 0x%x 0x%x 0x%x %d %d %d %d\n",
				index, word, (uint32_t)codes[walked], (part << 4) & 0xffff,
				gain % 6, count, walked, (int)steps[walked]);
		}
		if(limit < gain) {
			if(verbose != 0)
				phy_printf("max_gain: %d\n", (int)(int8_t)index);
			return(index & 0xff);
		}
		index++;
		gain = (gain + 1) & 0xff;
		if(index == _ROWS)
			return(_ROWS);
	}
}

/**
 * Interpolate one DC sample pair.
 * @param samples Four halfwords per slot
 * @param slot Slot number, limited to 2
 * @param scale Scale applied to the difference
 * @returns Packed low and high halves
 */
uint32_t get_rxbb_dc(const uint16_t *samples, uint32_t slot, int scale) {
	const uint16_t *row;
	int i;
	int q;
	uint32_t lo;
	uint32_t hi;

	if(slot > 2)
		slot = 2;
	else
		slot = slot & 0xff;
	row = samples + slot * 4;
	i = (int)(int16_t)(row[2] - row[0]) * scale;
	q = (int)(int16_t)(row[3] - row[1]) * scale;
	lo = (uint32_t)((i + 2) / 4 + row[0]) & 0xffff;
	hi = (uint32_t)((q + 2) / 4 + row[1]);
	return((hi << 16) | lo);
}

/**
 * Interpolate y at one x, or return the nearer endpoint.
 * @param y Two output samples
 * @param x Two input samples
 * @param at Position
 * @returns Interpolated halfword
 */
uint32_t phy_get_interp_data(const uint16_t *y, const uint16_t *x, uint32_t at) {
	uint32_t left;
	uint32_t right;
	int span;
	int delta;

	left = x[0];
	if(left >= at)
		return(y[0]);
	right = y[1];
	if(at >= x[1])
		return(right);
	span = (int)x[1] - (int)left;
	delta = (int)right - (int)y[0];
	if(span == 0)
		delta = -1;
	else
		delta = (delta * (int)(at - left)) / span;
	return((uint32_t)(delta + (int)y[0]) & 0xffff);
}

/** Fill the gain memory with the idle row, then enable the AGC. */
void phy_rx_table_init(void) {
	uint32_t row;

	mp_arch_rf_set_u16(0x120, 0x4f4f);
	for(row = 0; row < _CAP; row++)
		write_gain_mem_new(0x40200000, 0x10080, 0, row & 0xff);
	phy_reg_init();
	enable_agc();
}

/**
 * Measure the receive IQ correction unless it is already stored.
 * @param tone Non-zero mixes the measured tone bit
 * @param unused Unused
 * @param keep Non-zero leaves the measurement path enabled
 */
void rxiq_cal_init(int tone, uint32_t unused, int keep) {
	uint32_t status;
	uint16_t sample;
	uint32_t mixed;
	int i;

	(void)unused;
	status = mp_arch_rf_u32(_PAR_STAT);
	if((status & 0x400) != 0)
		return;
	phy_chan_ana(6);
	MP_ARCH_RF_FN(0x64)(phy_param + _PAR_DC + 8, 6);
	mp_arch_rf_set(_REG_FE, 0x4000UL);
	mp_arch_rf_set(_REG_FE, 0x8000UL);
	pbus_debugmode();
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
	MP_ARCH_RF_FN(0x0c)(1);
	if(tone != 0) {
		mixed = (uint32_t)MP_ARCH_RF_FN(0x78)(1, 1);
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS)(1, 1, (mixed | 2) & 0xffff);
	}
	mp_arch_rf_set(_REG_IQ, 0x20000000UL);
	mp_arch_rf_set(_REG_IQ, 0x2000UL);
	mp_arch_rf_clr(_REG_IQ, 0x40000000UL);
	mp_arch_rf_clr(_REG_IQ, 0x4000UL);
	set_rx_gain_cal_iq_new(0, 0x80, (uint16_t *)(phy_param + _PAR_DC), 0);
	for(i = 0; i < 4; i++) {
		sample = mp_arch_rf_u16(_PAR_DC + (unsigned)i * 2);
		mp_arch_rf_set_u16(_PAR_DC + (unsigned)i * 2,
			(uint16_t)(((sample >> 1) & 0x1f80) | (sample & 0x7f)));
	}
	if(keep == 0) {
		mp_arch_rf_set(_REG_IQ, 0x40000000UL);
		mp_arch_rf_set(_REG_IQ, 0x4000UL);
		mp_arch_rf_clr(_REG_IQ, 0x20000000UL);
		mp_arch_rf_clr(_REG_FE, 0x8000UL);
		MP_ARCH_RF_FN(0x0c)(0);
		MP_ARCH_RF_FN(0x88)();
		pbus_workmode();
	}
	mp_arch_rf_set_u32(_PAR_STAT, mp_arch_rf_u32(_PAR_STAT) | 0x400);
}

/**
 * Rebuild the receive table when the temperature has moved by more than 40.
 * @param verbose Non-zero prints the two temperatures
 */
void rx_table_track(int verbose) {
	int delta;
	uint16_t channel;
	int mode;

	delta = abs_temp((int)(int16_t)mp_arch_rf_u16(_PAR_TEMP) - (int)(int16_t)mp_arch_rf_u16(0));
	if(delta <= 40)
		return;
	force_txrx_off(1);
	channel = mp_arch_rf_u16(_PAR_CHAN);
	mode = (int)(int8_t)phy_param[_PAR_MODE];
	if(verbose != 0) {
		phy_printf("rx:%d,%d\n",
			(int)(int16_t)mp_arch_rf_u16(_PAR_TEMP),
			(int)(int16_t)mp_arch_rf_u16(0));
	}
	mp_arch_rf_clr(_REG_CLK, 2UL);
	mp_arch_rf_set_u32(_PAR_STAT, mp_arch_rf_u32(_PAR_STAT) & 0xfffffdffUL);
	set_rx_gain_table(0x985, 0);
	mac_enable_bb();
	mp_arch_rf_set_u16(_PAR_TEMP, mp_arch_rf_u16(0));
	phy_chan_set(channel & 0xff, mode);
	force_txrx_off(0);
}

/**
 * Build both receive-gain banks unless the status bit is already set.
 * @param tag Unused
 * @param unused Unused
 */
void set_rx_gain_table(uint32_t tag, uint32_t unused) {
	uint32_t rows[85];
	uint32_t status;
	uint32_t count;
	int verbose;
	uint32_t gain;
	unsigned long word;

	(void)tag;
	(void)unused;
	status = mp_arch_rf_u32(_PAR_STAT);
	if((status & 0x200) == 0) {
		if((status & 0x80) == 0) {
			set_rx_gain_cal_dc_new(0, 1,
				(uint16_t *)(phy_param + _PAR_NAR),
				(uint16_t *)(phy_param + _PAR_NAR),
				(uint16_t *)(phy_param + _PAR_NAR));
			set_rx_gain_cal_dc_new(0, 0,
				(uint16_t *)(phy_param + _PAR_PAIR),
				(uint16_t *)(phy_param + _PAR_ALT),
				(uint16_t *)(phy_param + _PAR_WIDE));
			set_rx_gain_cal_dc_new(1, 0,
				(uint16_t *)(phy_param + _PAR_PAIR),
				(uint16_t *)(phy_param + _PAR_ALT),
				(uint16_t *)(phy_param + _PAR_WIDE));
			mp_arch_rf_set_u32(_PAR_STAT, mp_arch_rf_u32(_PAR_STAT) | 0x80);
		}
		verbose = (mp_arch_rf_u16(_PAR_FLAG) >> 8) & 1;
		count = gen_rx_gain_table_new(rows, 0x21, _code8, _hi_step, _hi_curve, 8, verbose);
		phy_param[_PAR_HI] = count < 0x50 ? (uint8_t)count : 0x4f;
		wr_rx_gain_mem(0, (uint32_t)(phy_param[_PAR_HI] + 1) & 0xff, rows,
			(const uint16_t *)(phy_param + _PAR_PAIR),
			(const uint16_t *)(phy_param + _PAR_ALT),
			(const uint16_t *)(phy_param + _PAR_WIDE), 0xe, 0);
		count = gen_rx_gain_table_new(rows, 0x1c, _code7, _lo_step, _lo_curve, 7, verbose);
		phy_param[_PAR_LO] = count < 0x50 ? (uint8_t)count : 0x4f;
		wr_rx_gain_mem(1, (uint32_t)(phy_param[_PAR_LO] + 1) & 0xff, rows,
			(const uint16_t *)(phy_param + _PAR_NAR),
			(const uint16_t *)(phy_param + _PAR_ALT),
			(const uint16_t *)(phy_param + _PAR_NAR), 0xe, 0);
		mp_arch_rf_set_u32(_PAR_STAT, mp_arch_rf_u32(_PAR_STAT) | 0x200);
		mp_arch_rf_set_u16(_PAR_TEMP, mp_arch_rf_u16(0));
	}
	word = mp_arch_rf_read(_REG_AGC);
	mp_arch_rf_write(_REG_AGC,
		(word & 0xffff80ffUL) | (((unsigned long)phy_param[_PAR_HI] << 8) & 0x7f00UL));
	gain = phy_param[_PAR_HI];
	if(gain > 0x4c)
		gain = 0x4c;
	word = mp_arch_rf_read(_REG_WIN);
	mp_arch_rf_write(_REG_WIN, (word & 0xfe03ffffUL) | ((unsigned long)(gain & 0xff) << 18));
	bt_gain_offset(0x50);
	iq_corr_enable();
}

/**
 * Write one receive-gain bank.
 * @param bank Zero selects the low bank and the DC pair
 * @param count Number of rows, compared as an unsigned limit
 * @param rows Prebuilt rows, used when synth is zero
 * @param dc DC halfwords
 * @param pair First alternate pair
 * @param alt Second alternate pair
 * @param span Width used to pick the alternate pair
 * @param synth Non-zero builds the rows from the fixed curve
 */
void wr_rx_gain_mem(int bank, uint32_t count, const uint32_t *rows,
		const uint16_t *dc, const uint16_t *pair, const uint16_t *alt,
		int span, int synth) {
	uint32_t index;
	uint32_t band;
	uint32_t gain;
	uint32_t value;
	uint32_t which;
	uint32_t base;
	uint16_t first;
	uint16_t second;
	uint32_t high;
	uint32_t low;
	uint32_t corr;
	unsigned long word0;
	unsigned long word1;
	unsigned long word2;
	int sum;
	int i;

	base = bank == 0 ? 0 : 0x50;
	pbus_debugmode();
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
	set_rxclk_en(1);
	set_txclk_en(1);
	sum = 0;
	for(i = 0; i < 7; i++)
		sum = (sum + _synth_step[i]) & 0xffff;
	if(synth == 0) {
		index = 0;
		band = 0;
	} else {
		index = (uint32_t)sum & 0xff;
		band = 7;
	}
	gain = (uint32_t)(uint16_t)_synth_curve[band];
	span = ((span - 1) & 0xfe) * 2;
	while(index < count) {
		if(synth == 0) {
			value = rows[index];
		} else {
			if((int)gain >= (int)_synth_step[band] + (int)_synth_curve[band] && band != 7) {
				band = (band + 1) & 0xffff;
				gain = (uint32_t)(uint16_t)_synth_curve[band];
			}
			value = ((uint32_t)_bb_gain[gain / 6] << 4) + gain % 6;
			value = (value & 0xffff) | ((uint32_t)_synth_code[band] << 12);
			gain = (gain + 1) & 0xffff;
		}
		which = rfrx_gain_index_new(bank, (value >> 12) & 0xffff);
		if(synth == 0) {
			first = dc[(which & 0x7f) * 2];
			second = dc[(which & 0x7f) * 2 + 1];
		} else {
			first = *(const uint16_t *)((const uint8_t *)alt + span);
			second = *(const uint16_t *)((const uint8_t *)alt + span + 2);
		}
		if(bank == 0) {
			high = pair[0];
			low = pair[1];
			corr = mp_arch_rf_u16(_PAR_DC) & 0x1fff;
		} else {
			low = 0x100;
			high = 0x100;
			corr = 0;
		}
		word0 = ((unsigned long)high << 31) | ((unsigned long)low << 13) |
			((unsigned long)second << 22) | corr;
		word1 = ((unsigned long)high >> 1) |
			((unsigned long)phy_param[_PAR_RX] << 29) |
			((value & 7) << 17) |
			(((value >> 4) & 0x7f) << 20) |
			((unsigned long)first << 8);
		word2 = ((value >> 10) & 0xe0) | (((value >> 12) & 7) << 2) |
			(unsigned long)(phy_param[_PAR_RX] >> 6);
		write_gain_mem_new((uint32_t)word0, (uint32_t)word1, (uint32_t)word2,
			(index + base) & 0xff);
		index = (index + 1) & 0xff;
	}
	set_rxclk_en(0);
	set_txclk_en(0);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
	pbus_workmode();
}

/**@}*/
