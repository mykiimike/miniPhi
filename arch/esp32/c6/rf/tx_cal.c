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

Measure the transmit DC, image and power codes and store them in phy_param.

@{
*/

	#define _REG_IQ       0x600A0418UL
	#define _REG_SAVED    0x600A0420UL
	#define _REG_FE_B     0x600A0450UL
	#define _REG_FE_GATE  0x600A0810UL
	#define _REG_FE_MASK  0x600A081CUL
	#define _REG_CAP_GATE 0x600A0910UL

	#define _PAR_FLAG     0xa4
	#define _PAR_BT_CODE  0x03
	#define _PAR_POWER    0x0e
	#define _PAR_TXGAIN   0x10
	#define _PAR_BT_BB    0x13
	#define _PAR_BT_DIG   0x14
	#define _PAR_TEMP     0x18
	#define _PAR_BACKOFF  0x2b
	#define _PAR_XTAL     0x51
	#define _PAR_TXDC     0xa8
	#define _PAR_BT_DC    0x104
	#define _PAR_IQ       0xd0
	#define _PAR_IQ_LB    0xe6
	#define _PAR_DCAP     0xdc
	#define _PAR_WIFI_ATT 0xf1
	#define _PAR_WIFI_CAL 0xf4
	#define _PAR_WIFI_PWR 0xf7
	#define _PAR_BT_ATT   0xf8
	#define _PAR_BT_STEP  0xfb
	#define _PAR_BT_PWR   0xfe
	#define _PAR_BT_ZERO  0x100
	#define _PAR_CHAN     0x11c
	#define _PAR_PWDET    0x18e
	#define _PAR_BUSY     0x1a1

	#define _IQ_FULL      256
	#define _IQ_LIMIT     511
	#define _TONE_GAIN    120
	#define _GAP          20

static const uint8_t _chan[3] = { 1, 6, 11 };

static void _pbus(uint32_t sel, uint32_t part, uint32_t value) {
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS)(sel, part, value);
}

static uint32_t _pbus_half(uint32_t sel, uint32_t part) {
	uint32_t	word;

	word = (uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS_RD)(sel, part);
	return(word & 0xffffU);
}

static void _tone(uint32_t enable, uint32_t freq, uint32_t gain) {
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_TX_TONE)(enable, freq, gain, 0, 0, 0);
}

static int _i8(int value) {
	return((int)(int8_t)value);
}

static int _i16(int value) {
	return((int)(int16_t)value);
}

static int _flag_high(int shift) {
	return((int)(mp_arch_rf_u32(_PAR_FLAG) << shift) < 0);
}

static void _or_flag(uint32_t bit) {
	mp_arch_rf_set_u32(_PAR_FLAG, mp_arch_rf_u32(_PAR_FLAG) | bit);
}

/**
 * Search one I or Q code against the IQ detector.
 * @param row Four halfwords; the first two receive the codes
 * @param unused_1 Ignored
 * @param unused_2 Ignored
 * @param unused_3 Ignored
 * @param unused_4 Ignored
 */
void txdc_cal_new(uint16_t *row, uint32_t unused_1, uint32_t unused_2,
		uint32_t unused_3, uint32_t unused_4) {
	int		i_code;
	int		q_code;
	int		i_sum;
	int		q_sum;
	int		step;
	int		round;
	int		grown;
	uint32_t	sense;
	uint32_t	high;

	(void)unused_1;
	(void)unused_2;
	(void)unused_3;
	(void)unused_4;
	_pbus(2, 2, _IQ_FULL);
	_pbus(3, 2, _IQ_FULL);
	row[2] = _IQ_FULL;
	row[3] = _IQ_FULL;
	set_txclk_en(1);
	_tone(1, 600, _TONE_GAIN);
	i_code = _IQ_FULL;
	q_code = _IQ_FULL;
	i_sum = 0;
	q_sum = 0;
	step = 124;
	for(round = 0; round < 12; round++) {
		_pbus(3, 1, (uint32_t)(q_code & 0xffff));
		_pbus(2, 1, (uint32_t)(i_code & 0xffff));
		ets_delay_us(2);
		mp_arch_rf_write(_REG_IQ, mp_arch_rf_read(_REG_IQ) | 2UL);
		mp_arch_rf_write(_REG_IQ, mp_arch_rf_read(_REG_IQ) & ~1UL);
		mp_arch_rf_write(_REG_IQ, mp_arch_rf_read(_REG_IQ) | 1UL);
		do {
			sense = mp_arch_rf_read(_REG_IQ);
		} while((int)(sense << 9) >= 0);
		sense = mp_arch_rf_read(_REG_IQ);
		high = mp_arch_rf_read(_REG_IQ);
		if((int)(sense << 2) < 0) {
			grown = i_code - (step & 0xffff);
			i_code = ((int)((uint16_t)grown) & 0x8000) != 0 ? 0 : grown;
		}
		else {
			grown = i_code + (step & 0xffff);
			i_code = _i16(grown) > _IQ_LIMIT ? _IQ_LIMIT : grown;
		}
		i_code = _i16(i_code);
		if((high & 0x10000000UL) == 0) {
			grown = q_code + (step & 0xffff);
			q_code = _i16(grown) > _IQ_LIMIT ? _IQ_LIMIT : grown;
		}
		else {
			grown = q_code - (step & 0xffff);
			q_code = ((int)((uint16_t)grown) & 0x8000) != 0 ? 0 : grown;
		}
		q_code = _i16(q_code);
		if(step == 2)
			step = 1;
		else
			step = (step >> 1) + 1;
		if(round >= 8) {
			i_sum = _i16(i_sum + i_code);
			q_sum = _i16(q_sum + q_code);
		}
	}
	q_code = (int)((uint16_t)((q_sum + 2) >> 2));
	_pbus(3, 1, (uint32_t)q_code);
	i_code = (int)((uint16_t)((i_sum + 2) >> 2));
	_pbus(2, 1, (uint32_t)i_code);
	row[0] = (uint16_t)i_code;
	row[1] = (uint16_t)q_code;
	mp_arch_rf_write(_REG_IQ, mp_arch_rf_read(_REG_IQ) & ~2UL);
	mp_arch_rf_write(_REG_IQ, mp_arch_rf_read(_REG_IQ) & ~1UL);
	_tone(0, 600, _TONE_GAIN);
}

/**
 * Measure the Bluetooth transmit DC once and mark the status word.
 */
void bt_txdc_cal_new(void) {
	if(_flag_high(0x13))
		return;
	pbus_debugmode();
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_TX)(0xf, 0);
	_pbus(1, 1, _pbus_half(1, 1) | 2U);
	_pbus(4, 2, (uint32_t)phy_param[_PAR_BT_DIG] << 3);
	txdc_cal_new((uint16_t *)(phy_param + _PAR_BT_DC), 0, 0, 0, 0);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
	pbus_workmode();
	_or_flag(0x1000);
}

/**
 * Walk the five baseband gains and record a DC pair for each.
 * @param row First of five DC pairs
 * @param span Passed to the transmit power-up slot
 * @param unused Passed to the transmit power-up slot
 * @param flag Non-zero forces the receiver path on
 */
void txdc_cal_init_new(uint16_t *row, uint32_t span, uint32_t unused, int flag) {
	int		log;
	int		index;
	uint32_t	gain;

	log = (int)((mp_arch_rf_u16(_PAR_TXGAIN) >> 4) & 1U);
	pbus_debugmode();
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_TX)(span, unused);
	if(flag != 0)
		_pbus(1, 1, _pbus_half(1, 1) | 2U);
	if(log != 0)
		phy_printf("%d txdc: ", flag);
	for(index = 0; index < 5; index++) {
		gain = (uint32_t)index_to_txbbgain(index);
		_pbus(1, 2, gain);
		txdc_cal_new(row, 0, 0, 0, 0);
		if(log != 0)
			phy_printf("bb=0x%x,%d,%d,%d,%d,", gain,
				row[0], row[1], row[2], row[3]);
		row += 4;
	}
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
	pbus_workmode();
	if(log != 0)
		phy_printf("\n");
}

/**
 * Balance one transmit image coefficient pair.
 * @param code Starting attenuator code
 * @param power Power word handed to the mismatch measurement
 * @param out Two result bytes
 * @param dump Non-zero prints the eight attempts
 * @param verbose Non-zero prints each attempt
 */
void txiq_cover_new(int code, uint32_t power, uint8_t *out, int dump, int verbose) {
	int		i_code;
	int		q_code;
	int		i_sum;
	int		q_sum;
	int		round;
	int		floor;
	int		left;
	int		right;
	int		smaller;
	int		sum;
	int		i_byte;
	int		q_byte;
	int16_t		pair[2];
	int8_t		trace[16];

	floor = _i8(code - 12);
	if(floor < 0)
		floor = 0;
	floor = (int)(uint8_t)floor;
	i_code = 0;
	q_code = 0;
	i_sum = 0;
	q_sum = 0;
	for(round = 0; round < 7; round++) {
		i_code = (int)txiq_set_reg(i_code, 1);
		q_code = (int)txiq_set_reg(q_code, 0);
		txiq_get_mis_pwr(1, floor, power, &pair[0], &pair[1]);
		smaller = pair[1];
		if(pair[0] < pair[1])
			smaller = pair[0];
		smaller = _i16(smaller);
		if(smaller == 0)
			smaller = 1;
		left = (((int)pair[1] - (int)pair[0]) << 11) / smaller;
		left = (left + 16) >> 5;
		out[0] = (uint8_t)left;
		if(verbose != 0) {
			phy_printf("sar_code=%d, pwr1=%d, pwr2=%d, diff_pwr=%d, mis_data[0]=%d\n",
				(int)get_tone_sar_dout(1), (int)pair[0], (int)pair[1],
				left, (int)(int8_t)out[0]);
		}
		txiq_get_mis_pwr(0, code, power, &pair[0], &pair[1]);
		sum = _i16((int)pair[0] + (int)pair[1]);
		if(sum == 0)
			sum = 1;
		right = ((((int)pair[0] - (int)pair[1]) << 12) / sum + 8) >> 4;
		out[1] = (uint8_t)right;
		if(verbose != 0) {
			phy_printf("sar_code=%d, pwr1=%d, pwr2=%d, diff_pwr=%d, mis_data[1]=%d\n",
				(int)get_tone_sar_dout(1), (int)pair[0], (int)pair[1],
				right, (int)(int8_t)out[1]);
		}
		i_byte = (int)(uint8_t)(int8_t)out[0];
		q_byte = (int)(uint8_t)(int8_t)out[1];
		if(round <= 2) {
			i_code = _i8(i_code - i_byte);
			q_code = _i8(q_code - q_byte);
		}
		else {
			i_sum = _i8(i_sum + i_byte);
			q_sum = _i8(q_sum + q_byte);
			if(round == 6) {
				i_code = _i8(i_code - ((i_sum + 2) >> 2));
				q_code = _i8(q_code - ((q_sum + 2) >> 2));
			}
		}
		trace[round * 2] = (int8_t)out[0];
		trace[round * 2 + 1] = (int8_t)out[1];
	}
	txiq_set_reg(i_code, 1);
	txiq_set_reg(q_code, 0);
	out[0] = (uint8_t)i_code;
	out[1] = (uint8_t)q_code;
	trace[14] = (int8_t)i_code;
	trace[15] = (int8_t)q_code;
	if(dump == 0)
		return;
	for(round = 0; round < 8; round++)
		phy_printf("%d,%d; ", (int)trace[round * 2], (int)trace[round * 2 + 1]);
	phy_printf("\n");
}

/**
 * Measure one transmit image and pack it into a halfword.
 * @param tone Tone selector
 * @param dco DC word address
 * @param iq Packed result
 * @param atten Target attenuation
 * @param power Power argument
 * @param mode 1 forces the receiver, 2 calibrates DC first
 */
void rfcal_txiq_new(uint32_t tone, uint32_t dco, uint16_t *iq, int atten,
		uint32_t power, int mode) {
	uint32_t	saved;
	uint32_t	span;
	int		backoff;
	int		i_code;
	int		q_code;
	uint8_t		pair[2];

	(void)tone;
	span = (uint32_t)(uint8_t)(64 - phy_param[_PAR_BACKOFF]);
	backoff = (int)mp_arch_rf_u16(_PAR_POWER);
	mp_arch_rf_write(_REG_FE_B, mp_arch_rf_read(_REG_FE_B) | 0x2000UL);
	mp_arch_rf_write(_REG_FE_B, mp_arch_rf_read(_REG_FE_B) & 0xffffbfffUL);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_TXCAL_DBG)();
	_pbus(1, 2, 0);
	if(mode == 2) {
		MP_ARCH_RF_FN(0x0c)(1);
		txdc_cal_new((uint16_t *)dco, 0, 0, 0, 0);
	}
	else {
		if(mode == 1)
			_pbus(1, 1, _pbus_half(1, 1) | 2U);
		pbus_set_dco(dco);
	}
	atten = _i16(atten);
	saved = mp_arch_rf_read(_REG_SAVED);
	span = (uint32_t)get_power_atten(atten, power, (int)span, backoff, 0);
	txiq_cover_new((int)(uint8_t)span, (uint32_t)atten, pair, 0, 0);
	txcal_work_mode();
	i_code = (int)get_data_sat((int)(int8_t)pair[0], 31, -31);
	q_code = (int)get_data_sat((int)(int8_t)pair[1], 63, -63);
	pair[0] = (uint8_t)i_code;
	pair[1] = (uint8_t)q_code;
	*iq = (uint16_t)(((pair[0] & 0x3f) << 7) | (q_code & 0x7f));
	mp_arch_rf_write(_REG_SAVED, saved);
	if(mode == 2)
		MP_ARCH_RF_FN(0x0c)(0);
	mp_arch_rf_write(_REG_FE_B, mp_arch_rf_read(_REG_FE_B) | 0x4000UL);
}

/**
 * Calibrate the direct image and the loopback image.
 */
void txiq_cal_init_new(void) {
	int		level;
	int		clamped;
	uint8_t		scratch[8];

	if(_flag_high(0x11))
		return;
	level = _i16((int)(int8_t)phy_param[_PAR_TEMP]);
	clamped = level;
	if(clamped < 0)
		clamped = 0;
	rfcal_txiq_new(0, (uint32_t)(uintptr_t)(phy_param + _PAR_TXDC),
		(uint16_t *)(phy_param + _PAR_IQ), 128, (uint32_t)(uint8_t)clamped, 0);
	clamped = level;
	if(clamped < 0)
		clamped = 0;
	clamped = (int)get_data_sat(_i16(clamped - 20), 120, 0);
	rfcal_txiq_new(0, (uint32_t)(uintptr_t)scratch,
		(uint16_t *)(phy_param + _PAR_IQ_LB), 128, (uint32_t)(uint8_t)clamped, 2);
	_or_flag(0x4000);
}

/**
 * Servo one power table until each step is within a count of its target.
 * @param tone Tone selector
 * @param table Target bytes, last entry first
 * @param count Number of target bytes
 * @param base Subtracted from the chosen attenuator
 * @param dest One result byte per target
 * @param power Power detector selector
 * @param target Desired detector reading
 * @param verbose Non-zero prints each attempt
 */
void rfcal_pwrctrl_new(uint32_t tone, uint8_t *table, int count, int base,
		uint8_t *dest, uint32_t power, int target, int verbose) {
	int		index;
	int		servo;
	int		step;
	int		prev;
	int		guess;
	int		clipped;
	int		reading;
	int		error;
	int		goal;
	int		mixed;

	index = _i8(count - 1);
	servo = 0;
	for(;;) {
		if(_i8(index) < 0) {
			MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_STOP_TONE)(1);
			return;
		}
		goal = (int)table[index];
		if(index < count - 1)
			servo = _i16(servo + (int)table[index + 1] - (int)(uint16_t)goal);
		step = 0;
		prev = 2;
		for(;;) {
			guess = _i16(target - _i8(-servo));
			clipped = guess;
			if(clipped < 0)
				clipped = 0;
			clipped = _i16(clipped);
			if(clipped > 100)
				clipped = 100;
			_tone(1, tone, (uint32_t)(uint8_t)clipped);
			reading = (int)get_power_db(power);
			reading = _i16(reading + (int)get_power_db(power));
			reading = (reading + 4) >> 3;
			if(reading < 0)
				reading = 0;
			error = reading - (int)(uint16_t)goal;
			if(error > 24)
				error = 24;
			else if(error < -24)
				error = -24;
			if(verbose != 0)
				phy_printf("i=%02d  acc=%03d  tgt=%03d  err=%03d  ser=%03d att=%03d\n",
					step, reading, goal, error, servo, clipped);
			if(error == 0) {
				if(step != 0)
					break;
			}
			else if(error == -1 && prev == 1)
				break;
			if(prev == 0)
				break;
			if(guess > 0) {
				if(guess > 99 && error > 0) {
					clipped = 100;
					break;
				}
			}
			else if(error < 0) {
				clipped = 0;
				break;
			}
			mixed = (int)(uint16_t)((uint16_t)error + (uint16_t)servo);
			if((uint16_t)((uint16_t)error + 2) > 4)
				mixed -= error >> 2;
			servo = _i16(mixed);
			step++;
			prev = error;
			if(step == 10)
				break;
		}
		mixed = _i8(clipped - base);
		if(mixed < -24)
			dest[index] = 0xe8;
		else
			dest[index] = (uint8_t)mixed;
		index--;
	}
}

/**
 * Sweep one capacitor nibble and keep the code with the largest estimate.
 * @param verbose Non-zero prints each sample
 * @param out Two result bytes
 */
void rfcal_txcap_loop(int verbose, uint8_t *out) {
	int		pass;
	int		code;
	int		stride;
	int		best;
	int		sample;
	int		msb;
	int		lsb;
	uint32_t	est[4];

	code = 7;
	stride = 1;
	for(pass = 0; pass < 2; pass++) {
		out[pass] = (uint8_t)code;
		best = 0;
		for(;;) {
			if(pass == 1) {
				msb = 7;
				lsb = 4;
			}
			else {
				msb = 3;
				lsb = 0;
			}
			mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 2, (uint32_t)msb,
				(uint32_t)lsb, (uint32_t)(uint8_t)code);
			dc_iq_est_high(1, 0x800, (int *)est);
			sample = (int)(int16_t)est[2];
			if(verbose != 0)
				phy_printf("%d, %d; ", code, sample);
			if(best < sample) {
				out[pass] = (uint8_t)code;
				best = sample;
			}
			code = _i8((int)(uint8_t)code - stride);
			if(code < 0)
				break;
		}
		if(verbose != 0)
			phy_printf("reg_i=%d, indata=%d\n", pass, out[pass]);
		if(pass == 1) {
			msb = 7;
			lsb = 4;
		}
		else {
			msb = 3;
			lsb = 0;
		}
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 2, (uint32_t)msb,
			(uint32_t)lsb, out[pass]);
		code = 13;
		stride = 2;
	}
}

/**
 * Calibrate the transmit capacitor at channels 1, 6 and 11.
 */
void tx_cap_init_loop(void) {
	uint32_t	seed[2];
	uint32_t	est[4];
	int		verbose;
	int		round;
	int		tries;
	int		code;
	int		sample;

	seed[0] = 0x00010001;
	seed[1] = 0x00010001;
	verbose = (int)((mp_arch_rf_u16(_PAR_TXGAIN) >> 7) & 1U);
	for(round = 0; round < 3; round++) {
		set_channel_rfpll_freq(_chan[round], phy_param[_PAR_XTAL], 0);
		code = 60;
		tries = 2;
		mp_arch_rf_write(_REG_CAP_GATE, mp_arch_rf_read(_REG_CAP_GATE) | 0x4000UL);
		mp_arch_rf_write(_REG_CAP_GATE, mp_arch_rf_read(_REG_CAP_GATE) | 0x8000UL);
		pbus_debugmode();
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
		MP_ARCH_RF_FN(0x0c)(1);
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_LOOPBACK)(0, 0x43, 0x20);
		pbus_rx_dco_cal(0x800, seed, 10, 0, 0);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 2, 3, 0, 1);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 2, 7, 4, 5);
		for(;;) {
			_tone(1, 128, (uint32_t)(uint8_t)code);
			dc_iq_est(1, 0x800, est);
			sample = (int)est[2];
			if(sample > 46)
				code += 24;
			else if(sample <= 31)
				code -= 24;
			else
				break;
			code = _i8(code);
			tries--;
			if(tries == 0)
				break;
		}
		rfcal_txcap_loop(verbose, phy_param + _PAR_DCAP + round * 2);
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_STOP_TONE)(1);
		MP_ARCH_RF_FN(0x0c)(0);
		MP_ARCH_RF_FN(0x88)();
		pbus_workmode();
		mp_arch_rf_write(_REG_CAP_GATE, mp_arch_rf_read(_REG_CAP_GATE) & 0xffffbfffUL);
		mp_arch_rf_write(_REG_CAP_GATE, mp_arch_rf_read(_REG_CAP_GATE) & 0xffff7fffUL);
	}
}

/**
 * Settle one DC pair from the power detector.
 * @param row Two halfwords, updated in place
 * @param verbose Non-zero prints the search
 */
void txdc_cal_pwdet(uint16_t *row, int verbose) {
	uint16_t	dco[4];
	uint16_t	sample[128];
	uint8_t		mark[2];
	int		axis;
	int		up;
	int		down;
	int		pass;
	int		side;
	int		step;
	int		count;
	int		best;
	int		reading;
	int		index;
	int		gap;
	int		floor;
	int		hi;
	int		lo;
	int		plus;
	int		minus;
	uint32_t	limit;
	uint16_t	base;

	dco[0] = row[0];
	dco[1] = row[1];
	dco[2] = 0x0100;
	dco[3] = 0x0100;
	for(axis = 0; axis < 2; axis++) {
		up = 10;
		down = 10;
		pass = 2;
		for(;;) {
			dco[axis] = (uint16_t)(row[axis] + up);
			pbus_set_dco(dco);
			ets_delay_us(10);
			reading = (int)get_tone_sar_dout(2);
			dco[axis] = (uint16_t)(row[axis] - down);
			pbus_set_dco(dco);
			ets_delay_us(10);
			best = (int)get_tone_sar_dout(2);
			if((uint32_t)(best + _GAP) < (uint32_t)reading)
				up = (up - 5) & 0xff;
			else if((uint32_t)(reading + _GAP) >= (uint32_t)best)
				break;
			else
				down = (down - 5) & 0xff;
			if(pass == 1)
				break;
			pass = 1;
		}
		up = up & 0xffff;
		down = down & 0xffff;
		count = 0;
		for(side = 0; side < 2; side++) {
			best = 0xffff;
			for(step = 0; step < 50; step++) {
				if(side == 0)
					plus = up + step;
				else
					plus = -(down + step);
				dco[axis] = (uint16_t)(row[axis] + (uint16_t)plus);
				pbus_set_dco(dco);
				ets_delay_us(10);
				reading = (int)get_tone_sar_dout(2);
				sample[count] = (uint16_t)reading;
				count = (count + 1) & 0xff;
				if((uint32_t)reading < (uint32_t)best)
					best = reading;
				mark[side] = (uint8_t)step;
				if(verbose != 0)
					phy_printf("%d,%d,%d, dco %d,%d, code %d,%d\n",
						axis, side, step, dco[0], dco[1], reading, best);
				if(step > 5 && (uint32_t)reading > (uint32_t)best + 30)
					break;
			}
		}
		for(index = 0; index < count; index++) {
			if(index == 0 || index == (int)mark[0] + 1) {
				if(sample[index + 1] < sample[index])
					sample[index] = sample[index + 1];
			}
			else if(sample[index] < sample[index - 1])
				sample[index] = sample[index - 1];
		}
		gap = (int)abs_temp((int)sample[0] - (int)sample[mark[0] + 1]);
		if((uint16_t)gap < _GAP)
			gap = _GAP;
		gap = (int)(uint8_t)gap;
		floor = sample[0];
		if(sample[mark[0] + 1] < sample[0])
			floor = sample[mark[0] + 1];
		floor = floor & 0xffff;
		base = row[axis];
		hi = _i16((int)base + up);
		lo = _i16((int)base - down);
		limit = (uint32_t)gap + (uint32_t)floor;
		for(index = 0; index < count; index++) {
			if((uint32_t)sample[index] > limit)
				continue;
			plus = _i16((int)base + up + (index & 0xffff));
			minus = _i16((int)base + (int)mark[0] + 1 - down - (index & 0xffff));
			if((unsigned)mark[0] < (unsigned)index)
				lo = minus;
			else
				hi = plus;
			if(verbose != 0)
				phy_printf("%d,%d,%d, %d,%d, %d, %d,%d, %d,%d\n",
					index, count, mark[0], plus, minus, sample[index],
					hi, lo, floor, gap);
		}
		dco[axis] = (uint16_t)((hi + lo + 1) / 2 + 1);
		if(verbose != 0)
			phy_printf("%d,%d,%d\n", dco[0], dco[1]);
	}
	row[0] = dco[0];
	row[1] = dco[1];
	pbus_set_dco(dco);
}

/**
 * Average the power-detector DC at two baseband gains.
 * @param unused Ignored
 * @param keep Non-zero leaves the front end in the measurement state
 */
void txdc_cal_pwdet_init(uint32_t unused, int keep) {
	uint32_t	saved_mask;
	uint32_t	saved_gate;
	uint16_t	pair[2];
	uint32_t	flat[2];
	uint32_t	gain;
	int		log;
	int		index;
	int		sum_i;
	int		sum_q;
	int		pass;

	(void)unused;
	mp_arch_rf_set_u16(_PAR_PWDET, mp_arch_rf_u16(_PAR_TXDC));
	mp_arch_rf_set_u16(_PAR_PWDET + 2, mp_arch_rf_u16(_PAR_TXDC + 2));
	mp_arch_rf_set_u16(_PAR_PWDET + 4, mp_arch_rf_u16(_PAR_TXDC + 8));
	mp_arch_rf_set_u16(_PAR_PWDET + 6, mp_arch_rf_u16(_PAR_TXDC + 10));
	log = (int)((mp_arch_rf_u16(_PAR_TXGAIN) >> 4) & 1U);
	saved_mask = mp_arch_rf_read(_REG_FE_MASK);
	saved_gate = mp_arch_rf_read(_REG_FE_GATE);
	mp_arch_rf_write(_REG_FE_MASK, (mp_arch_rf_read(_REG_FE_MASK) & 0xffffff00UL) | 0xf0UL);
	mp_arch_rf_write(_REG_FE_GATE, (mp_arch_rf_read(_REG_FE_GATE) & 0xfffff00fUL) | 0x780UL);
	set_txclk_en(1);
	MP_ARCH_RF_FN(0x1c)();
	pbus_debugmode();
	MP_ARCH_RF_FN(0x88)();
	MP_ARCH_RF_FN(0x80)();
	_tone(1, 512, _TONE_GAIN);
	ets_delay_us(1);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_TX)(0x3f, 0);
	for(index = 0; index < 2; index++) {
		pair[0] = mp_arch_rf_u16(_PAR_PWDET + (unsigned)index * 4);
		pair[1] = mp_arch_rf_u16(_PAR_PWDET + 2 + (unsigned)index * 4);
		gain = (uint32_t)index_to_txbbgain(index);
		_pbus(1, 2, gain);
		if(index == 0) {
			sum_i = 0;
			sum_q = 0;
			for(pass = 0; pass < 4; pass++) {
				txdc_cal_pwdet(pair, log);
				sum_i = (sum_i + pair[0]) & 0xffff;
				sum_q = (sum_q + pair[1]) & 0xffff;
			}
			mp_arch_rf_set_u16(_PAR_PWDET, (uint16_t)((sum_i + 2) >> 2));
			mp_arch_rf_set_u16(_PAR_PWDET + 2, (uint16_t)((sum_q + 2) >> 2));
		}
		else {
			txdc_cal_pwdet(pair, log);
			mp_arch_rf_set_u16(_PAR_PWDET + 4, pair[0]);
			mp_arch_rf_set_u16(_PAR_PWDET + 6, pair[1]);
		}
	}
	if(keep != 0)
		return;
	flat[0] = 0x01000100;
	flat[1] = 0x01000100;
	pbus_set_dco(flat);
	MP_ARCH_RF_FN(0x88)();
	_tone(0, 128, _TONE_GAIN);
	pbus_workmode();
	set_txclk_en(0);
	mp_arch_rf_write(_REG_FE_MASK,
		(mp_arch_rf_read(_REG_FE_MASK) & 0xffffff00UL) | (saved_mask & 0xffUL));
	mp_arch_rf_write(_REG_FE_GATE,
		(mp_arch_rf_read(_REG_FE_GATE) & 0xfffff00fUL) | (saved_gate & 0xff0UL));
}

/**
 * Build the wifi or Bluetooth power table at channels 1, 6 and 11.
 * @param kind Zero selects wifi
 * @param atten Three result bytes
 * @param power One corrected power byte
 * @param dest Three bytes written by the servo
 */
void tx_pwctrl_init_cal_new(int kind, int8_t *atten, int8_t *power, uint8_t *dest) {
	uint8_t		target[1];
	int		sub;
	int		limit;
	int		add;
	uint32_t	tone;
	int		verbose;
	int		index;
	int		first;
	int		delta;

	target[0] = (uint8_t)(56 - phy_param[_PAR_BACKOFF]);
	if(kind == 0) {
		tone = 128;
		sub = 18;
		limit = 34;
		add = 40;
	}
	else {
		tone = phy_param[_PAR_BT_CODE];
		sub = 20;
		limit = 40;
		add = 18;
	}
	verbose = (int)(phy_param[_PAR_TXGAIN] & 1);
	if(kind != 0)
		MP_ARCH_RF_FN(0x64)(phy_param + _PAR_DCAP, 6);
	phy_param[_PAR_BUSY] = 1;
	for(index = 0; index < 3; index++) {
		set_channel_rfpll_freq(_chan[index], phy_param[_PAR_XTAL], 0);
		if(kind == 0)
			MP_ARCH_RF_FN(0x64)(phy_param + _PAR_DCAP, _chan[index]);
		rfcal_pwrctrl_new(tone, target, 1, add, dest + index,
			mp_arch_rf_u16(_PAR_POWER), (int)(int8_t)phy_param[_PAR_TEMP], verbose);
		atten[index] = (int8_t)dest[index];
		phy_param[_PAR_TEMP] = (uint8_t)((int)(int8_t)dest[index] + add);
	}
	first = (int)atten[0];
	if(first <= 11 || first > limit) {
		delta = (int)(uint8_t)get_data_sat(_i8(first - sub), 40, -40);
		power[0] = (int8_t)(target[0] + delta);
		atten[0] = (int8_t)((uint8_t)atten[0] - (uint8_t)delta);
		atten[1] = (int8_t)((uint8_t)atten[1] - (uint8_t)delta);
		atten[2] = (int8_t)((uint8_t)atten[2] - (uint8_t)delta);
	}
	else
		power[0] = (int8_t)target[0];
	phy_param[_PAR_BUSY] = 0;
}

/**
 * Calibrate the wifi power table and record channel 11.
 * @param unused Ignored
 */
void tx_pwctrl_init_new(int unused) {
	(void)unused;
	if(_flag_high(0x0b))
		return;
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_TXCAL_DBG)();
	set_channel_rfpll_freq(1, phy_param[_PAR_XTAL], 0);
	MP_ARCH_RF_FN(0x64)(phy_param + _PAR_DCAP, 1);
	pwdet_ref_code(80);
	tx_pwctrl_init_cal_new(0, (int8_t *)(phy_param + _PAR_WIFI_ATT),
		(int8_t *)(phy_param + _PAR_WIFI_PWR), phy_param + _PAR_WIFI_CAL);
	txcal_work_mode();
	_or_flag(0x100000);
	mp_arch_rf_set_u16(_PAR_CHAN, 11);
}

/**
 * Calibrate the Bluetooth power table and clear its spare halfword.
 */
void bt_tx_pwctrl_init_new(void) {
	uint32_t	saved;
	uint32_t	field;
	uint32_t	bb;
	uint32_t	index;

	if(_flag_high(0x10))
		return;
	saved = mp_arch_rf_reg_read(MP_ARCH_RF_HOST_FILTER, 1, 28);
	field = mp_arch_rf_reg_read_mask(MP_ARCH_RF_HOST_FILTER, 1, 30, 5, 0);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 28, 2);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 29, 2);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 30, 5, 0, 2);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 31, 5, 0, 2);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_TXCAL_DBG)();
	bb = phy_param[_PAR_BT_BB];
	_pbus(5, 1, bb + 0x1c0);
	_pbus(1, 2, 0);
	_pbus(1, 1, _pbus_half(1, 1) | 2U);
	_pbus(4, 2, (uint32_t)phy_param[_PAR_BT_DIG] << 3);
	index = (uint32_t)(uint8_t)bt_bb_to_index(0);
	pbus_set_dco((uintptr_t)(phy_param + _PAR_BT_DC) + index * 8);
	tx_pwctrl_init_cal_new(1, (int8_t *)(phy_param + _PAR_BT_STEP),
		(int8_t *)(phy_param + _PAR_BT_PWR), phy_param + _PAR_BT_ATT);
	mp_arch_rf_set_u16(_PAR_BT_ZERO, 0);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 28, saved);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 29, saved);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 30, 5, 0, field);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 31, 5, 0, field);
	txcal_work_mode();
	_or_flag(0x8000);
}

/** @} */
