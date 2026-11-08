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

Feature queries: target power, CCA, noise, the 11p filter and register backup.

@{
*/

#define _REG_CLK     0x600A981CUL
#define _REG_CLK_EN  0x600A9814UL
#define _REG_SYS_A   0x600A9804UL
#define _REG_SYS_B   0x600A980CUL
#define _REG_FE      0x600A0914UL
#define _REG_BB      0x600A7030UL
#define _REG_CCA     0x600A701CUL
#define _REG_NOISE   0x600A7050UL
#define _REG_RSSI    0x600A706CUL
#define _REG_CCA_CTL 0x600A7C58UL
#define _REG_CCA_A   0x600A7C5CUL
#define _REG_CCA_B   0x600A7C60UL
#define _REG_PMU_A   0x600B0154UL
#define _REG_PMU_B   0x600B00CCUL

#define _PAR_TPW    0x06
#define _PAR_RC_RAW 0xe8
#define _PAR_RC_A   0xe9
#define _PAR_RC_B   0xea
#define _PAR_11P_A  0x28
#define _PAR_11P_B  0x29
#define _PAR_CHAN   0x11c
#define _PAR_CHAN_B 0x11f
#define _PAR_TAB_A  0x52
#define _PAR_RATE   0x66
#define _PAR_TAB_B  0x67
#define _PAR_TAB_C  0x70


/**
 * Write one filter register.
 * @param reg Register index
 * @param data Byte to store
 */
static void _filt(uint32_t reg, uint32_t data) {
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, reg, data);
}

/**
 * Write one PBUS word through the installed slot.
 * @param bank Bank
 * @param index Index in the bank
 * @param value Word to store
 */
static void _pbus(uint32_t bank, uint32_t index, uint32_t value) {
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS)(bank, index, value);
}

/**
 * Select the 11p filter coefficients, or restore the calibrated ones.
 * @param mode Zero restores the calibrated bytes
 * @param band Zero uses the wider divisor
 */
void phy_11p_set(int mode, int band) {
	unsigned long word;
	int field;
	int div;
	int value;
	uint32_t byte;

	phy_param[_PAR_11P_A] = (uint8_t)mode;
	phy_param[_PAR_11P_B] = (uint8_t)band;
	if(mode == 0) {
		word = mp_arch_rf_read(_REG_CLK);
		mp_arch_rf_write(_REG_CLK, word & 0xffffffe3UL);
		mp_arch_rf_set(_REG_BB, 0x20UL);
		_filt(4, phy_param[_PAR_RC_A]);
		_filt(5, phy_param[_PAR_RC_A]);
		_filt(6, phy_param[_PAR_RC_B]);
		_filt(7, phy_param[_PAR_RC_B]);
		_filt(0xc, phy_param[_PAR_RC_A]);
		_filt(0xd, phy_param[_PAR_RC_A]);
		_filt(0xe, phy_param[_PAR_RC_B]);
		_filt(0xf, phy_param[_PAR_RC_B]);
		return;
	}
	if(band == 0) {
		field = 4;
		div = 100;
	} else {
		field = 5;
		div = 50;
	}
	word = mp_arch_rf_read(_REG_CLK);
	mp_arch_rf_write(_REG_CLK, (word & 0xffffffe3UL) | ((unsigned long)field << 2));
	mp_arch_rf_clr(_REG_BB, 0x20UL);
	value = (int)phy_param[_PAR_RC_RAW];
	value = ((value + 56) * 103) / div;
	value -= 8;
	if(value > 63)
		value = 63;
	byte = (uint32_t)value & 0xff;
	_filt(4, byte);
	_filt(5, byte);
	_filt(6, byte);
	_filt(7, byte);
	_filt(0xc, byte);
	_filt(0xd, byte);
	_filt(0xe, byte);
	_filt(0xf, byte);
}

/**
 * Save or restore the digital baseband registers.
 * @param save Zero writes the buffer back, non-zero reads the registers
 * @param buf At least 20 words
 * @returns 20 when the extra pair is included, otherwise 18
 */
uint32_t phy_dig_reg_backup_new(int save, uint32_t *buf) {
	static const unsigned long regs[18] = {
		0x600A790CUL, 0x600A780CUL, 0x600A7904UL, 0x600A7074UL,
		0x600A711CUL, 0x600A08B0UL, 0x600A08B4UL, 0x600A08B8UL,
		0x600A08BCUL, 0x600A702CUL, 0x600A7018UL, 0x600A701CUL,
		0x600A7010UL, 0x600A7014UL, 0x600A7044UL, 0x600A7108UL,
		0x600A70CCUL, 0x600A7124UL
	};
	unsigned i;

	if(save == 0) {
		for(i = 0; i < 18; i++)
			mp_arch_rf_write(regs[i], buf[i]);
		if((mp_arch_rf_read(_REG_CLK_EN) & 4UL) != 0) {
			mp_arch_rf_write(0x600A8060UL, buf[0x12]);
			mp_arch_rf_write(0x600A807CUL, buf[0x13]);
			return(0x14);
		}
	} else {
		for(i = 0; i < 18; i++)
			buf[i] = (uint32_t)mp_arch_rf_read(regs[i]);
		if((mp_arch_rf_read(_REG_CLK_EN) & 4UL) != 0) {
			buf[0x12] = (uint32_t)mp_arch_rf_read(0x600A8060UL);
			buf[0x13] = (uint32_t)mp_arch_rf_read(0x600A807CUL);
			return(0x14);
		}
	}
	return(0x12);
}

/**
 * Kept as an empty entry.
 */
void phy_freq_mem_backup(void) {
}

/**
 * Compensation code from the channel byte.
 * @returns 253, 199 or 151
 */
uint32_t phy_ftm_comp(void) {
	int8_t chan;

	chan = (int8_t)phy_param[_PAR_CHAN_B];
	if(chan == 0)
		return(0xfd);
	if(chan == 1)
		return(199);
	return(0x97);
}

/**
 * Prepare a random ADC sample, or return the bus to work mode.
 * @param enable Zero leaves receive clock off
 */
void phy_get_adc_rand(int enable) {
	int funcs;
	unsigned long word;
	uint32_t dco[2];

	phy_param_addr(phy_param);
	funcs = phy_get_romfuncs();
	*(uint8_t **)(funcs + 0x6c) = (uint8_t *)ram_pbus_rd_addr;
	g_phyFuns = (unsigned char *)(uintptr_t)funcs;
	*(uint8_t **)(funcs + 0x70) = (uint8_t *)ram_pbus_rd_shift;
	if(enable == 0) {
		(*(mp_arch_rf_code_t *)(funcs + 0x80))();
		pbus_workmode();
		set_rxclk_en(0);
		return;
	}
	mp_arch_rf_set(_REG_PMU_A, 0xf0000000UL);
	mp_arch_rf_write(_REG_PMU_B, 0x10000000UL);
	mp_arch_rf_set(_REG_SYS_A, 0x80000000UL);
	mp_arch_rf_set(_REG_CLK_EN, 0x1e400UL);
	word = mp_arch_rf_read(_REG_SYS_B);
	mp_arch_rf_write(_REG_SYS_B, (word & 0xffff0fffUL) | 0x4000UL);
	word = mp_arch_rf_read(_REG_SYS_B);
	mp_arch_rf_write(_REG_SYS_B, (word & 0xf0ffffffUL) | 0x04000000UL);
	word = mp_arch_rf_read(_REG_SYS_B);
	mp_arch_rf_write(_REG_SYS_B, (word & 0x0fffffffUL) | 0x40000000UL);
	set_rxclk_en(1);
	mp_arch_rf_set(_REG_FE, 0x100000UL);
	pbus_debugmode();
	_pbus(4, 1, 0);
	_pbus(4, 2, 1);
	_pbus(5, 1, 0);
	_pbus(0, 1, 0);
	_pbus(0, 2, 0);
	_pbus(1, 1, 0x189);
	_pbus(1, 2, 0x100);
	dco[0] = 0x00010001;
	dco[1] = 0x00010001;
	pbus_set_dco(dco);
}

/**
 * Signed CCA byte.
 * @returns The low byte of the CCA register, sign-extended
 */
int phy_get_cca(void) {
	return((int)(int8_t)mp_arch_rf_read(_REG_CCA));
}

/**
 * Read the two CCA counters and the overflow bit.
 * @param out Two masked counters
 * @returns Bit 27 of the first counter register
 */
uint32_t phy_get_cca_cnt(uint32_t *out) {
	out[0] = (uint32_t)mp_arch_rf_read(_REG_CCA_A) & 0x7ffffff;
	out[1] = (uint32_t)mp_arch_rf_read(_REG_CCA_B) & 0x7ffffff;
	return((uint32_t)(mp_arch_rf_read(_REG_CCA_A) >> 27) & 1);
}

/**
 * Largest signed target-power byte over channels 1..14.
 * @returns The maximum, sign-extended
 */
uint32_t phy_get_most_tpw(void) {
	int8_t best;
	int8_t sample[18];
	int chan;
	int i;
	int8_t byte;

	best = -100;
	chan = 1;
	while(chan != 15) {
		get_chan_target_power_new(chan, (int)(int8_t)phy_param[_PAR_TPW],
			(int)(uintptr_t)sample,
			(int)(uintptr_t)(phy_param + _PAR_TAB_A),
			phy_param[_PAR_RATE],
			(uint32_t)(uintptr_t)(phy_param + _PAR_TAB_C),
			(uint32_t)(uintptr_t)(phy_param + _PAR_TAB_B));
		for(i = 0; i < 18; i++) {
			byte = sample[i];
			if(byte >= best)
				best = byte;
		}
		chan = (int)(uint16_t)(chan + 1);
	}
	return((uint32_t)(int)best);
}

/**
 * Noise floor with the low ten bits forced on.
 * @returns The register value with bits 9:0 set
 */
uint32_t phy_get_noise_floor(void) {
	return((uint32_t)mp_arch_rf_read(_REG_NOISE) | 0xfffffc00UL);
}

/**
 * RSSI from the noise floor and the high byte of the RSSI register.
 * @returns The sum, sign-extended from 16 bits
 */
int phy_get_rssi(void) {
	int noise;
	unsigned byte;

	noise = (int)phy_get_noise_floor();
	byte = (unsigned)(mp_arch_rf_read(_REG_RSSI) >> 8) & 0xff;
	return((int)(int16_t)((noise >> 2) + (int)byte));
}

/**
 * Signed frequency offset from one packed word.
 * @param rate Rate, below 8 uses the low half
 * @param packed Packed frequency word
 * @returns The offset, sign-extended from 16 bits
 */
int phy_get_rx_freq(uint32_t rate, uint32_t packed) {
	int half;
	int value;
	int div;

	if(rate < 8) {
		half = (int)(int16_t)packed;
		value = (int)(int16_t)(half & 0x7fff);
		if(((half << 17) & 0x80000000) != 0)
			value = (int)(int16_t)(value - 0x8000);
		div = -48;
	} else {
		half = (int)(int16_t)(packed >> 15);
		value = (int)(int16_t)(half & 0x7fff);
		if(((half << 17) & 0x80000000) != 0)
			value = (int)(int16_t)(value - 0x8000);
		value = value * -5;
		div = 128;
	}
	return((int)(int16_t)(value / div));
}

/**
 * @returns 0
 */
uint32_t phy_internal_delay(void) {
	return(0);
}

/**
 * Program the CCA window and optionally start it.
 * @param count Window length, low 27 bits
 * @param start Non-zero sets the start bits
 */
void phy_set_cca_cnt(uint32_t count, int start) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_CCA_CTL);
	mp_arch_rf_write(_REG_CCA_CTL, (word & 0xf8000000UL) | (count & 0x7ffffffUL));
	if(start != 0)
		mp_arch_rf_set(_REG_CCA_CTL, 0x18000000UL);
}

/**
 * Store the target-power byte and reload the transmit gain.
 * @param power Byte stored at phy_param + 6
 */
void phy_set_most_tpw(uint8_t power) {
	phy_param[_PAR_TPW] = power;
	wifi_set_tx_gain(mp_arch_rf_u16(_PAR_CHAN), 0);
}

/**
 * Kept as an empty entry.
 */
void phy_set_rate(void) {
}

/**@}*/
