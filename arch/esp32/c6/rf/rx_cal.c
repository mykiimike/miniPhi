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

Measure the receive DC and image codes and store them in phy_param.

@{
*/

	#define _REG_FE     0x600A0450UL
	#define _REG_IQ_A   0x600A0470UL
	#define _REG_IQ_B   0x600A0474UL
	#define _REG_MIS_I  0x600A0478UL
	#define _REG_MIS_Q  0x600A047CUL
	#define _REG_MIS_C  0x600A0480UL
	#define _REG_MIS_D  0x600A0484UL
	#define _REG_EST_I  0x600A0488UL
	#define _REG_EST_Q  0x600A048CUL
	#define _REG_PWR    0x600A0490UL
	#define _REG_BUSY   0x600A04A0UL
	#define _REG_GATE   0x600A0904UL
	#define _REG_AGC    0x600A702CUL
	#define _REG_LEVEL  0x600A708CUL

	#define _PAR_TXGAIN 0x10
	#define _PAR_IQ     0xd0
	#define _PAR_COUNT  0x1a2
	#define _PAR_SAT    0x1a4

	#define _DCO_FLAT   0x01000100UL
	#define _DCO_HALF   0x00010001UL
	#define _CODE_MAX   511
	#define _LEVEL_MAX  69

static const uint16_t _code8[8] = {
	0x0040, 0x0041, 0x0043, 0x006b, 0x0077, 0x0079, 0x007b, 0x007f
};
static const uint16_t _code7[7] = {
	0x0041, 0x0043, 0x006b, 0x0077, 0x0079, 0x007b, 0x007f
};
static const uint16_t _chan[7] = { 1, 3, 5, 7, 9, 11, 13 };
static const uint16_t _bb[7] = {
	0x0000, 0x0020, 0x0030, 0x0038, 0x003c, 0x003e, 0x003f
};

static void _pbus(uint32_t sel, uint32_t part, uint32_t value) {
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS)(sel, part, value);
}

static uint32_t _pbus_rd(uint32_t sel, uint32_t part) {
	return((uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS_RD)(sel, part));
}

static void _tone(uint32_t enable, uint32_t freq, uint32_t gain) {
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_TX_TONE)(enable, freq, gain, 0, 0, 0);
}

static void _mix(unsigned long addr, unsigned long mask, unsigned long bits) {
	mp_arch_rf_write(addr, (mp_arch_rf_read(addr) & mask) | bits);
}

static int _i16(int value) {
	return((int)(int16_t)value);
}

static int _sdiv(int num, int den) {
	if(den == 0)
		return(-1);
	return(num / den);
}

static int _clamp_code(int value) {
	value = _i16(value);
	if(value > _CODE_MAX)
		value = _CODE_MAX;
	if(value < 0)
		value = 0;
	return(value);
}

/**
 * Pulse the forced receive-gain trigger when the gate bit is set.
 */
void phy_force_rx_gain_trig(void) {
	unsigned long word;

	if((mp_arch_rf_read(_REG_GATE) & 1UL) == 0)
		return;
	word = mp_arch_rf_read(_REG_AGC);
	mp_arch_rf_write(_REG_AGC, (word & 0x00ffffffUL) | 0x46000000UL);
	word = mp_arch_rf_read(_REG_AGC);
	mp_arch_rf_write(_REG_AGC, word | 0x00800000UL);
	ets_delay_us(1);
	word = mp_arch_rf_read(_REG_AGC);
	mp_arch_rf_write(_REG_AGC, word & 0xff7fffffUL);
}

/**
 * Arm the IQ estimator and count level hits while it is busy.
 * @param unused Ignored
 * @param length Sample length written into the estimator
 */
void ram_iq_est_enable(uint32_t unused, uint32_t length) {
	unsigned long word;
	uint16_t count;
	int dirty;
	uint32_t level;

	(void)unused;
	mp_arch_rf_set_u16(_PAR_COUNT, 0);
	_mix(_REG_IQ_A, 0xf3ffffffUL, 0x04000000UL);
	_mix(_REG_IQ_B, 0xffe7ffffUL, 0x00100000UL);
	_mix(_REG_IQ_B, 0xfffe0003UL, ((unsigned long)length << 2) & 0x1fffcUL);
	word = mp_arch_rf_read(_REG_IQ_B);
	mp_arch_rf_write(_REG_IQ_B, word | 1UL);
	ets_delay_us(1);
	word = mp_arch_rf_read(_REG_IQ_B);
	mp_arch_rf_write(_REG_IQ_B, word | 2UL);
	phy_force_rx_gain_trig();
	count = mp_arch_rf_u16(_PAR_COUNT);
	dirty = 0;
	for(;;) {
		if((mp_arch_rf_read(_REG_BUSY) & 0x10000UL) != 0)
			break;
		level = (mp_arch_rf_read(_REG_LEVEL) >> 12) & 127U;
		if(level > _LEVEL_MAX)
			continue;
		count = (uint16_t)(count + 1U);
		dirty = 1;
	}
	if(dirty != 0)
		mp_arch_rf_set_u16(_PAR_COUNT, count);
}

/**
 * Estimate DC and power with a selectable accumulator shift.
 * @param enable Passed to the estimator enable
 * @param count Denominator, before the added one
 * @param out Three words: I, Q and the power code
 * @param scale Non-zero selects the shorter accumulator shift
 */
void dc_iq_est_new(uint32_t enable, int count, int *out, int scale) {
	int shift;
	int den;
	int i_est;
	int q_est;
	int power;
	int noise;
	int db;

	shift = scale != 0 ? 4 : 6;
	ram_iq_est_enable(enable, (uint32_t)count);
	den = count + 1;
	i_est = _sdiv((int)((int32_t)mp_arch_rf_read(_REG_EST_I) >> shift), den);
	q_est = _sdiv((int)((int32_t)mp_arch_rf_read(_REG_EST_Q) >> shift), den);
	power = _sdiv((int)(int32_t)mp_arch_rf_read(_REG_PWR), den);
	out[0] = i_est;
	out[1] = q_est;
	noise = (int)((uint32_t)i_est * (uint32_t)i_est + (uint32_t)q_est * (uint32_t)q_est);
	if(scale != 0)
		noise >>= 4;
	noise = power * 8 - noise;
	if(noise < 0)
		noise = 0;
	db = (int)linear_to_db(noise, 0);
	db = (db + 8) >> 4;
	out[2] = db;
	MP_ARCH_RF_FN(0xa4)(db);
}

/**
 * Estimate DC and power with the long accumulator shift.
 * @param enable Passed to the estimator
 * @param count Denominator, before the added one
 * @param out Three words: I, Q and the unscaled power sum
 */
void dc_iq_est_high(uint32_t enable, int count, int *out) {
	int den;
	int i_est;
	int q_est;
	int power;
	int noise;
	int db;

	MP_ARCH_RF_FN(0xa0)(enable, count, out);
	/* Signed accumulators select the matched PA capacitor and the gain-63
	 * transmit step then browns out the board, so the sums are read
	 * unsigned until that is solved.
	 */
	den = count + 1;
	i_est = (int)((mp_arch_rf_read(_REG_EST_I) >> 6) / (unsigned)den);
	q_est = (int)((mp_arch_rf_read(_REG_EST_Q) >> 6) / (unsigned)den);
	power = (int)(mp_arch_rf_read(_REG_PWR) / (unsigned)den);
	out[0] = i_est;
	out[1] = q_est;
	noise = (power << 3) - (int)((uint32_t)i_est * (uint32_t)i_est
		+ (uint32_t)q_est * (uint32_t)q_est);
	if(noise < 0)
		noise = 0;
	db = (int)linear_to_db(noise, 0) + 8;
	out[2] = db;
	MP_ARCH_RF_FN(0xa4)(db);
}

/**
 * Read one IQ mismatch pair.
 * @param shift Right-shift applied to the correlators
 * @param out Two bytes, I then Q
 * @param verbose Non-zero prints the power and the pair
 */
void rxiq_get_mis_new(uint32_t shift, uint8_t *out, int verbose) {
	int sh;
	int32_t a;
	int32_t b;
	int32_t c;
	int32_t d;
	int32_t sum_i;
	int32_t diff_i;
	int32_t sum_q;
	int32_t diff_q;
	int64_t mag;
	int64_t cross;
	int64_t other;
	int i_code;
	int q_code;

	MP_ARCH_RF_FN(0xa0)(1, (uint32_t)(uint16_t)(1U << (shift & 31U)), verbose);
	sh = ((int)shift - 2) & 31;
	a = (int32_t)mp_arch_rf_read(_REG_MIS_I) >> sh;
	d = (int32_t)mp_arch_rf_read(_REG_MIS_D) >> sh;
	c = (int32_t)mp_arch_rf_read(_REG_MIS_C) >> sh;
	b = (int32_t)mp_arch_rf_read(_REG_MIS_Q) >> sh;
	diff_i = a - d;
	sum_q = c + b;
	sum_i = a + d;
	diff_q = b - c;
	mag = (int64_t)diff_q * diff_q + (int64_t)sum_i * sum_i;
	if(mag == 0)
		mag = 1;
	cross = (int64_t)sum_i * diff_i - (int64_t)diff_q * sum_q;
	other = (int64_t)sum_i * sum_q + (int64_t)diff_q * diff_i;
	i_code = (int)(int8_t)(uint32_t)((int64_t)((uint64_t)cross << 9) / mag);
	q_code = (int)(int8_t)(uint32_t)((int64_t)((uint64_t)other << 10) / mag);
	i_code = (i_code + 1) >> 1;
	q_code = (q_code + 1) >> 1;
	out[0] = (uint8_t)i_code;
	out[1] = (uint8_t)q_code;
	if(verbose != 0) {
		sh = ((int)shift - 3) & 31;
		phy_printf("%d, %d-%d, ",
			(int)((int32_t)mp_arch_rf_read(_REG_PWR) >> sh), i_code, q_code);
	}
	MP_ARCH_RF_FN(0xa4)(q_code);
}

/**
 * Cover the image with two gain-mismatch passes.
 * @param shift Correlator shift
 * @param i_out I code
 * @param q_out Q code
 * @param verbose Forwarded to the mismatch read
 */
void rxiq_cover_mg_mp_new(uint32_t shift, int8_t *i_out, int8_t *q_out, uint32_t verbose) {
	int i_code;
	int q_code;
	int pass;
	uint8_t pair[2];

	i_code = 0;
	q_code = 0;
	for(pass = 0; pass < 2; pass++) {
		i_code = (int)rxiq_set_reg(i_code, 1);
		q_code = (int)rxiq_set_reg(q_code, 0);
		rxiq_get_mis_new(shift, pair, (int)verbose);
		i_code = (int)(int8_t)(uint8_t)(pair[0] + i_code);
		q_code = (int)(int8_t)(uint8_t)(q_code - pair[1]);
	}
	i_code = (int)(int8_t)get_data_sat(i_code, 31, -31);
	q_code = (int)(int8_t)get_data_sat(q_code, 63, -63);
	rxiq_set_reg(i_code, 1);
	rxiq_set_reg(q_code, 0);
	*i_out = (int8_t)i_code;
	*q_out = (int8_t)q_code;
}

/**
 * Measure one receive image pair at a tone.
 * @param shift Correlator shift
 * @param tone Tone frequency
 * @param gain Tone gain
 * @param out Two result bytes
 * @param verbose Non-zero prints the cover
 */
void rfcal_rxiq_new(uint32_t shift, uint32_t tone, uint32_t gain, uint8_t *out,
	uint32_t verbose) {
	int8_t pair_i;
	int8_t pair_q;

	mp_arch_rf_set(_REG_FE, 0x20000000UL);
	mp_arch_rf_clr(_REG_FE, 0x40000000UL);
	_tone(1, tone, gain);
	rxiq_cover_mg_mp_new(shift, &pair_i, &pair_q, verbose);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_STOP_TONE)(1);
	out[0] = (uint8_t)pair_i;
	out[1] = (uint8_t)pair_q;
}

/**
 * Average up to four image measurements.
 * @param tone Tone frequency
 * @param gain Tone gain
 * @param verbose Non-zero prints each sample
 * @returns Packed Q in the low byte and I in the next byte
 */
uint32_t get_rfcal_rxiq_data_new(uint32_t tone, uint32_t gain, int verbose) {
	int round;
	int prev_i;
	int prev_q;
	int sum_i;
	int sum_q;
	int cur_i;
	int cur_q;
	int i_code;
	int q_code;
	uint8_t pair[2];

	round = 0;
	prev_i = 0;
	prev_q = 0;
	sum_i = 0;
	sum_q = 0;
	i_code = 0;
	q_code = 0;
	for(;;) {
		rfcal_rxiq_new(14, tone, gain, pair, (uint32_t)verbose);
		cur_i = (int)(int8_t)pair[0];
		cur_q = (int)(int8_t)pair[1];
		if(verbose != 0)
			phy_printf("%d_%d_%d\n", round, cur_i, cur_q);
		if(round != 0 && abs_temp(prev_i - cur_i) <= 1 && abs_temp(prev_q - cur_q) <= 1) {
			i_code = (int)(int8_t)((cur_i + prev_i + 1) >> 1);
			q_code = (cur_q + prev_q + 1) >> 1;
			break;
		}
		prev_i = cur_i;
		prev_q = cur_q;
		round++;
		sum_i = _i16(sum_i + cur_i);
		sum_q = _i16(sum_q + cur_q);
		if(round == 4) {
			i_code = (int)(int8_t)((sum_i + 2) >> 2);
			q_code = (sum_q + 2) >> 2;
			break;
		}
	}
	i_code = (int)get_data_sat((int)(int8_t)i_code, 31, -31);
	q_code = (int)get_data_sat((int)(int8_t)q_code, 63, -63);
	return((uint32_t)((q_code & 0xff) | ((i_code & 0xff) << 8)) & 0xffffU);
}

/**
 * Walk a DC pair until both components sit inside the bit-count limit.
 * @param samples Estimator length
 * @param row DC pair, and the following pair when the index is not zero
 * @param delay Microseconds between writes
 * @param wide Non-zero uses the wider step limit
 * @param verbose Non-zero prints each step
 * @param passes Part index minus one
 */
void pbus_rx_dco_cal_new(uint32_t samples, int16_t *row, uint32_t delay, int wide,
	int verbose, uint32_t passes) {
	uint32_t read;
	uint32_t full;
	uint32_t bits;
	uint32_t high;
	uint8_t part;
	int limit;
	int index;
	int rounds;
	int step;
	int i_code;
	int q_code;
	int prev_i;
	int prev_q;
	int est[4];
	int16_t *i_at;
	int16_t *q_at;

	full = _pbus_rd(1, 2);
	high = (full >> 6) & 0xffU;
	bits = 0;
	read = full & 63U;
	while(read != 0) {
		bits = (uint8_t)(bits + (read & 1U));
		read >>= 1;
	}
	_pbus(2, 1, (uint16_t)row[0]);
	_pbus(3, 1, (uint16_t)row[1]);
	_pbus(2, 2, 256);
	_pbus(3, 2, 256);
	if(wide == 0) {
		limit = (int)(bits << 1);
		if(bits > 2)
			limit = (int)((bits + 1U) << 1);
	}
	else if(bits == 0)
		limit = 10;
	else
		limit = (int)(bits * 20U);
	limit = (int)(uint8_t)limit;
	part = (uint8_t)(passes + 1U);
	for(;;) {
		index = (int)(uint8_t)(part - 1U);
		if(index == 0) {
			i_code = row[0];
			q_code = row[1];
			rounds = 12;
		}
		else {
			i_code = row[2];
			q_code = row[3];
			rounds = 8;
		}
		i_at = row + index * 2;
		q_at = i_at + 1;
		prev_i = 0;
		prev_q = 0;
		for(step = 0; step != rounds; step = (int)(uint8_t)(step + 1)) {
			int abs_i;
			int corr;

			*i_at = (int16_t)i_code;
			*q_at = (int16_t)q_code;
			_pbus(2, part, (uint16_t)*i_at);
			_pbus(3, part, (uint16_t)*q_at);
			ets_delay_us(delay);
			dc_iq_est(1, samples, est);
			if(verbose != 0) {
				phy_printf(" (%d,%d) ", (int)*i_at, (int)*q_at);
				phy_printf("%d,%d ", est[0], est[1]);
			}
			abs_i = (int)abs_temp(est[0]);
			if(abs_i <= limit && (int)abs_temp(est[1]) <= limit)
				break;
			if(step == 0) {
				prev_i = (int)(int16_t)est[0];
				prev_q = (int)(int16_t)est[1];
			}
			if(limit < (int)abs_temp(est[0])) {
				corr = (int)get_dco_comp(index == 0, wide, (int)(int16_t)est[0],
					prev_i, bits, high);
				i_code = _i16(i_code - corr);
			}
			if(limit < (int)abs_temp(est[1])) {
				corr = (int)get_dco_comp(index == 0, wide, (int)(int16_t)est[1],
					prev_q, bits, high);
				q_code = _i16(q_code - corr);
			}
			i_code = _i16((int)get_data_sat(i_code, 500, 20));
			q_code = _i16((int)get_data_sat(q_code, 500, 20));
			prev_i = (int)(int16_t)est[0];
			prev_q = (int)(int16_t)est[1];
		}
		*i_at = (int16_t)i_code;
		*q_at = (int16_t)q_code;
		if(verbose != 0) {
			phy_printf("stage %d: bb=0x%x (%d,%d) %d; ",
				index + 1, full, est[0], est[1], step);
			phy_printf("\n");
		}
		if(passes < part)
			break;
		part = (uint8_t)(part + 1U);
	}
	if(verbose != 0)
		phy_printf("\n");
}

/**
 * Keep the smallest DC estimate that the saturation flag accepts.
 * @param samples Estimator length
 * @param unused Ignored
 * @param out Three words, replaced when a smaller power is accepted
 * @param scale Forwarded to the estimator
 */
void rxdc_est_min_new(uint32_t samples, uint32_t unused, int *out, uint32_t scale) {
	int best;
	int tries;
	int est[3];

	(void)unused;
	best = 100;
	tries = 0;
	for(;;) {
		dc_iq_est_new(1, (int)samples, est, (int)scale);
		if(est[2] < best && (mp_arch_rf_u16(_PAR_COUNT) == 0 || phy_param[_PAR_SAT] == 1)) {
			out[0] = est[0];
			out[1] = est[1];
			out[2] = est[2];
			best = est[2];
		}
		if(best <= 47) {
			if(tries > 1)
				return;
			if(best <= 35)
				return;
		}
		tries = (int)(uint8_t)(tries + 1);
		if(tries == 8) {
			out[2] = 56;
			return;
		}
	}
}

static int _nudge(int alt, int sample, int shift) {
	int step;

	if((int)abs_temp(alt) > 49)
		step = _i16(alt >> shift);
	else if(sample > 0)
		step = 1;
	else if(sample != 0)
		step = -1;
	else
		step = 0;
	return(_i16(step));
}

/**
 * One binary-search step of the receive DC servo.
 * @param wide Non-zero raises the difference threshold
 * @param stage One uses the direct estimate, two clamps the step
 * @param samples Estimator length
 * @param row DC pair updated in place
 * @param flag Set when the pair is already inside the window
 */
void pbus_rx_dco_cal_1step_new(int wide, int stage, uint32_t samples,
	uint16_t *row, int8_t *flag) {
	int verbose;
	int limit;
	int bits;
	int shift_n;
	int thresh;
	int part;
	int orig_i;
	int orig_q;
	int i_code;
	int q_code;
	int round;
	uint32_t word;
	int sh;

	verbose = (int)((mp_arch_rf_u16(_PAR_TXGAIN) >> 3) & 1U);
	limit = stage == 1 ? 8 : 12;
	orig_i = (int)(int16_t)row[0];
	orig_q = (int)(int16_t)row[1];
	i_code = orig_i;
	q_code = orig_q;
	word = (uint8_t)_pbus_rd(1, 2);
	bits = 0;
	for(sh = 0; sh < 6; sh++)
		bits = (int)(uint8_t)(bits + (int)((word >> sh) & 1U));
	if(stage == 1) {
		thresh = bits < 2 ? 2 : bits;
		thresh = (int)(uint8_t)(thresh - 1);
		shift_n = bits < 2 ? 2 : bits;
		shift_n = (int)(uint8_t)(shift_n - 2);
	}
	else {
		thresh = wide != 0 ? 24 : 4;
		shift_n = wide != 0 ? 5 : 2;
	}
	*flag = 0;
	part = stage == 1 ? 2 : 1;
	for(round = 0; (uint8_t)round < limit; round++) {
		int est[3];
		int second[3];
		int i_sample;
		int q_sample;
		int power;
		int i_step;
		int q_step;
		int res_i;
		int res_q;
		int res_p;
		int sec_i;
		int sec_q;
		int sec_p;
		uint16_t i_snap;

		i_snap = (uint16_t)i_code;
		row[0] = i_snap;
		row[1] = (uint16_t)q_code;
		_pbus(2, (uint32_t)part, row[0]);
		_pbus(3, (uint32_t)part, row[1]);
		res_i = 0;
		res_q = 0;
		res_p = 0;
		sec_i = 0;
		sec_q = 0;
		sec_p = 0;
		if(stage == 1) {
			ets_delay_us(10);
			rxdc_est_min_new(samples, 1, est, 0);
			i_sample = est[0];
			q_sample = est[1];
			power = est[2];
		}
		else {
			_pbus(1, 2, 0);
			ets_delay_us(10);
			rxdc_est_min_new(samples, 1, est, 1);
			_pbus(1, 2, 32);
			ets_delay_us(10);
			rxdc_est_min_new(samples, 1, second, 1);
			res_i = est[0];
			res_q = est[1];
			res_p = est[2];
			sec_i = second[0];
			sec_q = second[1];
			sec_p = second[2];
			i_sample = second[0] - est[0];
			q_sample = second[1] - est[1];
			power = second[2] > est[2] ? second[2] : est[2];
		}
		i_step = (int)abs_temp(i_sample) >= thresh ? _i16(i_sample >> shift_n) : 0;
		q_step = (int)abs_temp(q_sample) >= thresh ? _i16(q_sample >> shift_n) : 0;
		if(i_step == 0)
			i_step = _nudge(res_i, i_sample, shift_n);
		if(q_step == 0)
			q_step = _nudge(res_q, q_sample, shift_n);
		if(stage != 1) {
			if(power > 44) {
				i_step = 0;
				q_step = 0;
			}
			else if((int)abs_temp(i_sample) <= 9)
				abs_temp(q_sample);
			if(stage == 2) {
				i_step = _i16((int)get_data_sat(i_step, 5, -5));
				q_step = _i16((int)get_data_sat(q_step, 5, -5));
			}
		}
		if(verbose != 0) {
			uint32_t rfrx;
			uint32_t bb2;
			uint32_t dco_i;
			uint32_t dco_q;
			uint32_t dco_i2;
			uint32_t dco_q2;

			rfrx = _pbus_rd(0, 1);
			bb2 = _pbus_rd(1, 2);
			dco_i = _pbus_rd(2, 1);
			dco_q = _pbus_rd(3, 1);
			dco_i2 = _pbus_rd(2, 2);
			dco_q2 = _pbus_rd(3, 2);
			phy_printf("stage=%d,rfrx=0x%x,bb2=0x%x; i=%d, dco: %d,%d,%d,%d; "
				"res dc: %d,%d,%d; %d,%d,%d; %d,%d;,scale=%d,%d\n",
				stage == 1, rfrx, bb2, round,
				dco_i, dco_q, dco_i2, dco_q2,
				res_i, res_q, res_p, sec_i, sec_q, sec_p,
				i_sample, q_sample, i_step, q_step);
		}
		if((int)abs_temp(i_sample) <= thresh && (int)abs_temp(q_sample) <= thresh
			&& power <= 45) {
			*flag = 1;
			break;
		}
		if((int)abs_temp(i_sample) > thresh)
			i_code = _i16((int)i_snap - i_step);
		if((int)abs_temp(q_sample) > thresh)
			q_code = _i16(q_code - q_step);
		i_code = _clamp_code(i_code);
		q_code = _clamp_code(q_code);
	}
	if(*flag == 0 && stage != 1) {
		row[0] = (uint16_t)orig_i;
		row[1] = (uint16_t)orig_q;
	}
	if((int)(int16_t)row[0] < 0)
		row[0] = 0;
	else if((int)(int16_t)row[0] > _CODE_MAX)
		row[0] = _CODE_MAX;
	if((int)(int16_t)row[1] < 0)
		row[1] = 0;
	else if((int)(int16_t)row[1] > _CODE_MAX)
		row[1] = _CODE_MAX;
	_pbus(2, (uint32_t)part, row[0]);
	_pbus(3, (uint32_t)part, row[1]);
}

/**
 * Split a packed IQ word into two signed bytes.
 * @param out Two bytes, magnitude then the low code
 * @param sample Packed word
 */
void get_iq_value(uint8_t *out, uint32_t sample) {
	int high;
	int low;

	high = (int)(int8_t)(sample >> 7);
	low = (int)(int8_t)sample;
	high &= 63;
	if(((sample >> 7) & 0x20U) != 0)
		high -= 64;
	low &= 127;
	out[0] = (uint8_t)high;
	if((sample & 0x40U) != 0)
		low -= 128;
	out[1] = (uint8_t)low;
}

/**
 * Program the loopback transmit image from a packed code.
 * @param packed Code passed through get_iq_value()
 */
void set_lb_txiq_new(uint32_t packed) {
	uint8_t pair[2];

	get_iq_value(pair, packed);
	txiq_set_reg((int)(int8_t)pair[0], 1);
	txiq_set_reg((int)(int8_t)(uint8_t)(pair[1] - 4U), 0);
}

/**
 * Search the tone gain that lands the receive power in range, then measure the image.
 * @param wide Non-zero saves and clears the filter image bit
 * @param tone Tone frequency
 * @param out Packed image code
 * @param verbose Non-zero prints the search
 */
void set_rx_gain_cal_iq_new(int wide, uint32_t tone, uint16_t *out, int verbose) {
	uint32_t seed[2];
	uint32_t saved;
	int log;
	int bb;
	int atten;
	int tries;
	int power;

	seed[0] = _DCO_HALF;
	seed[1] = _DCO_HALF;
	log = (int)((mp_arch_rf_u16(_PAR_TXGAIN) >> 3) & 1U);
	saved = 0;
	if(wide != 0) {
		saved = mp_arch_rf_reg_read_mask(MP_ARCH_RF_HOST_FILTER, 1, 3, 2, 2);
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 3, 2, 2, 0);
	}
	seed[0] = _DCO_FLAT;
	bb = 32;
	atten = 48;
	tries = 2;
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_LOOPBACK)(0, 67, bb);
	set_lb_txiq_new(mp_arch_rf_u16(_PAR_IQ));
	pbus_rx_dco_cal(4000, seed, 10, 0, log);
	for(;;) {
		_pbus(1, 1, 0);
		_pbus(1, 1, 505);
		_tone(1, tone, (uint32_t)(uint8_t)atten);
		MP_ARCH_RF_FN(0xa0)(1, 1023);
		power = (int)((int32_t)mp_arch_rf_read(_REG_PWR) >> 7);
		if(verbose != 0) {
			uint32_t rftx;
			uint32_t bb_rd;
			uint32_t rfrx;

			rftx = _pbus_rd(5, 1);
			bb_rd = _pbus_rd(1, 2);
			rfrx = _pbus_rd(0, 2);
			phy_printf("total_pwr=%ld, min=%ld, max=%ld, rftx=0x%x, bb=0x%x, rfrx2=%x,att=%d, dc_i=%d, dc_q=%d\n",
				power, 0x1000, 0x20000, rftx, bb_rd, rfrx, atten,
				(int)((int32_t)mp_arch_rf_read(_REG_EST_I) >> 16),
				(int)((int32_t)mp_arch_rf_read(_REG_EST_Q) >> 16));
		}
		MP_ARCH_RF_FN(0xa4)();
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_STOP_TONE)(1);
		if(power > 0x20000) {
			if(bb != 0)
				bb = 0;
			else
				atten += 24;
		}
		else if(power >= 0x1000)
			break;
		else
			atten -= 24;
		atten = _i16(atten);
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_LOOPBACK)(0, 67, bb);
		pbus_rx_dco_cal(4000, seed, 10, 0, log);
		if(atten > 120)
			atten = 120;
		atten = _i16(atten);
		if(atten < 0)
			atten = 0;
		if(tries == 1)
			break;
		tries = 1;
	}
	if(verbose != 0)
		phy_printf("rxiq: rftx=0x%x, rfrx=x%x, att=%d, bb=0x%x\n", 0, 0x104, atten, bb);
	*out = (uint16_t)get_rfcal_rxiq_data_new(tone, (uint32_t)(uint8_t)atten, verbose);
	if(wide != 0)
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 3, 2, 2, saved);
}

/**
 * Copy the nearest measured DC pair onto channels that have none.
 * @param pairs Seven I/Q pairs
 * @param flags One byte per channel, zero where a pair is missing
 */
void rx_chan_dc_sort(int16_t *pairs, uint8_t *flags) {
	int count;
	int mark;
	int index;
	int other;
	int best;
	int dist;
	int gap;

	count = 0;
	for(index = 0; index < 7; index++) {
		if(flags[index] == 1)
			count = (int)(uint8_t)(count + 1);
	}
	mark = (count == 0) ? 2 : 1;
	for(index = 0; index < 7; index++) {
		if(flags[index] != 0)
			continue;
		best = (int)(int8_t)index;
		dist = 20;
		for(other = 0; other < 7; other++) {
			if(flags[other] != (uint8_t)mark)
				continue;
			gap = (int)(int8_t)abs_temp(index - other);
			if(gap < dist) {
				best = (int)(int8_t)other;
				dist = gap;
			}
		}
		pairs[index * 2] = pairs[best * 2];
		pairs[index * 2 + 1] = pairs[best * 2 + 1];
	}
}

/**
 * Sum the two receive-image magnitudes.
 * @returns The 64-bit sum of the squared sums and differences
 */
int64_t phy_corr_get_pwr(void) {
	int32_t i_lo;
	int32_t q_lo;
	int32_t i_hi;
	int32_t q_hi;
	int32_t sum_a;
	int32_t diff_b;
	int32_t diff_a;
	int32_t sum_b;

	i_lo = (int32_t)mp_arch_rf_read(_REG_MIS_I) >> 12;
	q_lo = (int32_t)mp_arch_rf_read(_REG_MIS_Q) >> 12;
	i_hi = (int32_t)mp_arch_rf_read(_REG_MIS_C) >> 12;
	q_hi = (int32_t)mp_arch_rf_read(_REG_MIS_D) >> 12;
	sum_a = i_lo + q_hi;
	diff_b = q_lo - i_hi;
	diff_a = i_lo - q_hi;
	sum_b = q_lo + i_hi;
	return((int64_t)sum_a * sum_a + (int64_t)diff_b * diff_b
		+ (int64_t)diff_a * diff_a + (int64_t)sum_b * sum_b);
}

/**
 * Calibrate receive DC across the channel list.
 * @param kind Zero walks one channel, one walks the seven wifi channels
 * @param wide Non-zero uses the short code list and the filter image bit
 * @param dc Destination pairs for kind zero
 * @param bb Baseband pair written on the first narrow channel
 * @param wide_out Destination pair for a non-zero kind, one pair per channel
 */
void set_rx_gain_cal_dc_new(int kind, int wide, uint16_t *dc, uint16_t *bb,
	uint16_t *wide_out) {
	uint16_t dco[8];
	int8_t flag;
	int inner;
	int outer;
	int stage;
	int ch;
	int step;
	uint16_t code;

	for(step = 0; step < 8; step++)
		dco[step] = 0x0100;
	inner = wide == 0 ? 8 : 7;
	outer = 1;
	if(kind != 0) {
		if(kind == 1)
			inner = 1;
		outer = 7;
	}
	stage = kind == 1 ? 2 : 0;
	for(ch = 0; (uint8_t)ch < outer; ch++) {
		if(kind == 0)
			phy_chan_ana(14);
		else
			phy_chan_ana(_chan[ch]);
		pbus_debugmode();
		MP_ARCH_RF_FN(0x88)();
		MP_ARCH_RF_FN(0x80)();
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
		set_rxclk_en(1);
		set_txclk_en(1);
		if(wide != 0)
			mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 3, 2, 2, 0);
		if(kind == 1) {
			dco[0] = dc[4];
			dco[1] = dc[5];
		}
		else if(wide == 0 && kind == 0 && ch == 0) {
			MP_ARCH_RF_FN(0x7c)(0x43000);
			rx_pbus_reset(0);
			_pbus(2, 2, 256);
			_pbus(3, 2, 256);
			pbus_rx_dco_cal_1step_new(0, 0, 0x800, dco, &flag);
			pbus_rx_dco_cal_1step_new(0, 1, 0x800, dco + 4, &flag);
			bb[0] = dco[4];
			bb[1] = dco[5];
		}
		for(step = 0; (uint8_t)step < inner; step++) {
			if(wide != 0)
				code = _code7[step];
			else if(kind != 0)
				code = 0x7f;
			else
				code = _code8[step];
			MP_ARCH_RF_FN(0x7c)((uint32_t)code << 12);
			rx_pbus_reset(wide);
			if(wide == 1) {
				_pbus(2, 2, 256);
				_pbus(3, 2, 256);
			}
			else {
				_pbus(2, 2, bb[0]);
				_pbus(3, 2, bb[1]);
			}
			pbus_rx_dco_cal_1step_new(wide, stage, 0x800, dco, &flag);
			if(kind == 0) {
				dc[step * 2] = dco[0];
				dc[step * 2 + 1] = dco[1];
			}
			else {
				wide_out[0] = dco[0];
				wide_out[1] = dco[1];
			}
		}
		if(wide != 0)
			mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_FILTER, 1, 3, 2, 2, 1);
		set_rxclk_en(0);
		set_txclk_en(0);
		wide_out += 2;
		MP_ARCH_RF_FN(0x80)();
		pbus_workmode();
	}
}

/**
 * Find a baseband gain code in the seven-entry table.
 * @param code Gain code
 * @returns The matching index, or 7 when the code is absent
 */
uint32_t ram_bb_gain_index(uint32_t code) {
	uint32_t index;

	for(index = 0; index < 7; index++) {
		if(_bb[index] == code)
			return(index);
	}
	return(7);
}

/**
 * Find a receive code in the narrow or wide list.
 * @param bank Zero selects the eight-entry list
 * @param code Code to find
 * @returns The matching index, or the list length when the code is absent
 */
uint32_t rfrx_gain_index_new(int bank, uint32_t code) {
	const uint16_t *tab;
	uint32_t limit;
	uint32_t index;

	if(bank == 0) {
		tab = _code8;
		limit = 8;
	}
	else {
		tab = _code7;
		limit = 7;
	}
	for(index = 0; index < limit; index++) {
		if(tab[index] == code)
			return(index);
	}
	return(limit);
}

/**
 * Count receive-level hits and mark the image when any land in range.
 */
void phy_check_rx_sat(void) {
	uint32_t seed[2];
	uint16_t left;
	uint16_t hits;
	uint32_t level;

	seed[0] = _DCO_HALF;
	seed[1] = _DCO_HALF;
	pbus_debugmode();
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_XPD_RX)(0);
	pbus_set_dco(seed);
	phy_force_rx_gain_trig();
	ets_delay_us(5);
	hits = 0;
	for(left = 100; left != 0; left = (uint16_t)(left - 1U)) {
		level = (mp_arch_rf_read(_REG_LEVEL) >> 12) & 127U;
		if(level <= _LEVEL_MAX)
			hits = (uint16_t)(hits + 1U);
	}
	if(hits != 0)
		phy_param[_PAR_SAT] = 1;
	pbus_workmode();
}

/** @} */
