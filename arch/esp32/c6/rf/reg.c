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

Baseband register setup, transmit-tone control and the gain-memory writer.

@{
*/

#define _REG_PMU     0x600B0154UL
#define _REG_PMU_B   0x600B00CCUL
#define _REG_FE      0x600A0910UL
#define _REG_TONE_A  0x600A0414UL
#define _REG_TONE_B  0x600A0420UL
#define _REG_TONE_C  0x600A0424UL
#define _REG_TONE_D  0x600A042CUL
#define _REG_CLK     0x600A981CUL
#define _REG_STAT    0x600A00C0UL
#define _REG_BBPLL   0x600AF818UL
#define _PAR_TXON    0x19a

/**
 * Clear or set one field, as its own read and write.
 * @param addr Register
 * @param keep Bits that stay
 * @param bits Bits that are set
 */
static void _mix(unsigned long addr, unsigned long keep, unsigned long bits) {
	mp_arch_rf_write(addr, (mp_arch_rf_read(addr) & keep) | bits);
}

/**
 * Program the 11b receive options. Each field is its own update.
 * @param enable Non-zero selects the wider set
 */
void rx_11b_opt(int enable) {
	unsigned long word;

	if(enable == 0) {
		word = mp_arch_rf_read(0x600A7044UL);
		mp_arch_rf_write(0x600A7044UL, (word & 0xffc0ffffUL) | 0x3e00000UL);
		word = mp_arch_rf_read(0x600A7044UL);
		mp_arch_rf_write(0x600A7044UL, (word & 0xffffc0ffUL) | 0x1800UL);
		word = mp_arch_rf_read(0x600A7124UL);
		mp_arch_rf_write(0x600A7124UL, (word & 0xffff03ffUL) | 0x6000UL);
		word = mp_arch_rf_read(0x600A7124UL);
		mp_arch_rf_write(0x600A7124UL, (word & 0xfffffff0UL) | 4UL);
		word = mp_arch_rf_read(0x600A8004UL);
		mp_arch_rf_write(0x600A8004UL, (word & 0xffff0fffUL) | 0x6000UL);
	} else {
		word = mp_arch_rf_read(0x600A7044UL);
		mp_arch_rf_write(0x600A7044UL, word | 0x3f00000UL);
		word = mp_arch_rf_read(0x600A7044UL);
		mp_arch_rf_write(0x600A7044UL, (word & 0xffffc0ffUL) | 0x2100UL);
		word = mp_arch_rf_read(0x600A7124UL);
		mp_arch_rf_write(0x600A7124UL, (word & 0xffff03ffUL) | 0x8400UL);
		word = mp_arch_rf_read(0x600A7124UL);
		mp_arch_rf_write(0x600A7124UL, (word & 0xfffffff0UL) | 3UL);
		word = mp_arch_rf_read(0x600A8004UL);
		mp_arch_rf_write(0x600A8004UL, (word & 0xffff0fffUL) | 0x9000UL);
	}
	word = mp_arch_rf_read(0x600A7104UL);
	mp_arch_rf_write(0x600A7104UL, (word & 0xfffffe00UL) | 0x1c8UL);
}

/** Program the power-amplifier on-time fields. */
void tx_paon_set_new(void) {
	unsigned long word;

	_mix(0x600A7C00UL, 0xffe007ffUL, 0xa000UL);
	_mix(0x600A08F8UL, 0xffff00ffUL, 0x5000UL);
	mp_arch_rf_write(0x600A7C6CUL, 0x140c81eUL);
	_mix(0x600A7C30UL, 0xfffffc00UL, 0x1eUL);
	word = mp_arch_rf_read(0x600A08FCUL);
	mp_arch_rf_write(0x600A08FCUL, (word & 0xffffUL) | 0xa0e00000UL);
	word = mp_arch_rf_read(0x600A08FCUL);
	mp_arch_rf_write(0x600A08FCUL, (word & 0xffff00ffUL) | 0xc800UL);
}

/** Load the receive compensation constants. */
void set_rx_comp_new(void) {
	_mix(0x600A702CUL, 0xffffff00UL, 0xe9UL);
	_mix(0x600A70A0UL, 0x00ffffffUL, 0xe9000000UL);
}

/**
 * Load the AGC windows from the two parameters.
 * @param span Window length
 * @param extra Extra delay added to 80
 */
void agc_reg_init_new(uint32_t span, int extra) {
	unsigned long word;
	uint32_t step;

	step = span - 1;
	_mix(0x600A713CUL, 0xfe03ffffUL, ((unsigned long)step << 18) & 0x1fc0000UL);
	_mix(0x600A7094UL, 0xfffffe03UL, ((unsigned long)step << 2) & 0x1fcUL);
	_mix(0x600A702CUL, 0xffff80ffUL, ((unsigned long)span << 8) & 0x7f00UL);
	_mix(0x600A0958UL, 0xfffff00fUL, (unsigned long)span << 4);
	_mix(0x600A0958UL, 0xfff00fffUL, ((unsigned long)(extra + 80) << 12) & 0xff000UL);
	word = mp_arch_rf_read(0x600A702CUL);
	mp_arch_rf_write(0x600A702CUL, (word & 0x00ffffffUL) | 0x32000000UL);
	mp_arch_rf_set(0x600A702CUL, 0x80000UL);
	mp_arch_rf_clr(0x600A702CUL, 0x800000UL);
	_mix(0x600A7128UL, 0x00ffffffUL, 0xd2000000UL);
	mp_arch_rf_write(0x600A7068UL, 0x404UL);
}

/** Set the shared Bluetooth and Wi-Fi baseband bits. */
void btbb_wifi_bb_cfg2(void) {
	mp_arch_rf_set(0x600A7CD0UL, 0xf000bUL);
}

/** Load the baseband defaults, then the shared Bluetooth bits. */
void bb_reg_init_new(void) {
	unsigned long word;

	mp_arch_rf_set(0x600A7400UL, 0x6000UL);
	_mix(0x600A7848UL, 0xff000000UL, 0x433afUL);
	_mix(0x600A7848UL, 0xe0ffffffUL, 0x17000000UL);
	_mix(0x600A7808UL, 0xffffc07fUL, 0x3000UL);
	_mix(0x600A78DCUL, 0xffffc07fUL, 0x100UL);
	mp_arch_rf_clr(0x600A78E4UL, 0x400000UL);
	word = mp_arch_rf_read(0x600A7C30UL);
	mp_arch_rf_write(0x600A7C30UL, word & 0xfff00fffUL);
	mp_arch_rf_clr(0x600A790CUL, 0x800UL);
	mp_arch_rf_set(0x600A7CA8UL, 0x100000UL);
	mp_arch_rf_clr(0x600A7980UL, 0x2000000UL);
	mp_arch_rf_clr(0x600A7890UL, 0x2000000UL);
	mp_arch_rf_set(0x600A7890UL, 0x1000000UL);
	word = mp_arch_rf_read(0x600A7A28UL);
	mp_arch_rf_write(0x600A7A28UL, word & 0xf000fUL);
	word = mp_arch_rf_read(0x600A7424UL);
	mp_arch_rf_write(0x600A7424UL, (word & 0xe01fffffUL) | 0x1000000UL);
	mp_arch_rf_set(0x600A7CD0UL, 0xf000fUL);
	mp_arch_rf_set(0x600A7C00UL, 0x200UL);
	mp_arch_rf_set(_REG_CLK, 0x800UL);
	mp_arch_rf_clr(0x600A7438UL, 0xc0UL);
	mp_arch_rf_clr(0x600A7438UL, 0x100UL);
	btbb_wifi_bb_cfg2();
}

/**
 * Bring up the analog rail, then wait for the SDM.
 * @param cold Non-zero clears the top power bits and waits first
 */
void open_i2c_xpd_new(int cold) {
	unsigned long word;

	if(cold != 0) {
		word = mp_arch_rf_read(_REG_PMU);
		mp_arch_rf_write(_REG_PMU, word & 0x0fffffffUL);
		mp_arch_rf_delay_us(100);
	}
	mp_arch_rf_set(_REG_PMU, 0xf0000000UL);
	mp_arch_rf_write(_REG_PMU_B, 0x10000000UL);
	word = mp_arch_rf_read(_REG_PMU);
	if(((long)word << 4) >= 0) {
		mp_arch_rf_set(_REG_PMU, 0x08000000UL);
		mp_arch_rf_clr(_REG_PMU, 0x04000000UL);
		mp_arch_rf_set(_REG_PMU, 0x04000000UL);
	}
	word = mp_arch_rf_read(_REG_PMU);
	if(((long)word << 5) >= 0)
		mp_arch_rf_set(_REG_PMU, 0x04000000UL);
	wait_i2c_sdm_stable();
}

/**
 * Force the transmit path on or off.
 * @param on Non-zero turns the path on
 */
void force_txon(int on) {
	unsigned long word;

	mp_arch_rf_clr(0x600A70DCUL, 0x10000UL);
	if(on == 0) {
		word = mp_arch_rf_read(_REG_FE);
		if(((word >> 10) & 3UL) == 3UL) {
			_mix(_REG_FE, 0xfffff3ffUL, 0x800UL);
			mp_arch_rf_delay_us(1);
		}
		word = mp_arch_rf_read(_REG_FE);
		mp_arch_rf_write(_REG_FE, word & 0xfffff3ffUL);
		mp_arch_rf_delay_us(1);
		word = mp_arch_rf_read(_REG_FE);
		mp_arch_rf_write(_REG_FE, word & 0xfffffcffUL);
	} else {
		_mix(_REG_FE, 0xfffffcffUL, 0x200UL);
		mp_arch_rf_delay_us(1);
		mp_arch_rf_set(_REG_FE, 0xc00UL);
	}
	mp_arch_rf_delay_us(1);
}

/**
 * Force a transmit mode and record it.
 * @param on Non-zero selects the active mode
 * @param apply Non-zero programs the frequency path first
 * @param index Power index, plus 32 when apply is set
 */
void force_txon_mode(uint32_t on, uint32_t apply, uint32_t index) {
	unsigned long stat;
	unsigned long word;
	unsigned long bits;

	if(apply != 0)
		index = (index + 0x20) & 0xff;
	stat = mp_arch_rf_read(_REG_STAT);
	if(apply != 0) {
		phy_dis_hw_set_freq_1();
		_mix(0x600AF4B4UL, 0xfffffffeUL, (unsigned long)(apply & on & 1));
		bits = 0;
		if(on != 0)
			bits = 0x3000UL;
		_mix(_REG_FE, 0xffffcfffUL, bits);
		mp_arch_rf_delay_us(1);
		bits = 0;
		if(on != 0)
			bits = 0xc0000000UL;
		_mix(0x600A28A0UL, 0x3fffffffUL, bits);
		mp_arch_rf_delay_us(100);
		if(((long)stat << 5) >= 0)
			phy_en_hw_set_freq();
	}
	phy_force_pwr_index(on != 0, index);
	force_txon(0);
	force_txon((int)on);
	phy_param[_PAR_TXON] = (uint8_t)on;
}

/**
 * Force both transmit and receive off, or receive on.
 * @param rx Non-zero leaves receive forced on
 */
void force_txrx_off(int rx) {
	unsigned long word;

	if(rx == 0) {
		_mix(_REG_FE, 0xfffff0ffUL, 0x200UL);
		mp_arch_rf_delay_us(1);
		word = mp_arch_rf_read(_REG_FE);
		mp_arch_rf_write(_REG_FE, word & 0xfffff0ffUL);
	} else {
		_mix(_REG_FE, 0xfffff0ffUL, 0x800UL);
		mp_arch_rf_delay_us(1);
		_mix(_REG_FE, 0xfffff0ffUL, 0xa00UL);
	}
	mp_arch_rf_delay_us(1);
}

/**
 * Set the MAC channel-offset field.
 * @param mode 2 stores 2, 3 stores 1, anything else stores 0
 */
void mac_tx_chan_offset_new(int mode) {
	unsigned long bits;

	if(mode == 2)
		bits = 2;
	else if(mode == 3)
		bits = 1;
	else
		bits = 0;
	_mix(0x600A4400UL, 0xfffffffcUL, bits);
}

/** Enable the automatic noise-floor update, one bit at a time. */
void noise_floor_auto_set_new(void) {
	mp_arch_rf_set(0x600A7018UL, 0x80000UL);
	mp_arch_rf_set(0x600A7018UL, 0x10000000UL);
}

/** Set the Bluetooth filter bits one at a time. */
void bt_filter_reg_new(void) {
	mp_arch_rf_set(0x600A0900UL, 0x02000000UL);
	mp_arch_rf_clr(0x600A0900UL, 0x400000UL);
	mp_arch_rf_clr(0x600A0900UL, 0x01800000UL);
}

/** Run the BBPLL calibration, discarding one status read between the two calls. */
void phy_bbpll_recal(void) {
	phy_bbpll_cal(1);
	(void)mp_arch_rf_read(_REG_BBPLL);
	phy_bbpll_cal(0);
}

/** Disable CCA in two updates. */
void phy_disable_cca_new(void) {
	_mix(0x600A4C5CUL, 0x3fffffffUL, 0x80000000UL);
	_mix(0x600A4C5CUL, 0xcfffffffUL, 0x20000000UL);
}

/** Enable CCA in two updates. */
void phy_enable_cca_new(void) {
	mp_arch_rf_clr(0x600A4C5CUL, 0xc0000000UL);
	mp_arch_rf_clr(0x600A4C5CUL, 0x30000000UL);
}

/**
 * Force the FFT scale in three updates.
 * @param bit Low bit written to bit 19
 * @param scale Scale written to bits 27:20
 */
void phy_fft_scale_force(uint32_t bit, int scale) {
	unsigned long word;

	_mix(0x600A7800UL, 0xf00fffffUL, (unsigned long)scale << 20);
	mp_arch_rf_clr(0x600A7800UL, 0x80000UL);
	word = mp_arch_rf_read(0x600A7800UL);
	mp_arch_rf_write(0x600A7800UL,
		(word & 0xfff7ffffUL) | (((unsigned long)bit << 19) & 0x80000UL));
}

/**
 * Force the receive gain in two updates.
 * @param bit Low bit written to bit 23
 * @param gain Gain written above bit 23
 */
void phy_force_rx_gain(uint32_t bit, int gain) {
	unsigned long word;

	_mix(0x600A702CUL, 0x00ffffffUL, (unsigned long)gain << 24);
	word = mp_arch_rf_read(0x600A702CUL);
	mp_arch_rf_write(0x600A702CUL,
		(word & 0xff7fffffUL) | (((unsigned long)bit << 23) & 0x800000UL));
}

/**
 * Enable or clear the Wi-Fi clock bit.
 * @param enable Non-zero sets the bit
 */
void phy_wifi_enable_set(int enable) {
	if(enable == 0)
		mp_arch_rf_clr(_REG_CLK, 2UL);
	else
		mp_arch_rf_set(_REG_CLK, 2UL);
}

/**
 * Program one transmit tone. A zero first argument plays a quiet tone.
 * @param on Non-zero uses the gain tables
 * @param bt Non-zero selects the Bluetooth table
 * @param level Level passed to the saturation helper
 */
void phy_tx_tone(int on, int bt, uint32_t level) {
	uint8_t bytes[32];
	uint16_t seconds[32];
	uint16_t codes[32];
	int index;
	int base;
	int step;
	uint8_t byte;

	if(on == 0) {
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_TX_TONE)(0, 0, 0, 0, 0, 0);
		force_txon_mode(0, 1, 10);
		return;
	}
	if(bt == 0) {
		index = get_data_sat((int)level, 31, 0);
		wifi_get_tx_tab(6, (uint32_t)(uintptr_t)bytes, (uint32_t)(uintptr_t)seconds,
			(uint32_t)(uintptr_t)codes, 0);
		base = 40;
	} else {
		index = get_data_sat((int)level, 15, 0);
		bt_get_tx_tab((uint32_t)(uintptr_t)codes, (uint32_t)(uintptr_t)seconds,
			(uint32_t)(uintptr_t)bytes, 0);
		base = 18;
	}
	index = (int)(uint8_t)index;
	byte = bytes[index];
	force_txon_mode((uint32_t)on, (uint32_t)bt, (uint32_t)index);
	step = (int)(int8_t)(base - (int)byte);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_TX_TONE)(on, 0, step & 0xff, 0, 0, 0);
}

/**
 * Start a transmit tone.
 * @param on First enable
 * @param freq First frequency word
 * @param phase First phase
 * @param on2 Second enable
 * @param freq2 Second frequency word
 * @param phase2 Second phase
 */
void ram_start_tx_tone_step(int on, uint32_t freq, int phase, int on2, uint32_t freq2, int phase2) {
	unsigned long word;
	unsigned long low;

	if((on | on2) == 0)
		mp_arch_rf_set(_REG_TONE_A, 3UL);
	else
		mp_arch_rf_clr(_REG_TONE_A, 3UL);
	_mix(_REG_TONE_D, 0xfffffffcUL, (unsigned long)freq & 3UL);
	_mix(_REG_TONE_D, 0xfffffff3UL, ((unsigned long)freq2 << 2) & 0xcUL);
	low = ((unsigned long)on << 18) | (unsigned long)((int)freq >> 2);
	low |= ((unsigned long)(0 - phase) & 0xffUL) << 10;
	word = mp_arch_rf_read(_REG_TONE_B);
	mp_arch_rf_write(_REG_TONE_B, (low & 0x0fffffffUL) | (word & 0xf0000000UL));
	low = ((unsigned long)on2 << 18) | (unsigned long)((int)freq2 >> 2);
	low |= ((unsigned long)(0 - phase2) & 0xffUL) << 10;
	word = mp_arch_rf_read(_REG_TONE_C);
	mp_arch_rf_write(_REG_TONE_C, (low & 0x0fffffffUL) | (word & 0xf0000000UL));
}

/** Stop both transmit tones. */
void ram_stop_tx_tone(void) {
	mp_arch_rf_clr(_REG_TONE_B, 0x40000UL);
	mp_arch_rf_clr(_REG_TONE_C, 0x40000UL);
	mp_arch_rf_set(_REG_TONE_A, 3UL);
}

/**
 * Reset the receive saturation bits.
 * @param wide Non-zero selects the wide mask
 */
void rfrx_sat_rst(int wide) {
	unsigned long word;

	mp_arch_rf_write(0x600A7068UL, 0x404UL);
	if(wide == 0) {
		word = mp_arch_rf_read(0x600A705CUL);
		mp_arch_rf_write(0x600A705CUL, word & 0x2ef7ffffUL);
		word = mp_arch_rf_read(0x600A705CUL);
		mp_arch_rf_write(0x600A705CUL, (word & 0xfff80000UL) | 0x400UL);
	} else {
		word = mp_arch_rf_read(0x600A705CUL);
		mp_arch_rf_write(0x600A705CUL, word | 0xd1080000UL);
		word = mp_arch_rf_read(0x600A705CUL);
		mp_arch_rf_write(0x600A705CUL, (word & 0xfff80000UL) | 0x800UL);
	}
}

/**
 * Write one gain-memory row.
 * @param a First word
 * @param b Second word
 * @param c Third word
 * @param slot Row number
 */
void write_gain_mem_new(uint32_t a, uint32_t b, uint32_t c, int slot) {
	unsigned long word;

	mp_arch_rf_write(0x600A08CCUL, a);
	mp_arch_rf_write(0x600A08D0UL, b);
	mp_arch_rf_write(0x600A08D4UL, c);
	word = mp_arch_rf_read(0x600A08C8UL);
	mp_arch_rf_write(0x600A08C8UL,
		(word & 0xfff00000UL) | (((unsigned long)slot << 11) + 0x80000UL));
}

/**@}*/
