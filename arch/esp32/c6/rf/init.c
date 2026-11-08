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

Bring the C6 radio up: install the RAM slots, program the front end,
and move the calibration image in and out of phy_param.

@{
*/

	#define _REG_APB      0x6000E000UL
	#define _REG_FE_FREQ  0x600A00C0UL
	#define _REG_FE_PA    0x600A08F8UL
	#define _REG_FE_A     0x600A044CUL
	#define _REG_FE_B     0x600A0450UL
	#define _REG_FE_C     0x600A046CUL
	#define _REG_BB_TX    0x600A7428UL
	#define _REG_XTAL     0x60096130UL
	#define _REG_TIMER    0x600AD000UL
	#define _REG_SYS_CLK  0x600A9814UL
	#define _REG_SYS_GATE 0x600A981CUL
	#define _REG_PMU_XPD  0x600B0154UL
	#define _REG_EFUSE0   0x600B0844UL
	#define _REG_EFUSE1   0x600B0848UL

	#define _PAR_FLAG     0xa4
	#define _PAR_HOLD     0x25
	#define _PAR_DBM      0x4e
	#define _PAR_OFFSET   0xa1
	#define _PAR_CLOSED   0x17
	#define _PAR_SKIP_TS  0x19d
	#define _PAR_WIFI_OFF 0x19e
	#define _PAR_XTAL     0x51
	#define _PAR_RANGE    0x16
	#define _PAR_CHAN     0x11c
	#define _PAR_CHGP     0x196
	#define _PAR_TXDC     0xa8
	#define _PAR_RXDC     0x104
	#define _PAR_IQ       0xd0
	#define _PAR_IQ_BT    0xd4
	#define _PAR_IQ_RX    0x102
	#define _PAR_ATTEN    0xf4
	#define _PAR_BT_ATT   0xf8
	#define _PAR_DCAP     0xdc
	#define _PAR_PWDET    0x18e
	#define _PAR_GAIN0    0x120
	#define _PAR_GAIN1    0x121
	#define _PAR_LEVEL    0x50

	#define _SDM_READY    91
	#define _SDM_WAIT     0x270f
	#define _GAP          4

extern char ram_chip_i2c_readReg_org[];
extern char ram_get_i2c_hostid[];
extern char ram_get_i2c_mst0_mask[];
extern char ram_get_i2c_read_mask[];

static void _slot(unsigned off, void *fn) {
	*(void **)(g_phyFuns + off) = fn;
}

static uint16_t _at16(const uint8_t *row, unsigned off) {
	uint16_t	v;

	memcpy(&v, row + off, sizeof(v));
	return(v);
}

static void _put16(uint8_t *row, unsigned off, uint16_t v) {
	memcpy(row + off, &v, sizeof(v));
}

static void _gap(uint16_t *miss, int left, int right) {
	if((int)abs_temp(left - right) > _GAP)
		*miss = (uint16_t)(*miss + 1);
}

static void _iq_gap(uint16_t *miss, uint16_t left, uint16_t right) {
	uint8_t	a[2];
	uint8_t	b[2];

	get_iq_value(a, left);
	get_iq_value(b, right);
	_gap(miss, (int)(int8_t)a[0], (int)(int8_t)b[0]);
	_gap(miss, (int)(int8_t)a[1], (int)(int8_t)b[1]);
}

/**
 * Program the front-end and transmit-baseband defaults.
 */
void reg_set_new(void) {
	mp_arch_rf_write(_REG_FE_PA, (mp_arch_rf_read(_REG_FE_PA) & 0xffffff00UL) | 4UL);
	mp_arch_rf_write(_REG_FE_A, mp_arch_rf_read(_REG_FE_A) | 0x08000000UL);
	mp_arch_rf_write(_REG_FE_C, mp_arch_rf_read(_REG_FE_C) & 0xdfffffffUL);
	mp_arch_rf_write(_REG_FE_B, mp_arch_rf_read(_REG_FE_B) | 0x80008000UL);
	mp_arch_rf_write(_REG_BB_TX, (mp_arch_rf_read(_REG_BB_TX) & 0xffffc0ffUL) | 0x1500UL);
}

/**
 * Poll the SDM host until it reports ready, or the timer expires.
 */
void wait_i2c_sdm_stable(void) {
	uint32_t	start;
	uint32_t	now;

	start = mp_arch_rf_read(_REG_TIMER);
	for(;;) {
		now = mp_arch_rf_read(_REG_TIMER);
		if(now - start > _SDM_WAIT)
			return;
		if((int)mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL_SDM, 1, 0) == _SDM_READY)
			return;
	}
}

/**
 * Record the crystal code and the divider byte.
 */
void phy_get_xtal_freq(void) {
	int		freq;
	uint8_t		code;

	freq = rtc_clk_xtal_freq_get();
	code = 2;
	if(freq != 32)
		code = (uint8_t)(freq == 26);
	mp_arch_rf_set_u8(_PAR_XTAL, code);
	mp_arch_rf_write(_REG_XTAL,
		(mp_arch_rf_read(_REG_XTAL) & 0xffffff00UL) | ((uint32_t)(freq - 1) & 0xffUL));
}

/**
 * Apply the digital register defaults and enable the baseband.
 */
void phy_reg_init(void) {
	reg_set_new();
	iq_corr_enable();
	agc_reg_init_new(phy_param[_PAR_GAIN1], phy_param[_PAR_GAIN0]);
	wifi_agc_sat_gain(0x81825);
	bb_reg_init_new();
	bb_wdg_cfg();
	tx_paon_set_new();
	rx_11b_opt(1);
	tx_pwctrl_bg_init();
	noise_floor_auto_set_new();
	phy_ant_init();
	bt_filter_reg_new();
	mac_enable_bb(1);
}

/**
 * Wake the radio and restore the channel it was on.
 */
void phy_wakeup_init_(void) {
	uint32_t	held;
	uint32_t	saved;

	phy_bbpll_cal(1);
	bias_reg_set(1);
	held = (uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_ENTER)();
	saved = mp_arch_rf_read(_REG_SYS_CLK);
	mp_arch_rf_write(_REG_SYS_CLK, mp_arch_rf_read(_REG_SYS_CLK) | 0x404UL);
	mp_arch_rf_write(_REG_FE_FREQ,
		(mp_arch_rf_read(_REG_FE_FREQ) & 0xe7ffffffUL) | 0x10000000UL);
	phy_set_tsens_power(1);
	phy_get_xtal_freq();
	open_i2c_xpd_new(0);
	force_txrx_off(1);
	pbus_clear_reg();
	i2c_clk_sel(8);
	fe_txrx_reset();
	i2c_master_reset();
	i2c_bbpll_set(0);
	i2cmst_reg_init();
	freq_reg_init(2, 4);
	fe_reg_init();
	pwdet_reg_init_new();
	phy_i2c_init2();
	freq_i2c_data_write_new();
	filter_dcap_set();
	write_chan_freq(phy_param[_PAR_CHAN]);
	set_pbus_reg();
	phy_reg_init();
	set_chan_reg(1);
	set_rx_comp_new();
	enable_agc();
	phy_wait_freq_set_busy();
	phy_en_hw_set_freq();
	phy_bbpll_cal(0);
	force_txrx_off(0);
	mp_arch_rf_set_u8(_PAR_CLOSED, 0);
	mp_arch_rf_set_u8(_PAR_SKIP_TS, 0);
	mp_arch_rf_write(_REG_FE_FREQ, mp_arch_rf_read(_REG_FE_FREQ) & 0xe7ffffffUL);
	mp_arch_rf_write(_REG_SYS_CLK, saved);
	if(phy_param[_PAR_WIFI_OFF] != 0)
		phy_wifi_enable_set(0);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_EXIT)(held);
}

/**
 * Publish phy_param and install the RAM slots.
 */
void phy_get_romfunc_addr(void) {
	phy_param_addr(phy_param);
	g_phyFuns = (unsigned char *)(uintptr_t)phy_get_romfuncs();
	_slot(0x48, ram_chip_i2c_readReg_org);
	_slot(0x44, ram_get_i2c_hostid);
	_slot(0x3c, ram_get_i2c_read_mask);
	_slot(0x40, ram_get_i2c_mst0_mask);
	_slot(0x14, ram_set_chan_freq_sw_start);
	_slot(0x6c, ram_pbus_rd_addr);
	_slot(0x70, ram_pbus_rd_shift);
	_slot(0x98, ram_start_tx_tone_step);
	_slot(0x8c, ram_pbus_xpd_tx_on);
	_slot(0x24, ram_pwdet_tone_start);
	_slot(0x2c, ram_read_sar_dout);
	_slot(0x28, ram_pwdet_wait_idle);
	_slot(0x9c, ram_stop_tx_tone);
	_slot(0xa8, ram_bb_gain_index);
	_slot(0x08, ram_set_chan_cal_interp);
	_slot(0x10, ram_bb_bss_cbw40);
	_slot(0x94, ram_txcal_debuge_mode);
	_slot(0x90, ram_set_loopback_gain);
	_slot(0x84, ram_pbus_xpd_rx_on);
	_slot(0x34, phy_i2c_enter_critical);
	_slot(0x38, phy_i2c_exit_critical);
	_slot(0x68, ram_pbus_force_mode);
	_slot(0x30, ram_rfpll_set_freq);
}

/**
 * Measure the RC constants with the fixed seed words.
 */
void rc_cal_init(void) {
	uint32_t	seed;
	uint16_t	limit;
	uint32_t	mask;

	seed = 0x141e1428;
	limit = 0x2814;
	mask = 0x20163224;
	rc_cal_new((uint8_t *)&seed, (uint8_t *)&limit, (int)(uintptr_t)&mask);
}

/**
 * Open the analog path and seed the frequency memory.
 */
void rf_init(void) {
	uint32_t	field;

	phy_bbpll_cal(1);
	bias_reg_set(1);
	open_i2c_xpd_new(1);
	pbus_clear_reg();
	i2c_clk_sel(8);
	i2c_bbpll_set(1);
	i2cmst_reg_init();
	pwdet_reg_init_new();
	fe_reg_init();
	reg_set_new();
	tsens_read_init_new(1, phy_param[_PAR_RANGE]);
	tx_pwctrl_bg_init();
	i2c_rc_cal_set(3, 1);
	rc_cal_init();
	filter_dcap_set();
	mp_arch_rf_set_u8(_PAR_CHGP,
		(uint8_t)mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL, 1, 0xf));
	phy_i2c_init1();
	rfpll_chgp_cal();
	phy_i2c_master_cmd_mem_init();
	field = mp_arch_rf_reg_read_mask(MP_ARCH_RF_HOST_PERIF, 0, 4, 3, 0);
	if(field == 0)
		i2c_sar2_init_code(0x578);
	set_chan_freq_hw_init(2, 4);
}

/**
 * Calibrate the baseband once, then build the receive table.
 */
void bb_init(void) {
	mp_arch_rf_write(_REG_APB, mp_arch_rf_read(_REG_APB) | 0x20000000UL);
	mp_arch_rf_write(_REG_FE_FREQ,
		(mp_arch_rf_read(_REG_FE_FREQ) & 0xe7ffffffUL) | 0x10000000UL);
	if((mp_arch_rf_u32(_PAR_FLAG) & 8) == 0) {
		txdc_cal_init_new((uint16_t *)(phy_param + _PAR_TXDC), 0xf, 0, 0);
		pwdet_code_cal();
		tx_cap_init_loop();
		tsens_temp_read_new();
		tx_pwctrl_init_new(0);
		txdc_cal_pwdet_init(1, 0);
		txiq_cal_init_new();
		mp_arch_rf_set_u32(_PAR_FLAG, mp_arch_rf_u32(_PAR_FLAG) | 8);
	}
	bt_tx_gain_init();
	set_pbus_mem();
	tsens_temp_read_new();
	rxiq_cal_init(0, (uint32_t)(uintptr_t)(phy_param + _PAR_FLAG), 0);
	phy_rx_table_init();
	rfrx_sat_rst(0);
	phy_check_rx_sat();
	set_rx_gain_table(0x985, 0);
	rfrx_sat_rst(1);
	phy_reg_init();
	phy_chan_set(11, 0);
	mp_arch_rf_write(_REG_FE_FREQ, mp_arch_rf_read(_REG_FE_FREQ) & 0xe7ffffffUL);
	mp_arch_rf_write(_REG_APB, mp_arch_rf_read(_REG_APB) & 0xdfffffffUL);
	if(phy_param[_PAR_WIFI_OFF] != 0)
		phy_wifi_enable_set(0);
}

static void _copy(uint8_t *dst, unsigned dst_off, unsigned src_off, unsigned count) {
	unsigned	index;

	for(index = 0; index < count; index++)
		dst[dst_off + index] = phy_param[src_off + index];
}

/**
 * Copy one calibration row out of phy_param, or count the rows that drifted.
 * @param image Saved image, recovered before a copy
 * @param out Row written when compare is zero, read when it is not
 * @param compare Zero copies, non-zero counts differences larger than 4
 * @returns The number of differences, or 0 after a copy
 */
short phy_rfcal_data_check_value(uint32_t image, uint16_t *out, int compare) {
	uint8_t		*row;
	uint16_t	miss;
	unsigned	index;

	row = (uint8_t *)out;
	if(compare == 0) {
		rf_cal_data_recovery(image);
		for(index = 0; index < 20; index++)
			_put16(row, index * 2, mp_arch_rf_u16(_PAR_TXDC + index * 2));
		for(index = 0; index < 12; index++)
			_put16(row, 40 + index * 2, mp_arch_rf_u16(_PAR_RXDC + index * 2));
		_put16(row, 64, mp_arch_rf_u16(_PAR_IQ));
		_put16(row, 66, mp_arch_rf_u16(_PAR_IQ + 2));
		_put16(row, 68, mp_arch_rf_u16(_PAR_IQ + 4));
		_put16(row, 70, mp_arch_rf_u16(_PAR_IQ + 6));
		_put16(row, 72, mp_arch_rf_u16(_PAR_IQ + 8));
		_put16(row, 74, mp_arch_rf_u16(_PAR_IQ + 10));
		_put16(row, 76, mp_arch_rf_u16(_PAR_IQ_RX));
		_copy(row, 78, _PAR_ATTEN, 3);
		_copy(row, 81, _PAR_BT_ATT, 3);
		_copy(row, 84, _PAR_DCAP, 9);
		for(index = 0; index < 4; index++)
			_put16(row, 94 + index * 2, mp_arch_rf_u16(_PAR_PWDET + index * 2));
		return(0);
	}
	miss = 0;
	for(index = 0; index < 5; index++) {
		_gap(&miss, (int)_at16(row, index * 8), (int)mp_arch_rf_u16(_PAR_TXDC + index * 8));
		_gap(&miss, (int)_at16(row, index * 8 + 2), (int)mp_arch_rf_u16(_PAR_TXDC + 2 + index * 8));
	}
	for(index = 0; index < 4; index++)
		_gap(&miss, (int)_at16(row, 94 + index * 2), (int)mp_arch_rf_u16(_PAR_PWDET + index * 2));
	for(index = 0; index < 3; index++) {
		_gap(&miss, (int)_at16(row, 40 + index * 8), (int)mp_arch_rf_u16(_PAR_RXDC + index * 8));
		_gap(&miss, (int)_at16(row, 42 + index * 8), (int)mp_arch_rf_u16(_PAR_RXDC + 2 + index * 8));
	}
	for(index = 0; index < 2; index++)
		_iq_gap(&miss, _at16(row, 64 + index * 2), mp_arch_rf_u16(_PAR_IQ + index * 2));
	for(index = 0; index < 4; index++)
		_iq_gap(&miss, _at16(row, 68 + index * 2), mp_arch_rf_u16(_PAR_IQ_BT + index * 2));
	_iq_gap(&miss, _at16(row, 76), mp_arch_rf_u16(_PAR_IQ_RX));
	for(index = 0; index < 3; index++)
		_gap(&miss, (int)(int8_t)row[78 + index], (int)(int8_t)phy_param[_PAR_ATTEN + index]);
	for(index = 0; index < 3; index++)
		_gap(&miss, (int)(int8_t)row[81 + index], (int)(int8_t)phy_param[_PAR_BT_ATT + index]);
	for(index = 0; index < 9; index++)
		_gap(&miss, (int)row[84 + index], (int)phy_param[_PAR_DCAP + index]);
	return((short)miss);
}

/**
 * Move twelve-byte groups between the saved image and phy_param.
 * @param image Saved image
 * @param outward Non-zero writes phy_param out to the image
 */
void phy_rfcal_data_sub(uint32_t image, int outward) {
	uint8_t		*src;
	unsigned	off;
	unsigned	shift;
	uint32_t	word;

	src = (uint8_t *)image;
	for(off = 12; off != 0x1b4; off += 4) {
		if(outward == 0) {
			mp_arch_rf_set_u32(off - 12, (uint32_t)phy_byte_to_word(src + off));
			continue;
		}
		word = mp_arch_rf_u32(off - 12);
		for(shift = 0; shift != 32; shift += 8)
			src[off + shift / 8] = (uint8_t)(word >> shift);
	}
}

/**
 * Write the version and the MAC into the front of a saved image.
 * @param image Destination
 * @param version Version word stored little-endian
 */
void phy_set_mac_data(uint8_t *image, uint32_t version) {
	uint8_t		mac[8];
	unsigned	index;

	image[0] = (uint8_t)version;
	image[1] = (uint8_t)(version >> 8);
	image[2] = (uint8_t)(version >> 16);
	image[3] = (uint8_t)(version >> 24);
	memset(mac, 0, sizeof(mac));
	phy_get_mac_addr(mac);
	for(index = 0; index < 8; index++)
		image[4 + index] = mac[index];
}

/**
 * Check or store the checksum at the end of a saved image.
 * @param mode Zero stores the checksum, non-zero compares it
 * @param image Saved image
 * @param unused Unused
 * @param version Version written into the image header
 * @returns 1 when a compare finds a different checksum
 */
uint32_t phy_rfcal_data_check(int mode, uint32_t image, uint32_t unused, uint32_t version) {
	uint8_t		*row;
	uint8_t		*cursor;
	uint32_t	sum;
	uint32_t	stored;

	(void)unused;
	row = (uint8_t *)image;
	phy_set_mac_data(row, version);
	sum = 0;
	for(cursor = row; cursor != row + 0x1b4; cursor += 4)
		sum += (uint32_t)phy_byte_to_word(cursor);
	sum = ~sum;
	stored = (uint32_t)phy_byte_to_word(row + 0x1b4);
	if(mode == 0) {
		row[0x1b4] = (uint8_t)sum;
		row[0x1b5] = (uint8_t)(sum >> 8);
		row[0x1b6] = (uint8_t)(sum >> 16);
		row[0x1b7] = (uint8_t)(sum >> 24);
		return(0);
	}
	return((uint32_t)(sum != stored));
}

/**
 * Copy phy_param out into a saved image.
 * @param image Saved image
 * @returns 0
 */
uint32_t rf_cal_data_backup(uint32_t image) {
	phy_rfcal_data_sub(image, 1);
	return(0);
}

/**
 * Copy a saved image back into phy_param.
 * @param image Saved image
 */
void rf_cal_data_recovery(uint32_t image) {
	uint8_t	*row;

	row = (uint8_t *)image;
	memcpy(phy_param + _PAR_FLAG, row + 0xb0, 0xf4);
	mp_arch_rf_set_u16(0x1a, _at16(row, 0x26));
	mp_arch_rf_set_u16(0x1c, _at16(row, 0x28));
}

/**
 * Read the factory MAC words.
 * @param out Two words
 */
void esp_phy_efuse_get_mac(uint32_t *out) {
	out[0] = mp_arch_rf_read(_REG_EFUSE0);
	out[1] = mp_arch_rf_read(_REG_EFUSE1);
}

/**
 * Read the factory MAC and reverse its six bytes.
 * @param out Six-byte address
 */
void phy_get_mac_addr(uint8_t *out) {
	uint32_t	raw[2];
	uint8_t		*byte;

	raw[0] = 0;
	raw[1] = 0;
	esp_phy_efuse_get_mac(raw);
	byte = (uint8_t *)raw;
	out[0] = byte[5];
	out[1] = byte[4];
	out[2] = byte[3];
	out[3] = byte[2];
	out[4] = byte[1];
	out[5] = byte[0];
}

/**
 * Drop the radio into its closed bias state.
 */
void phy_xpd_rf(void) {
	disable_agc();
	mp_arch_rf_write(_REG_SYS_GATE, mp_arch_rf_read(_REG_SYS_GATE) & ~2UL);
	ets_delay_us(1);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 2, 6);
	mp_arch_rf_write(_REG_PMU_XPD, mp_arch_rf_read(_REG_PMU_XPD) & 0x0fffffffUL);
}

/**
 * Close the radio when a channel is still selected.
 */
void phy_close_rf_(void) {
	uint32_t	held;

	if(phy_param[_PAR_SKIP_TS] == 0)
		tsens_temp_read_new();
	if(phy_param[_PAR_HOLD] == 0)
		return;
	held = (uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_ENTER)();
	phy_dis_hw_set_freq_1();
	force_txrx_off(1);
	phy_xpd_rf();
	phy_bbpll_cal(1);
	bias_reg_reset();
	mp_arch_rf_set_u8(_PAR_CLOSED, 1);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_CRIT_EXIT)(held);
}

/**
 * Scatter the init-parameter bytes into phy_param.
 * @param init Packed init block
 */
void phy_register_param(uint8_t *init) {
	unsigned	index;

	phy_param[_PAR_LEVEL] = init[0];
	for(index = 0; index < 18; index++)
		phy_param[0x52 + index] = init[2 + index];
	phy_param[0x66] = init[0x18];
	for(index = 0; index < 14; index++)
		phy_param[0x70 + index] = init[0x19 + index];
	for(index = 0; index < 14; index++)
		phy_param[0x7e + index] = init[0x27 + index];
	for(index = 0; index < 14; index++)
		phy_param[0x8c + index] = init[0x35 + index];
	for(index = 0; index < 9; index++)
		phy_param[0x67 + index] = init[0x43 + index];
}

/**
 * Register the radio, calibrate it, and keep the saved image in step.
 * @param init Optional init block; a built-in block is used when this is null
 * @param image Saved calibration image
 * @param mode 1 reuses a matching image, anything else recalibrates
 * @returns 1 when the saved image does not match the new calibration
 */
uint32_t phy_register(uint8_t *init, uint32_t image, int mode) {
	uint8_t		builtin[128];
	uint8_t		row[104];
	uint32_t	word;
	uint32_t	saved;
	uint32_t	flags;
	uint32_t	differ;
	uint8_t		*block;

	memset(builtin, 0, sizeof(builtin));
	builtin[0] = 10;
	word = 0x5454;
	memcpy(builtin + 2, &word, 2);
	word = 0x4c505050;
	memcpy(builtin + 4, &word, 4);
	word = 0x484c4c4c;
	memcpy(builtin + 8, &word, 4);
	word = 0x3c3c3c44;
	memcpy(builtin + 12, &word, 4);
	word = 0x484c4c4c;
	memcpy(builtin + 16, &word, 4);
	phy_get_romfunc_addr();
	saved = mp_arch_rf_read(_REG_SYS_CLK);
	mp_arch_rf_write(_REG_SYS_CLK, mp_arch_rf_read(_REG_SYS_CLK) | 0x404UL);
	mp_arch_rf_write(_REG_SYS_GATE, mp_arch_rf_read(_REG_SYS_GATE) & ~2UL);
	force_txrx_off(1);
	freq_module_resetn();
	phy_dis_hw_set_freq_reg();
	i2c_master_reset();
	if(phy_param[_PAR_DBM] != 0)
		phy_set_23dbm(builtin);
	if(phy_param[_PAR_HOLD] == 0) {
		block = init;
		if(block == 0)
			block = builtin;
		phy_register_param(block);
	}
	phy_get_xtal_freq();
	differ = 0;
	if(phy_param[_PAR_HOLD] == 0) {
		differ = phy_rfcal_data_check(1, image, (uint32_t)(uintptr_t)init,
			phy_get_rf_cal_version());
		if(mode == 1) {
			if(differ == 0) {
				rf_cal_data_recovery(image);
				mp_arch_rf_set_u32(_PAR_FLAG, mp_arch_rf_u32(_PAR_FLAG) & 0xfffefd5fUL);
			}
			else {
				mode = 2;
				mp_arch_rf_set_u32(_PAR_FLAG, 0);
			}
		}
		else {
			if(differ == 0)
				phy_rfcal_data_check_value(image, (uint16_t *)row, 0);
			mp_arch_rf_set_u32(_PAR_FLAG, 0);
		}
	}
	flags = mp_arch_rf_u32(_PAR_FLAG);
	rf_init();
	bb_init();
	get_temp_init_new((int)((flags >> 5) ^ 1) & 1, (int)((flags >> 20) ^ 1) & 1);
	if(phy_param[_PAR_HOLD] == 0 && mode != 1) {
		if(differ == 0)
			differ = (uint32_t)(phy_rfcal_data_check_value(image, (uint16_t *)row, 1) != 0);
		rf_cal_data_backup(image);
		phy_rfcal_data_check(0, image, (uint32_t)(uintptr_t)init, phy_get_rf_cal_version());
	}
	if(phy_param[_PAR_HOLD] == 0 && phy_param[_PAR_OFFSET] != 0)
		phy_chan_set_offset(0);
	phy_bbpll_cal(0);
	mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL_SDM, 1, 0);
	mp_arch_rf_set_u8(_PAR_HOLD, 1);
	phy_en_hw_set_freq();
	force_txrx_off(0);
	mp_arch_rf_write(_REG_SYS_CLK, saved);
	return(differ);
}

/** @} */
