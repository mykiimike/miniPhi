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

Temperature tracking for the PLL cap, the transmit power and the I2C bias.

@{
*/

#define _REG_CLK 0x600A981CUL

#define _PAR_TEMP   0x00
#define _PAR_BASE   0x04
#define _PAR_FLAG   0x09
#define _PAR_PLL    0x0a
#define _PAR_BT     0x0b
#define _PAR_HOLD   0x17
#define _PAR_SPAN   0x48
#define _PAR_I2C    0x4d
#define _PAR_STATUS 0xa4
#define _PAR_CHAN   0x11c
#define _PAR_CHAN_B 0x11f
#define _PAR_STEP   0x122
#define _PAR_WIFI   0x123
#define _PAR_BTGAIN 0x124
#define _PAR_COPY   0x12e
#define _PAR_HOLD_T 0x130
#define _PAR_RXCAL  0x198
#define _PAR_SKIP   0x19a
#define _PAR_BUSY   0x19c
#define _PAR_OFF    0x19d
#define _PAR_SLOW   0x1a0
#define _PAR_MASK   0x1a5
#define _PAR_LIMIT  0x1a6

/**
 * Take the tracking token.
 * @returns The token passed back to _crit_exit
 */
static uint32_t _crit_enter(void) {
	return((uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_ENTER)());
}

/**
 * Hand the tracking token back.
 * @param token Value from _crit_enter
 */
static void _crit_exit(uint32_t token) {
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_EXIT)(token);
}

/**
 * Clear bit 1 of the clock register. The saved word is a separate read.
 * @param saved Previous word
 */
static void _clk_clear(unsigned long *saved) {
	*saved = mp_arch_rf_read(_REG_CLK);
	mp_arch_rf_write(_REG_CLK, mp_arch_rf_read(_REG_CLK) & ~2UL);
}

/**
 * Put bit 1 back from the saved word, using two fresh reads.
 * @param saved Word captured by _clk_clear
 */
static void _clk_restore(unsigned long saved) {
	unsigned long cur;
	unsigned long other;

	cur = mp_arch_rf_read(_REG_CLK);
	other = mp_arch_rf_read(_REG_CLK);
	mp_arch_rf_write(_REG_CLK, ((cur | saved) & 2UL) | (other & ~2UL));
}

/**
 * Same restore, with the saved word already shifted down by one.
 * @param shifted Saved word >> 1
 */
static void _clk_restore_shifted(unsigned long shifted) {
	unsigned long cur;
	unsigned long other;

	cur = mp_arch_rf_read(_REG_CLK) >> 1;
	other = mp_arch_rf_read(_REG_CLK);
	cur = ((cur | shifted) << 1) & 2UL;
	mp_arch_rf_write(_REG_CLK, cur | (other & ~2UL));
}

/**
 * Limit from the mask byte, or the default.
 * @param fallback Used when bit of the mask is clear
 * @param bit Mask bit
 * @returns The limit
 */
static int _limit(int fallback, unsigned bit) {
	if((phy_param[_PAR_MASK] & bit) != 0)
		return((int)phy_param[_PAR_LIMIT]);
	return(fallback);
}

/**
 * Signed temperature halfword.
 * @param off Byte offset
 * @returns The halfword
 */
static int _temp(unsigned off) {
	return((int)(int16_t)mp_arch_rf_u16(off));
}

/**
 * Track the PLL cap from the Bluetooth path.
 */
void bt_track_pll_cap(void) {
	phy_param_track_tot(0, 1);
}

/**
 * Track Bluetooth transmit power.
 * @param apply Non-zero applies a new step
 * @param log Non-zero prints the correction
 */
void bt_track_tx_power(int apply, int log) {
	txpwr_cal_track(1, apply, log);
}

/**
 * Track Wi-Fi transmit power.
 * @param apply Non-zero applies a new step
 * @param log Non-zero prints the correction
 */
void wifi_track_tx_power(int apply, int log) {
	txpwr_cal_track(0, apply, log);
}

/**
 * Run one full tracking pass and print when asked.
 * @param log Non-zero prints the PLL correction
 * @param flag Non-zero prints the power correction
 */
void phy_param_track(uint32_t log, uint32_t flag) {
	uint32_t token;

	token = _crit_enter();
	if(phy_param[_PAR_HOLD] == 0) {
		tsens_temp_read_new();
		if(phy_param[_PAR_PLL] != 0)
			rfpll_cap_track(phy_param[_PAR_FLAG]);
		wifi_track_tx_power((int)log, (int)flag);
		bt_track_tx_power((int)log, (int)flag);
	}
	_crit_exit(token);
}

/**
 * Track PLL, power and the I2C bias, then read the temperature.
 * @param wifi Non-zero tracks Wi-Fi power and the I2C bias
 * @param bt Non-zero tracks Bluetooth power
 */
void phy_param_track_tot(int wifi, int bt) {
	uint32_t token;

	token = _crit_enter();
	if(phy_param[_PAR_HOLD] == 0 && phy_param[_PAR_OFF] == 0) {
		if(phy_param[_PAR_PLL] != 0)
			phy_rfpll_cap_track_new(phy_param[_PAR_FLAG]);
		if(bt != 0)
			bt_track_tx_power(phy_param[_PAR_BT], phy_param[_PAR_FLAG]);
		if(wifi != 0) {
			tx_i2c_track();
			wifi_track_tx_power(1, phy_param[_PAR_FLAG]);
			if(phy_param[_PAR_SKIP] == 0)
				phy_cal_param_track(phy_param[_PAR_FLAG]);
		}
		tsens_temp_read_new();
	}
	_crit_exit(token);
}

/**
 * Recalibrate the receive or transmit path when the temperature has moved.
 * @param log Non-zero prints both temperatures
 */
void phy_cal_param_track(int log) {
	int gap;
	int chan_b;
	uint16_t chan;
	unsigned long saved;

	gap = abs_temp(_temp(_PAR_RXCAL) - _temp(_PAR_TEMP));
	if(_limit(30, 2) <= gap) {
		phy_dis_hw_set_freq_new();
		force_txrx_off(1);
		pbus_clear_reg();
		chan = mp_arch_rf_u16(_PAR_CHAN);
		chan_b = (int)(int8_t)phy_param[_PAR_CHAN_B];
		if(log != 0)
			phy_printf("cal:%d,%d\n", _temp(_PAR_RXCAL), _temp(_PAR_TEMP));
		_clk_clear(&saved);
		mp_arch_rf_set_u32(_PAR_STATUS, mp_arch_rf_u32(_PAR_STATUS) & 0xfffffd7fUL);
		set_rx_gain_table(0x985, 0);
		phy_chan_set(chan, chan_b);
		_clk_restore(saved);
		force_txrx_off(0);
		phy_en_hw_set_freq_new();
		mp_arch_rf_set_u16(_PAR_RXCAL, mp_arch_rf_u16(_PAR_TEMP));
	}
	gap = abs_temp(_temp(_PAR_SPAN) - _temp(_PAR_TEMP));
	if(_limit(40, 2) <= gap) {
		phy_dis_hw_set_freq_new();
		force_txrx_off(1);
		pbus_clear_reg();
		saved = mp_arch_rf_read(_REG_CLK) >> 1;
		chan = mp_arch_rf_u16(_PAR_CHAN);
		chan_b = (int)(int8_t)phy_param[_PAR_CHAN_B];
		mp_arch_rf_write(_REG_CLK, mp_arch_rf_read(_REG_CLK) & ~2UL);
		if(log != 0)
			phy_printf("cal:%d,%d\n", _temp(_PAR_SPAN), _temp(_PAR_TEMP));
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CBW40)(0);
		txiq_cal_init_new();
		txdc_cal_pwdet_init(0, 0);
		wifi_set_tx_gain(chan, 0);
		mp_arch_rf_set_u16(_PAR_SPAN, mp_arch_rf_u16(_PAR_TEMP));
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CBW40)(chan_b);
		_clk_restore_shifted(saved);
		force_txrx_off(0);
		phy_en_hw_set_freq_new();
	}
}

/**
 * Correct the PLL cap once the temperature has moved far enough.
 * @param log Non-zero prints the temperatures and the correction
 */
void phy_rfpll_cap_track_new(int log) {
	int gap;
	uint32_t corrected;

	gap = abs_temp(_temp(_PAR_TEMP) - _temp(_PAR_HOLD_T));
	if(_limit(15, 1) <= gap && phy_param[_PAR_BUSY] == 0) {
		phy_param[_PAR_BUSY] = 1;
		phy_dis_hw_set_freq_new();
		corrected = phy_rfpll_cap_correct_new(phy_param[_PAR_FLAG]);
		if(log != 0) {
			phy_printf("temp=%d,%d,delta=%d\n", _temp(_PAR_TEMP), _temp(_PAR_HOLD_T), corrected);
		}
		mp_arch_rf_set_u16(_PAR_HOLD_T, mp_arch_rf_u16(_PAR_TEMP));
		phy_en_hw_set_freq_new();
		phy_param[_PAR_BUSY] = 0;
	}
}

/**
 * Remember whether power tracking should use the slow limit.
 * @param slow Non-zero selects the limit of 10
 */
void phy_txpwr_track_slow(uint8_t slow) {
	phy_param[_PAR_SLOW] = slow;
}

/**
 * Move the SAR bias fields with the temperature.
 * Below 1 degree the cold set is written, above 54 the hot set, and a
 * halfword one below 54 writes the middle set.
 */
void tx_i2c_track(void) {
	int temp;
	uint32_t field;
	uint16_t chan;

	temp = _temp(_PAR_TEMP);
	if(temp <= 0 && phy_param[_PAR_I2C] != 1) {
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 3, 3, 0, 10);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 3, 7, 4, 3);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 7, 3, 0, 5);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 4, 7, 4, 6);
		phy_param[_PAR_I2C] = 1;
	}
	if(temp > 54 && phy_param[_PAR_I2C] != 2) {
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 3, 3, 0, 0xb);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 3, 7, 4, 4);
		chan = mp_arch_rf_u16(_PAR_CHAN);
		field = 5;
		if(chan <= 12)
			field = 7;
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 7, 3, 0, field);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 4, 7, 4, 10);
		phy_param[_PAR_I2C] = 2;
	}
	temp = (int)mp_arch_rf_u16(_PAR_TEMP);
	temp = (int)(uint16_t)(temp - 1);
	if(temp <= 53 && phy_param[_PAR_I2C] != 0) {
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 3, 3, 0, 0xb);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 3, 7, 4, 3);
		chan = mp_arch_rf_u16(_PAR_CHAN);
		field = 5;
		if(chan <= 12)
			field = 8;
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 7, 3, 0, field);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 4, 7, 4, 10);
		phy_param[_PAR_I2C] = 0;
	}
}

/**
 * Track Wi-Fi power from the background task.
 */
void tx_pwctrl_background_(void) {
	phy_param_track_tot(1, 0);
}

/**
 * Update the power step when the temperature has moved.
 * @param bt Non-zero selects the Bluetooth limits and gain
 * @param apply Non-zero writes the step
 * @param log Non-zero prints the correction
 */
void txpwr_cal_track(int bt, int apply, int log) {
	int anchor;
	int gap;
	int limit;
	int step;
	int sat_hi;
	int8_t held;

	anchor = _temp(_PAR_COPY);
	gap = abs_temp(_temp(_PAR_TEMP) - anchor);
	limit = 4;
	if(gap < 8)
		limit = 2;
	if(phy_param[_PAR_SLOW] != 0)
		limit = 10;
	sat_hi = 0x69;
	if(bt == 0)
		sat_hi = 0x50;
	step = (int)(int16_t)get_data_sat(_temp(_PAR_TEMP), sat_hi, -60);
	gap = abs_temp(step - _temp(_PAR_BASE));
	if(gap < limit)
		step = (int)(int8_t)phy_param[_PAR_STEP];
	else
		step = ram_temp_to_power(step, anchor, bt);
	if(apply == 0)
		return;
	held = (int8_t)phy_param[_PAR_WIFI];
	if(bt != 0)
		held = (int8_t)phy_param[_PAR_BTGAIN];
	if(held == (int8_t)step)
		return;
	phy_bbpll_cal(1);
	phy_param[_PAR_STEP] = (uint8_t)step;
	mp_arch_rf_set_u16(_PAR_BASE, mp_arch_rf_u16(_PAR_TEMP));
	if(bt == 0) {
		phy_param[_PAR_WIFI] = phy_param[_PAR_STEP];
		wifi_set_tx_gain(mp_arch_rf_u16(_PAR_CHAN), 0);
	} else {
		phy_param[_PAR_BTGAIN] = phy_param[_PAR_STEP];
		bt_set_tx_gain(0);
	}
	if(log != 0) {
		phy_printf("correct_power=%d,%d,temp=%d %d\n",
			(int)(int8_t)phy_param[_PAR_BTGAIN],
			(int)(int8_t)phy_param[_PAR_WIFI],
			_temp(_PAR_TEMP), anchor);
	}
	phy_bbpll_cal(0);
}

/**
 * Replace one power byte when the span no longer covers the limit.
 * @param bt Non-zero selects the Bluetooth gain
 * @param step Byte to update
 * @param span Two signed bounds
 * @param limit Temperature limit
 * @param ref Reference passed to the power step
 */
void txpwr_correct(int bt, int8_t *step, int16_t *span, int limit, int ref) {
	int8_t previous;
	int sample;

	sample = (int)span[1];
	previous = *step;
	if(sample < limit || (sample = (int)span[0], limit < sample))
		*step = (int8_t)ram_temp_to_power(sample, ref, bt);
	else
		*step = (int8_t)phy_param[_PAR_STEP];
	if(*step == previous)
		return;
	if(bt != 0) {
		bt_set_tx_gain(0);
		return;
	}
	wifi_set_tx_gain(mp_arch_rf_u16(_PAR_CHAN), 0);
}

/**@}*/
