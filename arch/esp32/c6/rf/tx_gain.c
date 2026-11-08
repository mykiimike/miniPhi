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

Transmit gain tables for Wi-Fi and Bluetooth, and the memory writer.

@{
*/

#define _REG_GAIN   0x600A0410UL

#define _PAR_TPW        0x06
#define _PAR_GAIN_EN    0x07
#define _PAR_STEP       0x08
#define _PAR_FCC        0x51
#define _PAR_LIMIT      0x52
#define _PAR_RATE       0x66
#define _PAR_FCC_A      0x67
#define _PAR_FCC_B      0x70
#define _PAR_MASK       0xd0
#define _PAR_BT_BASE    0xdc
#define _PAR_WIFI_SAMP  0xf1
#define _PAR_WIFI_REF   0xf7
#define _PAR_BT_SAMP    0xfb
#define _PAR_BT_REF     0xfe
#define _PAR_BT_QUAD    0x104
#define _PAR_WIFI_HOLD  0x123
#define _PAR_BT_HOLD    0x124
#define _PAR_PACK       0x18e

static const uint8_t _rate_chg[8] = { 6, 4, 2, 0, 7, 5, 3, 1 };

static const uint16_t _bt_tx_code[14] = {
	0x003f, 0x002f, 0x001f, 0x0017, 0x000f, 0x000d, 0x0007,
	0x0005, 0x0003, 0x0002, 0x0001, 0x0000, 0x0000, 0x0000
};
static const int16_t _bt_tx_pwr[14] = {
	27, 20, 9, 0, -12, -22, -35, -45, -56, -68, -81, -105, -105, -105
};
static const uint16_t _wifi_tx_code[16] = {
	0x003f, 0x003f, 0x002f, 0x0027, 0x001f, 0x0017, 0x000e, 0x000d,
	0x000c, 0x0007, 0x0005, 0x0003, 0x0002, 0x0001, 0x0000, 0x0000
};
static const int16_t _wifi_tx_pwr[16] = {
	0x22, 0x1a, 0x14, 0x0f, 9, 0, -16, -22, -28, -35, -45, -59, -69, -83, -99, -107
};

/**
 * Map a Wi-Fi 11g rate onto the gain-table column.
 * @param rate Rate code
 * @returns Column 0..7, or 0 when the rate is outside 8..15
 */
uint8_t wifi_11g_rate_chg_new(int rate) {
	unsigned index;

	index = (unsigned)(rate - 8) & 0xff;
	if(index < 8)
		return(_rate_chg[index]);
	return(0);
}

/**
 * Pick the FCC byte for one of the 18 rate slots.
 * @param word Four packed limits
 * @param slot Slot 0..17
 * @returns The selected byte, sign-extended
 */
static int _fcc_byte(uint32_t word, unsigned slot) {
	uint8_t byte;

	if(slot <= 1)
		byte = (uint8_t)word;
	else if(slot <= 5)
		byte = (uint8_t)(word >> 8);
	else if(slot <= 13)
		byte = (uint8_t)(word >> 16);
	else
		byte = (uint8_t)(word >> 24);
	return((int)(int8_t)byte);
}

/**
 * Fill one channel's target-power bytes, capped by the channel and the table.
 * @param channel Channel number
 * @param cap Signed cap applied to every byte
 * @param out 18 output bytes
 * @param limits 18 signed limit bytes
 * @param fcc Non-zero requests the FCC index fill; only 1 does
 * @param fcc_b Second FCC table
 * @param fcc_a First FCC table
 */
void get_chan_target_power_new(uint32_t channel, int cap, int out, int limits,
	int fcc, uint32_t fcc_b, uint32_t fcc_a) {
	uint32_t word;
	unsigned slot;
	int8_t *dest;
	int8_t *bound;
	int chosen;
	int8_t stored;

	word = 0x54545454;
	if(fcc == 1)
		get_rate_fcc_index(channel, &word, fcc_b, fcc_a);
	dest = (int8_t *)(uintptr_t)out;
	bound = (int8_t *)(uintptr_t)limits;
	for(slot = 0; slot < 18; slot++) {
		chosen = _fcc_byte(word, slot);
		if(chosen < (int)bound[slot])
			dest[slot] = (int8_t)chosen;
		else
			dest[slot] = bound[slot];
		stored = dest[slot];
		if(cap < (int)stored)
			dest[slot] = (int8_t)cap;
	}
}

/**
 * Walk a power table until the target sits between two entries.
 * @param index Starting entry
 * @param target Target power
 * @param code Receives the code halfword
 * @param second Receives the second halfword
 * @param remain Receives the residual
 * @param codes Code table address
 * @param seconds Second table address
 * @param powers Power table address
 * @param limit Maximum steps
 * @param adjust Added to the residual before the back-up test
 * @param thresh Residual threshold
 * @returns The entry that was selected
 */
uint32_t get_tx_gain_value_new(uint32_t index, uint32_t target, uint16_t *code,
	uint16_t *second, short *remain, int codes, int seconds, int powers,
	uint8_t limit, short adjust, short thresh) {
	int16_t *power;
	uint16_t *code_tab;
	uint16_t *second_tab;
	unsigned guard;
	int16_t here;
	int16_t next;
	uint16_t diff;
	int16_t sum;

	power = (int16_t *)(uintptr_t)powers;
	code_tab = (uint16_t *)(uintptr_t)codes;
	second_tab = (uint16_t *)(uintptr_t)seconds;
	guard = 0;
	while(true) {
		here = power[index];
		if(guard == limit)
			break;
		if((int)target < (int)here) {
			if(index == (uint32_t)(uint8_t)(limit - 1))
				break;
			next = power[index + 1];
			index = (index + 1) & 0xff;
			if((int)target >= (int)next)
				break;
		} else {
			if(index == 0)
				break;
			if((int)target < (int)power[index - 1])
				break;
			index = (index - 1) & 0xff;
		}
		guard = (guard + 1) & 0xff;
	}
	diff = (uint16_t)target - ((uint16_t *)power)[index];
	*remain = (short)diff;
	sum = (int16_t)((uint16_t)(diff + (uint16_t)adjust));
	if((int)thresh < (int)sum && index != 0) {
		index = (index - 1) & 0xff;
		*remain = (short)((uint16_t)target - ((uint16_t *)power)[index]);
	}
	*code = code_tab[index];
	*second = second_tab[index];
	return(index);
}

/**
 * Build 32 Wi-Fi gain rows.
 * @param channel Passed to the channel interpolator
 * @param samples Sample table passed as the interpolator's first argument
 * @param ref Reference subtracted from each step
 * @param extra Added to each step
 * @param codes Code table
 * @param seconds Second table
 * @param powers Power table
 * @param bytes Receives one signed byte per row
 * @param seconds_out Receives one halfword per row
 * @param codes_out Receives one halfword per row
 * @param log Non-zero prints each row
 */
void wifi_get_tx_gain_new(uint32_t channel, uint32_t samples, int ref, int extra,
	uint32_t codes, uint32_t seconds, uint32_t powers, int bytes, int seconds_out,
	int codes_out, int8_t log) {
	uint32_t index;
	unsigned row;
	int interp;
	int16_t adjust;
	int8_t step;
	int target;
	uint16_t code;
	uint16_t second;
	short remain;
	int8_t *bout;
	uint16_t *sout;
	uint16_t *cout;

	interp = (int)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CHAN_INTERP)(samples, channel);
	interp = (int)(uint16_t)interp;
	adjust = (int16_t)(0 - interp);
	index = 0;
	bout = (int8_t *)(uintptr_t)bytes;
	sout = (uint16_t *)(uintptr_t)seconds_out;
	cout = (uint16_t *)(uintptr_t)codes_out;
	for(row = 0; row < 32; row++) {
		step = (int8_t)((int)phy_param[_PAR_TPW] - (int)(row * 4));
		target = ((int)step - ref) + extra;
		index = get_tx_gain_value_new(index, (uint32_t)target, &code, &second,
			&remain, (int)codes, (int)seconds, (int)powers, 16, adjust, -8);
		bout[row] = (int8_t)((int)remain - interp);
		sout[row] = second;
		cout[row] = code;
		if(log != 0) {
			mp_arch_rf_printf("%d,0x%x,0x%x,%d, %d, power=%d, %d\n", row,
				code, second, (int)bout[row], (int)remain, (int)step, target);
		}
	}
}

/**
 * Build 16 Bluetooth gain rows.
 * @param base Byte pair used as the per-row offset
 * @param start Signed start subtracted inside the residual
 * @param span Signed span added inside the residual
 * @param codes Code table
 * @param seconds Second table
 * @param powers Power table
 * @param bytes Receives one signed byte per row
 * @param seconds_out Receives one halfword per row
 * @param codes_out Receives one halfword per row
 * @param origin Signed power origin
 * @param slope Signed power slope
 * @param log Non-zero prints each row
 */
void bt_get_tx_gain_new(int base, int start, int span, uint32_t codes, uint32_t seconds,
	uint32_t powers, int bytes, int seconds_out, int codes_out, int8_t origin,
	int8_t slope, int8_t log) {
	uint32_t index;
	unsigned row;
	uint16_t power;
	uint16_t pitch;
	int delta_base;
	int sat;
	int delta;
	int8_t scale;
	uint16_t scale16;
	uint16_t code;
	uint16_t second;
	short remain;
	int clamped;
	int8_t *bout;
	uint16_t *sout;
	uint16_t *cout;

	power = (uint16_t)(int16_t)origin;
	pitch = (uint16_t)((int)slope << 2);
	delta_base = span - start;
	index = 0;
	bout = (int8_t *)(uintptr_t)bytes;
	sout = (uint16_t *)(uintptr_t)seconds_out;
	cout = (uint16_t *)(uintptr_t)codes_out;
	for(row = 0; row < 16; row++) {
		sat = get_data_sat((int)(int16_t)power, 80, -60);
		delta = (int)(int16_t)(delta_base + sat);
		scale = *(int8_t *)(base + 1);
		scale16 = (uint16_t)(int16_t)scale;
		index = get_tx_gain_value_new(index, (uint32_t)delta, &code, &second,
			&remain, (int)codes, (int)seconds, (int)powers, 14,
			(short)(0 - (int)scale), 12);
		cout[row] = code;
		sout[row] = second;
		clamped = (int)(int16_t)((int)remain - (int)scale16);
		if(clamped > 24)
			clamped = 24;
		if(clamped < -60)
			clamped = -60;
		bout[row] = (int8_t)clamped;
		if(log != 0) {
			mp_arch_rf_printf("%d,0x%x,0x%x,%d, %d, power=%d, %d\n", row,
				code, second, (int)bout[row], (int)remain,
				(int)(int16_t)sat, delta);
		}
		power = (uint16_t)(power + pitch);
	}
}

/**
 * Load the Bluetooth code and power tables and build 16 rows.
 * @param codes_out Row codes
 * @param seconds_out Row second words
 * @param bytes_out Row bytes
 * @param log Print flag
 */
void bt_get_tx_tab(uint32_t codes_out, uint32_t seconds_out, uint32_t bytes_out, uint32_t log) {
	uint16_t codes[14];
	uint16_t seconds[14];
	int16_t powers[14];
	int8_t ref;
	int8_t span;

	memcpy(codes, _bt_tx_code, sizeof(codes));
	memset(seconds, 0, sizeof(seconds));
	memcpy(powers, _bt_tx_pwr, sizeof(powers));
	ref = (int8_t)phy_param[_PAR_BT_REF];
	span = (int8_t)((int)phy_param[_PAR_BT_HOLD] - (int)phy_param[_PAR_STEP]);
	bt_get_tx_gain_new((int)(uintptr_t)(phy_param + _PAR_BT_SAMP), (int)ref, (int)span,
		(uint32_t)(uintptr_t)codes, (uint32_t)(uintptr_t)seconds,
		(uint32_t)(uintptr_t)powers, (int)bytes_out, (int)seconds_out, (int)codes_out,
		-96, 3, (int8_t)log);
}

/**
 * Load the Wi-Fi code and power tables and build 32 rows.
 * @param channel Channel passed through
 * @param bytes_out Row bytes
 * @param seconds_out Row second words
 * @param codes_out Row codes
 * @param log Print flag
 */
void wifi_get_tx_tab(uint32_t channel, uint32_t bytes_out, uint32_t seconds_out,
	uint32_t codes_out, uint32_t log) {
	uint16_t codes[16];
	uint16_t seconds[16];
	int16_t powers[16];
	int8_t ref;
	int8_t span;

	memcpy(codes, _wifi_tx_code, sizeof(codes));
	memset(seconds, 0, sizeof(seconds));
	seconds[0] = 0x80;
	seconds[14] = 0x80;
	memcpy(powers, _wifi_tx_pwr, sizeof(powers));
	ref = (int8_t)phy_param[_PAR_WIFI_REF];
	span = (int8_t)((int)phy_param[_PAR_WIFI_HOLD] - (int)phy_param[_PAR_STEP]);
	wifi_get_tx_gain_new(channel, (uint32_t)(uintptr_t)(phy_param + _PAR_WIFI_SAMP),
		(int)ref, (int)span, (uint32_t)(uintptr_t)codes,
		(uint32_t)(uintptr_t)seconds, (uint32_t)(uintptr_t)powers,
		(int)bytes_out, (int)seconds_out, (int)codes_out, (int8_t)log);
}

/**
 * Target power for one channel into an 18-byte buffer.
 * @param channel Channel number
 * @param out Buffer address
 */
void wifi_get_target_power_new(uint32_t channel, int out) {
	get_chan_target_power_new(channel, (int)(int8_t)phy_param[_PAR_TPW], out,
		(int)(uintptr_t)(phy_param + _PAR_LIMIT), (int)phy_param[_PAR_RATE],
		(uint32_t)(uintptr_t)(phy_param + _PAR_FCC_B),
		(uint32_t)(uintptr_t)(phy_param + _PAR_FCC_A));
}

/**
 * Write one gain image into memory.
 * @param bt Non-zero selects the Bluetooth index
 * @param count Rows to write
 * @param codes Code halfwords
 * @param bb Baseband halfwords
 * @param bytes Signed byte per row
 * @param quad Quad table address
 * @param mask Halfword mixed into the first word
 */
void set_tx_gain_mem(int bt, int count, int codes, uint16_t *bb, int bytes, int quad,
	uint16_t *mask) {
	uint16_t row[4];
	unsigned long base;
	unsigned i;
	unsigned n;
	uint16_t bbv;
	uint32_t index;
	uint16_t code;
	int8_t scale;
	uint32_t word0;
	uint32_t word1;
	uint32_t word2;
	uint32_t slot;

	base = mp_arch_rf_read(_REG_GAIN) >> 24;
	base = (base + (unsigned long)(bt != 0) * 0x20UL) & 0xffUL;
	force_txrx_off(1);
	for(i = 0; i != (unsigned)count; i++) {
		if(bt == 0) {
			bbv = ((uint16_t *)(uintptr_t)bb)[i];
			index = txbbgain_to_index(bbv) & 0xffff;
		} else {
			bbv = bb[0];
			index = bt_bb_to_index(bbv);
		}
		for(n = 0; n < 4; n++)
			row[n] = *(uint16_t *)((uintptr_t)quad + index * 8 + n * 2);
		code = ((uint16_t *)(uintptr_t)codes)[i];
		scale = ((int8_t *)(uintptr_t)bytes)[i];
		word0 = ((uint32_t)mask[0] & 0x1fff)
			| ((uint32_t)row[2] << 22)
			| ((uint32_t)row[1] << 31)
			| ((uint32_t)row[3] << 13);
		word1 = ((uint32_t)row[0] << 8)
			| ((uint32_t)row[1] >> 1)
			| (((uint32_t)bbv >> 6) & 0xff) << 17
			| ((uint32_t)(code & 7) << 31)
			| ((uint32_t)(bbv & 0x3f) << 20)
			| 0x10000000UL;
		word2 = ((uint32_t)((int)scale << 7))
			| ((uint32_t)(code & 7) >> 1)
			| (((uint32_t)code >> 1) & 0x1c);
		slot = (base + i) & 0xff;
		write_gain_mem_new(word0, word1, word2, slot);
	}
	force_txrx_off(0);
}

/**
 * Program the Bluetooth transmit gain.
 * @param log Print flag forwarded to the table builder
 */
void bt_set_tx_gain(uint32_t log) {
	uint8_t bytes[16];
	uint16_t seconds[16];
	uint16_t codes[16];

	bt_get_tx_tab((uint32_t)(uintptr_t)codes, (uint32_t)(uintptr_t)seconds,
		(uint32_t)(uintptr_t)bytes, log);
	set_tx_gain_mem(1, 16, (int)(uintptr_t)codes, seconds, (int)(uintptr_t)bytes,
		(int)(uintptr_t)(phy_param + _PAR_BT_QUAD),
		(uint16_t *)(phy_param + _PAR_MASK));
}

/**
 * Calibrate Bluetooth DC, then load a quiet gain image.
 */
void bt_tx_gain_init(void) {
	set_channel_rfpll_freq(0x985, phy_param[_PAR_FCC], 0);
	MP_ARCH_RF_FN(100)(phy_param + _PAR_BT_BASE, 6);
	bt_txdc_cal_new();
	bt_tx_pwctrl_init_new();
	bt_set_tx_gain(0);
}

/**
 * Program the Wi-Fi transmit gain when the enable byte is clear.
 * @param channel Channel passed to the table builder
 * @param log Print flag
 */
void wifi_set_tx_gain(uint32_t channel, uint32_t log) {
	uint16_t pack[8];
	uint8_t bytes[32];
	uint16_t seconds[32];
	uint16_t codes[32];

	pack[0] = mp_arch_rf_u16(_PAR_PACK);
	pack[1] = mp_arch_rf_u16(_PAR_PACK + 2);
	pack[2] = 0x0100;
	pack[3] = 0x0100;
	pack[4] = mp_arch_rf_u16(_PAR_PACK + 4);
	pack[5] = mp_arch_rf_u16(_PAR_PACK + 6);
	pack[6] = 0x0100;
	pack[7] = 0x0100;
	wifi_get_tx_tab(channel, (uint32_t)(uintptr_t)bytes, (uint32_t)(uintptr_t)seconds,
		(uint32_t)(uintptr_t)codes, log);
	if(phy_param[_PAR_GAIN_EN] == 0) {
		set_tx_gain_mem(0, 32, (int)(uintptr_t)codes, seconds, (int)(uintptr_t)bytes,
			(int)(uintptr_t)pack, (uint16_t *)(phy_param + _PAR_MASK));
	}
}

/**@}*/
