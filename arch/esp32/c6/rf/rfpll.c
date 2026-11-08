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

Channel programming and RF PLL frequency, charge-pump and capacitor calibration.

@{
*/

#define _PAR_HW      0x0a
#define _PAR_CAP     0x20
#define _PAR_FCC     0x51
#define _PAR_MIC     0x26
#define _PAR_11P_A   0x28
#define _PAR_11P_B   0x29
#define _PAR_TRIM    0xa1
#define _PAR_TRIM_B  0xa2
#define _PAR_CHAN    0x11c
#define _PAR_CHAN_F  0x11e
#define _PAR_CHAN_B  0x11f
#define _PAR_RX      0x121
#define _PAR_RX_A    0x14e
#define _PAR_RX_B    0x16e
#define _PAR_RX_C    0x172
#define _PAR_CHGP    0x196
#define _PAR_SCALE   0x1a7

#define _ROM_SET_CHANNEL 0x4000123CUL

extern long long __divdi3(long long, long long);

/**
 * Take the critical-section token from slot 0.
 * @returns Token passed back to the exit slot
 */
static uint32_t _crit_enter(void) {
	return((uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_ENTER)());
}

/**
 * Release the critical-section token through slot 4.
 * @param token Token returned by the enter slot
 */
static void _crit_exit(uint32_t token) {
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_EXIT)(token);
}

/**
 * Pack one SDM word into five bytes.
 * @param out Five-byte destination
 * @param word Packed word
 */
static void _sdm_store(uint8_t *out, uint32_t word) {
	out[4] = (uint8_t)((word >> 27) & 1);
	out[3] = (uint8_t)(word >> 19);
	out[2] = (uint8_t)(word >> 11);
	out[1] = (uint8_t)(word >> 3);
	out[0] = (uint8_t)(word & 7);
}

/**
 * Divisor selected by the PLL mode byte.
 * @param mode Mode 1, 2, or anything else
 * @returns 26, 32 or 40
 */
static int _mode_div(int mode) {
	if(mode == 1)
		return(26);
	if(mode == 2)
		return(32);
	return(40);
}

/**
 * Program one RF channel through the ROM entry.
 * @param freq Frequency word
 * @param mode Mode byte
 * @param cap Capacitor word
 */
void set_channel_rfpll_freq(uint32_t freq, uint32_t mode, uint32_t cap) {
	((mp_arch_rf_rom_t)_ROM_SET_CHANNEL)(freq, mode, cap);
}

/**
 * Wait until the PLL calibration bit is set, or 2 ms have passed.
 */
void wait_rfpll_cal_end(void) {
	unsigned tries;

	tries = 0;
	do {
		mp_arch_rf_delay_us(20);
		if(mp_arch_rf_reg_read_mask(MP_ARCH_RF_HOST_RFPLL, 1, 7, 1, 1) != 0)
			return;
		if(tries == 99)
			mp_arch_rf_printf("error: pll_cal exceeds 2ms!!!\n");
		tries = (tries + 1) & 0xff;
	} while(tries != 100);
}

/**
 * Store a frequency word and forward it to the offset routine.
 * @param freq Frequency word
 * @param offset Offset halfword
 */
void phy_set_freq(uint32_t freq, uint32_t offset) {
	mp_arch_rf_set_u16(_PAR_CAP, (uint16_t)offset);
	set_rf_freq_offset(phy_param[_PAR_FCC], freq, offset);
}

/**
 * Build the five SDM bytes for one frequency and offset.
 * @param freq Frequency
 * @param mode PLL mode
 * @param offset Signed offset
 * @param out Five-byte destination
 */
void phy_rfpll_set_freq_offset(int freq, int mode, int offset, uint8_t *out) {
	uint32_t word;

	word = (uint32_t)(freq << 20) / (uint32_t)(_mode_div(mode) * 3);
	word = word + (uint32_t)(offset * 9) + 0xff000000UL;
	_sdm_store(out, word);
}

/**
 * Build the SDM bytes from a 64-bit frequency ratio.
 * @param delta Signed addition before the last division
 * @param mode PLL mode
 * @param freq Frequency word
 * @param out Five-byte destination
 */
void ram_rfpll_set_freq(int delta, int mode, uint32_t freq, uint8_t *out) {
	long long quot;
	unsigned scale;
	uint32_t word;

	scale = 1;
	if(phy_param[_PAR_SCALE] != 0)
		scale = 10;
	quot = __divdi3((long long)(int)freq << 20, 1000);
	quot = __divdi3(quot, (long long)scale);
	quot += (unsigned long long)(uint32_t)(delta << 20);
	quot = __divdi3(quot, (long long)(_mode_div(mode) * 3));
	word = (uint32_t)quot + 0xff000000UL;
	_sdm_store(out, word);
}

/**
 * Enable the hardware frequency path, or force one SDM word.
 * @param hw Zero enables the hardware path
 * @param freq Frequency passed to the offset routine
 * @param offset Signed offset
 */
void phy_force_freq_set(int hw, uint32_t freq, short offset) {
	uint8_t sdm[8];

	if(hw == 0) {
		phy_en_hw_set_freq();
		phy_param[_PAR_HW] = 1;
		return;
	}
	phy_param[_PAR_HW] = 0;
	phy_dis_hw_set_freq_1();
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xb, 6, 6, 0);
	phy_rfpll_set_freq_offset((int)freq, (int)phy_param[_PAR_FCC], (int)offset, sdm);
	write_rfpll_sdm(sdm);
	restart_cal();
	wait_rfpll_cal_end();
}

/**
 * Search ten steps below the current capacitor, then ten above, and average the hits.
 * @returns The signed distance from the starting capacitor
 */
int phy_rfpll_cap_init_cal_new(void) {
	int origin;
	int chosen;
	int sum;
	int count;
	int phase;
	int step;
	int hits;
	int trial;
	int field;

	origin = read_pll_cap();
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xb, 6, 6, 1);
	sum = 0;
	count = 0;
	for(phase = 0; phase < 2; phase++) {
		hits = 0;
		for(step = 0; step < 10; step++) {
			if(phase == 0)
				trial = origin - step;
			else
				trial = origin + 1 + step;
			trial = (int)(uint16_t)trial;
			write_pll_cap((int)(int16_t)trial);
			mp_arch_rf_delay_us(5);
			field = (int)((mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL, 1, 0xc) >> 2) & 3);
			if(field == 0) {
				sum = (int)(uint16_t)(sum + trial);
				count = (count + 1) & 0xff;
			} else if((field == 1 && phase == 0) || (field == 2 && phase != 0)) {
				hits++;
				if(hits == 2)
					break;
				hits = 1;
			}
		}
	}
	chosen = origin;
	if(count != 0)
		chosen = sum / count;
	write_pll_cap((int)(int16_t)chosen);
	return((int)(int16_t)(chosen - origin));
}

/**
 * Calibrate the capacitor and record a non-zero result.
 * @param log Non-zero prints the result
 * @returns The signed correction
 */
int phy_rfpll_cap_correct_new(int log) {
	int result;

	result = phy_rfpll_cap_init_cal_new();
	if(result != 0)
		pll_cap_mem_update_new(result);
	if(log != 0)
		mp_arch_rf_printf("%d\n", result);
	return(result);
}

/**
 * Nudge the capacitor by the PLL status field.
 * @param log Non-zero prints the step and the capacitor pair
 * @param unused_a Ignored
 * @param unused_b Ignored
 * @param unused_c Ignored
 * @param unused_d Ignored
 * @returns 2, -2, or 0
 */
int rfpll_cap_correct_new(int log, uint32_t unused_a, uint32_t unused_b,
	uint32_t unused_c, uint32_t unused_d) {
	int field;
	int step;
	int raw;
	int written;

	(void)unused_a;
	(void)unused_b;
	(void)unused_c;
	(void)unused_d;
	field = (int)((mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL, 1, 0xc) >> 2) & 3);
	raw = 0;
	written = 0;
	if(field == 1)
		step = 2;
	else if(field == 2)
		step = -2;
	else
		step = 0;
	if(step != 0) {
		raw = read_pll_cap();
		written = (int)(int16_t)(raw + step);
		raw = (int)(int16_t)raw;
		write_pll_cap(written);
		pll_cap_mem_update_new(step);
	}
	if(log != 0)
		mp_arch_rf_printf("%d,%d,%d\n", step, raw, written);
	return(step);
}

/**
 * Measure the charge pump and write the scaled code back.
 */
void rfpll_chgp_cal(void) {
	unsigned tries;
	int field;
	unsigned code;

	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xf, 6, 6, 0);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xf, 5, 5, 0);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xf, 5, 5, 1);
	tries = 0;
	do {
		mp_arch_rf_delay_us(20);
		if(mp_arch_rf_reg_read_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xe, 7, 7) != 0)
			break;
		if(tries == 99)
			mp_arch_rf_printf("error: chgp_cal no done!!!\n");
		tries = (tries + 1) & 0xff;
	} while(tries != 100);
	field = mp_arch_rf_reg_read_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xe, 4, 0);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xf, 6, 6, 1);
	code = (unsigned)(((field * 7) / 6 + 9) & 0xff);
	if(code > 31)
		code = 31;
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_RFPLL, 1, 0xf, 4, 0, code);
	phy_param[_PAR_CHGP] = (uint8_t)mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL, 1, 0xf);
}

/**
 * Convert megahertz to a channel and program it.
 * @param mhz Frequency in megahertz
 * @param mode Mode forwarded to the channel routine
 * @returns 0
 */
uint32_t set_chanfreq(uint32_t mhz, uint32_t mode) {
	uint16_t channel;

	channel = (uint16_t)mhz2ieee(mhz);
	phy_chan_set(channel, (int)mode);
	return(0);
}

/**
 * Program one channel, including the filter, microphone and receive gain.
 * @param channel Channel number, or a frequency above 2000 MHz
 * @param mode Mode byte stored with the channel
 */
void phy_chan_set(uint32_t channel, int mode) {
	uint32_t token;
	int cap;
	uint32_t freq;

	token = _crit_enter();
	cap = (int)(int16_t)mp_arch_rf_u16(_PAR_CAP);
	if(channel > 2000) {
		channel = mhz2ieee(channel);
		channel &= 0xffff;
	}
	phy_param[_PAR_CHAN_B] = (uint8_t)mode;
	phy_param[_PAR_CHAN_F] = (uint8_t)(mode != 0);
	mp_arch_rf_set_u16(_PAR_CHAN, (uint16_t)channel);
	disable_agc();
	force_txrx_off(1);
	phy_bbpll_cal(1);
	freq = chan_to_freq(channel);
	set_freq_i2c(freq);
	set_channel_rfpll_freq(freq, phy_param[_PAR_FCC], (uint32_t)cap);
	phy_chan_set_misc(channel);
	phy_i2c_master_mem_txcap();
	if(phy_param[_PAR_MIC] != 0)
		chan14_mic_cfg(channel == 14);
	if(phy_param[_PAR_11P_A] != 0)
		phy_11p_set((int)phy_param[_PAR_11P_A], (int)phy_param[_PAR_11P_B]);
	set_rx_comp_new();
	phy_bbpll_cal(0);
	force_txrx_off(0);
	enable_agc();
	_crit_exit(token);
}

/**
 * Reload the transmit gain and one receive-gain row for the channel.
 * @param channel Channel number
 */
void phy_chan_set_misc(uint32_t channel) {
	uint8_t row[16];

	set_chan_reg(1);
	wifi_set_tx_gain(channel, 0);
	wr_rx_gain_mem(0, (phy_param[_PAR_RX] + 1) & 0xff, (const uint32_t *)row,
		(const uint16_t *)(phy_param + _PAR_RX_A),
		(const uint16_t *)(phy_param + _PAR_RX_B),
		(const uint16_t *)(phy_param + _PAR_RX_C), (int)channel, 1);
}

/**
 * Align a capacitor offset and reload the channel frequency.
 * @param offset Requested offset
 */
void phy_chan_set_offset(short offset) {
	uint32_t token;
	int cap;

	token = _crit_enter();
	cap = (int)(int16_t)((offset + 2) & ~3);
	mp_arch_rf_set_u16(_PAR_CAP, (uint16_t)cap);
	if(phy_param[_PAR_TRIM] != 0) {
		cap = cap + (int)(int8_t)phy_param[_PAR_TRIM_B] * 8;
		mp_arch_rf_set_u16(_PAR_CAP, (uint16_t)cap);
	}
	phy_freq_correct(1, (int)(int16_t)mp_arch_rf_u16(_PAR_CAP));
	disable_agc();
	set_channel_rfpll_freq(mp_arch_rf_u16(_PAR_CHAN), phy_param[_PAR_FCC],
		(uint32_t)(int)(int16_t)mp_arch_rf_u16(_PAR_CAP));
	enable_agc();
	_crit_exit(token);
}

/**@}*/
