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

Hardware frequency memory: the I2C image, the PLL cap and DAC rows, and the busy wait.

@{
*/

#define _REG_CTRL  0x600A00C0UL
#define _REG_TRIG  0x600A00C4UL
#define _REG_DATA  0x600A00D0UL
#define _REG_LEN   0x600A00D4UL
#define _REG_WORD  0x600A00C8UL
#define _REG_STAT  0x600A00CCUL
#define _REG_READ  0x600A00E4UL
#define _REG_TICK  0x600AD000UL
#define _PAR_LOCK  0x19b
#define _PAR_STAT  0xa4
#define _PAR_MODE  0x51
#define _PAR_CHAN  0x11c
#define _MASK_HI   0x0ff00000UL
#define _ROWS      85
#define _WAIT_MAX  1000

/**
 * Poll the frequency engine, and reset it after 1000 ticks.
 * @returns 1 when the poll had to wait
 */
static uint32_t _busy(void) {
	uint32_t start;
	uint32_t spun;

	start = mp_arch_rf_read(_REG_TICK);
	spun = 0;
	for(;;) {
		if((mp_arch_rf_read(_REG_STAT) & 0x100UL) != 0)
			return(spun);
		spun = 1;
		if(mp_arch_rf_read(_REG_TICK) - start > _WAIT_MAX) {
			freq_module_resetn();
			phy_printf("phy wait timeout\n");
			return(spun);
		}
	}
}

/**
 * Write the channel field from a frequency in MHz.
 * @param freq Frequency
 * @param sign Non-zero sign-extends the offset
 */
static void _chan(int freq, int sign) {
	unsigned long field;
	unsigned long word;

	if(sign != 0)
		field = (unsigned long)(int)(int16_t)(freq - 0x960);
	else
		field = (uint16_t)(freq - 0x960);
	word = mp_arch_rf_read(_REG_CTRL);
	mp_arch_rf_write(_REG_CTRL, (word & 0xffffc00fUL) | ((field << 7) & 0x3ff0UL));
}

/**
 * Program one frequency-memory burst and pulse the write bit.
 * @param addr Row address
 * @param data Payload
 * @param bytes Burst length
 */
void freq_i2c_mem_write_new(uint32_t addr, uint32_t data, int bytes) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_CTRL);
	mp_arch_rf_write(_REG_CTRL, (word & 0xffffc00fUL) | (((unsigned long)addr << 4) & 0x3ff0UL));
	mp_arch_rf_write(_REG_DATA, ((unsigned long)bytes << 24) | data);
	mp_arch_rf_set(_REG_CTRL, 0x8000UL);
	mp_arch_rf_clr(_REG_CTRL, 0x8000UL);
}

/**
 * Read one frequency-memory row, pulsing the read bit around the result.
 * @param addr Row address
 * @param bytes Burst length
 * @returns The captured word
 */
uint32_t read_rf_freq_mem_new(uint32_t addr, uint32_t bytes) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_CTRL);
	mp_arch_rf_write(_REG_CTRL, (word & 0xffffc00fUL) | (((unsigned long)addr << 4) & 0x3ff0UL));
	word = mp_arch_rf_read(_REG_LEN);
	mp_arch_rf_write(_REG_LEN, (word & 0xfffffffcUL) | (bytes & 3UL));
	mp_arch_rf_set(_REG_TRIG, 1UL);
	mp_arch_rf_clr(_REG_TRIG, 1UL);
	return(mp_arch_rf_read(_REG_READ));
}

/**
 * Load the frequency-engine defaults. A set lock byte forces the second field to zero.
 * @param sel Field selector
 * @param mode Mode nibble
 */
void freq_reg_init(uint32_t sel, int mode) {
	unsigned long word;
	unsigned long field;

	word = mp_arch_rf_read(_REG_CTRL);
	mp_arch_rf_write(_REG_CTRL, word & 0x5bffbfffUL);
	mp_arch_rf_set(_REG_CTRL, 0x02000000UL);
	if(phy_param[_PAR_LOCK] != 0) {
		mode = 2;
		sel = 0;
	}
	field = (((unsigned long)mode << 4) | sel) << 17;
	word = mp_arch_rf_read(_REG_CTRL);
	mp_arch_rf_write(_REG_CTRL, (word & 0xfe01ffffUL) | (field & 0x1fe00000UL));
	word = mp_arch_rf_read(_REG_LEN);
	mp_arch_rf_write(_REG_LEN, (word & 0xfffff87fUL) | 0x300UL);
	mp_arch_rf_write(_REG_WORD, 0x19800249UL);
	mp_arch_rf_write(_REG_STAT, 0x25824e50UL);
}

/** Fill the analog image that the frequency writer consumes. */
void freq_get_i2c_data(uint8_t *host, uint8_t *block, int8_t *shift, int words, uint32_t count) {
	uint32_t pll;
	uint32_t sdm;
	uint32_t filt;
	uint32_t mixed;
	uint32_t wide;
	uint32_t narrow;
	int index;
	int hi;
	int mid;
	int lo;

	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xb, 6, 6, 1);
	pll = mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL, 1, 0xb);
	sdm = mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL_SDM, 1, 0);
	filt = mp_arch_rf_reg_read(MP_ARCH_RF_HOST_FILTER, 1, 3);
	wide = sdm & 0xf7;
	narrow = sdm & 0xff | 8;
	mixed = ((filt & 0xfb) << 8) | (((filt | 4) & 0xff) << 16) | ((filt | 4) & 0xff);
	hi = 0x20;
	mid = 0x10;
	lo = 0;
	for(index = 0; (uint32_t)(index & 0xff) < count; index++) {
		switch(index & 0xff) {
		case 0:
			host[0] = MP_ARCH_RF_HOST_RFPLL;
			block[0] = 1;
			shift[0] = (int8_t)hi;
			hi = (hi + 1) & 0xff;
			break;
		case 1:
			host[1] = MP_ARCH_RF_HOST_RFPLL;
			block[1] = 2;
			shift[1] = (int8_t)hi;
			hi = (hi + 1) & 0xff;
			break;
		case 2:
			host[2] = MP_ARCH_RF_HOST_RFPLL_SDM;
			block[2] = 0;
			shift[2] = (int8_t)mid;
			*(uint32_t *)(words + 8) = wide;
			mid = (mid + 1) & 0xff;
			break;
		case 3:
			host[3] = MP_ARCH_RF_HOST_RFPLL_SDM;
			block[3] = 6;
			shift[3] = (int8_t)hi;
			hi = (hi + 1) & 0xff;
			break;
		case 4:
			host[4] = MP_ARCH_RF_HOST_RFPLL_SDM;
			block[4] = 5;
			shift[4] = (int8_t)hi;
			hi = (hi + 1) & 0xff;
			break;
		case 5:
			host[5] = MP_ARCH_RF_HOST_RFPLL_SDM;
			block[5] = 4;
			shift[5] = (int8_t)hi;
			hi = (hi + 1) & 0xff;
			break;
		case 6:
			host[6] = MP_ARCH_RF_HOST_RFPLL_SDM;
			block[6] = 3;
			shift[6] = (int8_t)hi;
			hi = (hi + 1) & 0xff;
			break;
		case 7:
			host[7] = MP_ARCH_RF_HOST_RFPLL_SDM;
			block[7] = 0;
			shift[7] = (int8_t)mid;
			*(uint32_t *)(words + 0x1c) = narrow;
			mid = (mid + 1) & 0xff;
			break;
		case 8:
			host[8] = MP_ARCH_RF_HOST_RFPLL;
			block[8] = 0xb;
			shift[8] = (int8_t)mid;
			*(uint32_t *)(words + 0x20) = pll;
			mid = (mid + 1) & 0xff;
			break;
		case 9:
			host[9] = MP_ARCH_RF_HOST_FILTER;
			block[9] = 3;
			shift[9] = (int8_t)lo;
			*(uint32_t *)(words + 0x24) = mixed;
			lo = (lo + 1) & 0xff;
			break;
		default:
			host[index] = 0;
			block[index] = 0;
			shift[index] = 0;
			*(uint32_t *)(words + index * 4) = 0;
			break;
		}
	}
}

/** Capture ten analog rows and write them into frequency memory. */
void freq_i2c_data_write_new(void) {
	uint8_t host[12];
	uint8_t block[12];
	int8_t shift[12];
	uint32_t word[14];

	freq_get_i2c_data(host, block, shift, (int)(uintptr_t)word, 10);
	freq_i2c_write_set_new(host, block, (const uint8_t *)shift, word, 10);
}

/**
 * Turn the captured rows into frequency-memory bursts.
 * @param host Host of each row
 * @param block Block of each row
 * @param shift Shift nibble of each row
 * @param word Payload of each row
 * @param count Number of rows
 */
void freq_i2c_write_set_new(const uint8_t *host, const uint8_t *block,
		const uint8_t *shift, const uint32_t *word, int count) {
	uint8_t mark[20];
	uint32_t packed;
	uint32_t data;
	uint32_t param;
	uint32_t low;
	uint32_t high;
	uint32_t nibble;
	uint32_t kind;
	uint32_t addr;
	int i;

	memset(mark, 0, sizeof mark);
	for(i = 0; i != count; i++) {
		packed = ((uint32_t)block[i] << 8) | host[i];
		nibble = shift[i] & 0xf;
		kind = shift[i] >> 4;
		data = ((word[i] << 16) & _MASK_HI) | packed;
		if(kind == 0)
			addr = nibble;
		else if(kind == 1)
			addr = nibble + 1;
		else
			addr = nibble + 4;
		mark[i] = (uint8_t)addr;
		param = get_freq_mem_param((int)kind);
		low = param & 0xff;
		if(kind == 0) {
			high = (param >> 8) & 0xff;
			addr = nibble * 3 + low;
			freq_i2c_mem_write_new(addr, data, 7);
			freq_i2c_mem_write_new((high + nibble) * 3 + low,
				((word[i] << 8) & _MASK_HI) | packed, 7);
			freq_i2c_mem_write_new(addr + high * 6, (word[i] & _MASK_HI) | packed, 7);
		} else if(kind == 1) {
			freq_i2c_mem_write_new(nibble * 3 + low, data, 7);
		} else {
			freq_i2c_mem_write_new((nibble << 1) + low, packed, 3);
		}
	}
	freq_i2c_num_addr(mark, count);
}

/**
 * Write the Bluetooth power curve into 85 frequency rows.
 * @param table Power table, the second byte is the reference
 */
void bt_txpwr_freq(int table) {
	uint32_t param;
	uint32_t high;
	uint32_t mid;
	uint32_t index;
	int delta;

	param = get_freq_mem_param(2);
	high = (param >> 16) & 0xff;
	mid = (param >> 8) & 0xff;
	for(index = 0; index < _ROWS; index++) {
		delta = (int)(int8_t)((uint32_t)*(const uint8_t *)(table + 1) -
			(uint32_t)bt_chan_pwr_interp(table, (int)index));
		freq_i2c_mem_write_new(
			get_freq_mem_addr(high, mid, index, 6), (uint32_t)delta, 1);
	}
}

/**
 * Build the frequency image unless the status bit is already set.
 * @param count Number of rows
 * @param mode Mode passed to the PLL
 */
void get_rf_freq_init_new(uint32_t count, uint32_t mode) {
	uint8_t byte[4];
	uint32_t word[2];
	uint32_t field;
	uint32_t index;
	int first;
	int step;
	int accum;
	int sample;

	if((mp_arch_rf_u32(_PAR_STAT) & 0x20) != 0)
		return;
	write_pll_cap(200);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 2, 7, 7, 0);
	set_rfpll_freq(phy_param[_PAR_MODE], 0x985, mode, byte);
	field = mp_arch_rf_reg_read_mask(MP_ARCH_RF_HOST_RFPLL, 1, 6, 3, 0);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 2, 3, 0, field);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 2, 7, 7, 1);
	set_rfpll_freq(phy_param[_PAR_MODE], 0x960, mode, byte);
	first = read_pll_cap();
	set_rfpll_freq(phy_param[_PAR_MODE], 0x9a0, mode, byte);
	step = (int)(int16_t)read_pll_cap() - (int)(int16_t)first;
	accum = 0;
	for(index = 0; index != count; index++) {
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_RFPLL)(index + 0x960, phy_param[_PAR_MODE], mode, byte);
		sample = (int)(int16_t)(accum / 64 + first);
		accum += step;
		sample = get_data_sat(sample, 0x1ff, 0);
		word[0] = ((uint32_t)byte[0] << 16) | ((uint32_t)sample & 0xff) |
			((unsigned)((((int)(int16_t)sample >> 8) << 4) | (int)field | -128) & 0xff) << 8;
		word[1] = ((uint32_t)byte[3] << 16) | ((uint32_t)byte[2] << 8) | byte[1];
		wr_rf_freq_mem_new(index & 0xff, word);
	}
	mp_arch_rf_set_u32(_PAR_STAT, mp_arch_rf_u32(_PAR_STAT) | 0x20);
}

/** Wait, then set the hardware-frequency hold bit. */
void phy_dis_hw_set_freq_1(void) {
	phy_wait_freq_set_busy();
	phy_dis_hw_set_freq_reg();
}

/** Hold the frequency engine, then wait and pause when the wait spun. */
void phy_dis_hw_set_freq_new(void) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_CTRL);
	mp_arch_rf_write(_REG_CTRL, (word & 0xe7ffffffUL) | 0x10000000UL);
	mp_arch_rf_delay_us(2);
	if(phy_wait_freq_set_busy_new() != 0)
		mp_arch_rf_delay_us(20);
}

/** Set the hardware-frequency hold bit and pause. */
void phy_dis_hw_set_freq_reg(void) {
	mp_arch_rf_set(_REG_CTRL, 0x04000000UL);
	mp_arch_rf_delay_us(2);
}

/** Clear the hardware-frequency hold bits. */
void phy_en_hw_set_freq_new(void) {
	mp_arch_rf_clr(_REG_CTRL, 0x18000000UL);
}

/**
 * Wait until the frequency engine is idle.
 * @returns 1 when the poll had to wait
 */
uint32_t phy_wait_freq_set_busy(void) {
	return(_busy());
}

/**
 * Wait until the frequency engine is idle.
 * @returns 1 when the poll had to wait
 */
uint32_t phy_wait_freq_set_busy_new(void) {
	return(_busy());
}

/**
 * Add one delta to every capacitor row, then point the engine at the channel.
 * @param delta Signed capacitor step
 */
void pll_cap_mem_update_new(int delta) {
	uint32_t param;
	uint32_t high;
	uint32_t mid;
	uint32_t addr;
	uint32_t word;
	uint32_t sum;
	uint32_t index;

	param = get_freq_mem_param(2);
	high = (param >> 16) & 0xff;
	mid = (param >> 8) & 0xff;
	for(index = 0; index < _ROWS; index++) {
		addr = get_freq_mem_addr(high, mid, index & 0xff, 0);
		word = read_rf_freq_mem_new(addr, 2);
		sum = (uint16_t)((((word >> 4) & 0x100) | (word & 0xff)) + (uint32_t)delta);
		freq_i2c_mem_write_new(addr,
			(sum & 0xff) | (word & 0xef00) | ((unsigned)((int)(int16_t)sum >> 8) << 12), 3);
	}
	_chan(chan_to_freq(mp_arch_rf_u16(_PAR_CHAN)), 1);
}

/**
 * Write one DAC value into every row, then point the engine at the channel.
 * @param value DAC word
 */
void pll_dac_mem_update_new(uint32_t value) {
	uint32_t param;
	uint32_t high;
	uint32_t mid;
	uint32_t index;

	param = get_freq_mem_param(2);
	high = (param >> 16) & 0xff;
	mid = (param >> 8) & 0xff;
	for(index = 0; index < _ROWS; index++) {
		freq_i2c_mem_write_new(
			get_freq_mem_addr(high, mid, index & 0xff, 1), value, 1);
	}
	_chan(chan_to_freq(mp_arch_rf_u16(_PAR_CHAN)), 0);
}

/** Enable the software channel path and wait until the engine is idle. */
void ram_set_chan_freq_sw_start(void) {
	freq_chan_en_sw();
	mp_arch_rf_delay_us(10);
	while((mp_arch_rf_read(_REG_STAT) & 0x100UL) == 0)
		;
}

/**
 * Initialise the engine, build 85 rows, and write the analog image.
 * @param sel Field selector
 * @param mode Mode nibble
 */
void set_chan_freq_hw_init(uint32_t sel, int mode) {
	freq_reg_init(sel, mode);
	get_rf_freq_init_new(0x55, 0);
	freq_i2c_data_write_new();
}

/**
 * Write both words of one frequency row.
 * @param index Row number
 * @param word Two payload words
 */
void wr_rf_freq_mem_new(uint32_t index, const uint32_t *word) {
	uint32_t param;
	uint32_t high;
	uint32_t mid;

	param = get_freq_mem_param(2);
	high = (param >> 16) & 0xff;
	mid = (param >> 8) & 0xff;
	freq_i2c_mem_write_new(get_freq_mem_addr(high, mid, index, 0), word[0], 7);
	freq_i2c_mem_write_new(get_freq_mem_addr(high, mid, index, 3), word[1], 7);
}

/**@}*/
