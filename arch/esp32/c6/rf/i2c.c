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

Analog I2C for the C6 radio: clocks, bias, the channel filter,
the BBPLL and the command memory.

@{
*/

#define _REG_ADC_DAC  0x600A046CUL
#define _REG_I2C_SEL  0x600AF820UL
#define _REG_I2C_CLK0 0x600AF824UL
#define _REG_I2C_CLK1 0x600AF828UL
#define _REG_I2C_CLK2 0x600AF82CUL
#define _REG_I2C_CMD  0x600AFC00UL

#define _PAR_BBPLL     0x4a
#define _PAR_DCAP_A    0xe9
#define _PAR_DCAP_B    0xea
#define _PAR_FILT_A    0xed
#define _PAR_FILT_B    0xee
#define _PAR_FILT_TEST 0xef
#define _PAR_FILT_C    0xf0
#define _PAR_PLL_CAP   0x196

#define _SEL_KEEP 0xfffe000fUL
#define _SEL_MODE 0x1f00UL

void tsens_temp_read_new(void);

/**
 * Pack a command-memory word: data in bits 23:16, register in 15:8, host in 7:0.
 * @param host Analog host
 * @param reg Register index
 * @param data Byte to send
 * @returns The packed word
 */
static uint32_t _i2c_cmd(uint32_t host, uint32_t reg, uint32_t data) {
	return(host | (reg << 8) | (data << 16));
}

/**
 * Store one command at a 4-byte index.
 * @param index Word index in the command memory
 * @param host Analog host
 * @param reg Register index
 * @param data Byte to send
 */
static void _cmd_store(unsigned index, uint32_t host, uint32_t reg, uint32_t data) {
	mp_arch_rf_write(_REG_I2C_CMD + (unsigned long)index * 4UL, _i2c_cmd(host, reg, data));
}

/**
 * Write one filter register from a phy_param byte.
 * @param reg Register index
 * @param off Byte offset in phy_param
 * @param extra Bits merged into the byte
 */
static void _filt(uint32_t reg, unsigned off, uint32_t extra) {
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, reg, (uint32_t)phy_param[off] | extra);
}

/**
 * Apply the two clock fields of one divider.
 * Bits 10:6 take (clock << 4) & 0x7c0, then bits 5:0 take (clock >> 1) & 0x3f.
 * @param addr Clock register
 * @param hi Bits 10:6
 * @param lo Bits 5:0
 */
static void _clk_pair(unsigned long addr, unsigned long hi, unsigned long lo) {
	unsigned long word;

	word = mp_arch_rf_read(addr);
	mp_arch_rf_write(addr, (word & ~0x7c0UL) | hi);
	word = mp_arch_rf_read(addr);
	mp_arch_rf_write(addr, (word & ~0x3fUL) | lo);
}

/**
 * Select the ADC sample rate.
 * @param rate Zero selects the slow path
 */
void adc_rate_set(uint32_t rate) {
	unsigned long word;

	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BBPLL, 0, 4, 3, 2, (uint32_t)(rate == 0) << 1);
	word = mp_arch_rf_read(_REG_ADC_DAC);
	mp_arch_rf_write(_REG_ADC_DAC, (word & 0xdfffffffUL) | (((unsigned long)rate & 1UL) << 29));
}

/**
 * Restore the analog bias fields.
 */
void bias_reg_reset(void) {
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BIAS, 1, 0, 3, 0, 7);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BIAS, 1, 1, 3, 0, 7);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BIAS, 1, 0, 7, 4, 7);
}

/**
 * Program the analog bias fields.
 */
void bias_reg_set(int enable) {
	(void)enable;
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BIAS, 1, 0, 3, 0, 0xf);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BIAS, 1, 1, 3, 0, 0xc);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BIAS, 1, 0, 7, 4, 0xc);
}

/**
 * Select the DAC clock rate.
 * @param rate Bit stored at position 13
 */
void dac_rate_set(uint32_t rate) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_ADC_DAC);
	mp_arch_rf_write(_REG_ADC_DAC, (word & 0xffffdfffUL) | (((unsigned long)rate & 1UL) << 13));
}

/**
 * Load the channel-filter and DCAP coefficients.
 */
void filter_dcap_set(void) {
	_filt(0x14, _PAR_FILT_A, 0);
	_filt(0x15, _PAR_FILT_A, 0);
	_filt(0x16, _PAR_FILT_B, 0);
	_filt(0x17, _PAR_FILT_B, 0);
	_filt(0x18, _PAR_FILT_A, 0);
	_filt(0x19, _PAR_FILT_A, 0);
	_filt(0x1c, _PAR_FILT_C, 0);
	_filt(0x1d, _PAR_FILT_C, 0);
	_filt(0x1e, _PAR_FILT_C, 0x40);
	_filt(0x1f, _PAR_FILT_C, 0);
	_filt(4, _PAR_DCAP_A, 0);
	_filt(5, _PAR_DCAP_A, 0);
	_filt(6, _PAR_DCAP_B, 0);
	_filt(7, _PAR_DCAP_B, 0);
	_filt(0xc, _PAR_DCAP_A, 0);
	_filt(0xd, _PAR_DCAP_A, 0);
	_filt(0xe, _PAR_DCAP_B, 0);
	_filt(0xf, _PAR_DCAP_B, 0);
}

/**
 * Turn the BBPLL configuration on, or write the saved byte back.
 * @param enable Non-zero stores the current register into phy_param
 */
void i2c_bbpll_set(int enable) {
	if(enable != 0) {
		mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_BBPLL, 0, 4, 3, 2, 2);
		phy_param[_PAR_BBPLL] = (uint8_t)mp_arch_rf_reg_read(MP_ARCH_RF_HOST_BBPLL, 0, 4);
		return;
	}
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_BBPLL, 0, 4, phy_param[_PAR_BBPLL]);
}

/**
 * Program the three I2C clock dividers. Each register is updated twice.
 * @param clock Divider code
 */
void i2c_clk_sel(uint32_t clock) {
	unsigned long hi;
	unsigned long lo;

	hi = ((unsigned long)clock << 4) & 0x7c0UL;
	lo = ((unsigned long)clock >> 1) & 0x3fUL;
	_clk_pair(_REG_I2C_CLK0, hi, lo);
	_clk_pair(_REG_I2C_CLK1, hi, lo);
	_clk_pair(_REG_I2C_CLK2, hi, lo);
}

/**
 * Set the RC calibration fields. The last field is 9.
 * @param field_a Bits 5:4 of register 0x0b
 * @param field_b Bits 7:3 of register 8
 */
void i2c_rc_cal_set(uint32_t field_a, uint32_t field_b) {
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 0xb, 5, 4, field_a);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 8, 7, 3, field_b);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 0xd, 5, 2, 9);
}

/**
 * Set the RC calibration fields, including the last one.
 * @param final_field Bits 5:2 of register 0x0d
 * @param field_a Bits 5:4 of register 0x0b
 * @param field_b Bits 7:3 of register 8
 */
void i2c_rc_set_reg(uint32_t final_field, uint32_t field_a, uint32_t field_b) {
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 0xb, 5, 4, field_a);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 8, 7, 3, field_b);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 0xd, 5, 2, final_field);
}

/**
 * Pack a command-memory word.
 * @param host Analog host
 * @param reg Register index
 * @param data Byte to send
 * @returns The packed word
 */
uint32_t phy_encode_i2c_master(uint32_t host, int reg, int data) {
	return(_i2c_cmd(host, (uint32_t)reg, (uint32_t)data));
}

/**
 * No analog data to collect.
 */
void phy_get_i2c_data(void) {
}

/**
 * First radio I2C bring-up.
 */
void phy_i2c_init1(void) {
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_SAR, 1, 2, 0x72);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_SAR, 1, 3, 0x3b);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_SAR, 1, 4, 0xa4);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_SAR, 1, 5, 2);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_SAR, 1, 6, 0x88);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_SAR, 1, 7, 0xb8);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_SAR, 1, 8, 0x81);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_RFPLL, 1, 0, 0x68);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_RFPLL, 1, 4, 0x28);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_RFPLL, 1, 0xf, phy_param[_PAR_PLL_CAP]);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_RFPLL, 1, 0xb, 0x44);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 2, 0x26);
}

/**
 * Parallel I2C bring-up.
 * Six columns of seven bytes: host, register, then the payload bytes.
 */
void phy_i2c_init2(void) {
	uint8_t col[6][8] = {
		{ 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0 },
		{ 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0 },
		{ 0x72, 0x3b, 0xa4, 0x02, 0x88, 0xb8, 0x81, 0 },
		{ 0x62, 0x62, 0x62, 0x62, 0x67, 0x67, 0x67, 0 },
		{ 0x00, 0x04, 0x0f, 0x0b, 0x02, 0x02, 0x02, 0 },
		{ 0x68, 0x28, 0x00, 0x44, 0x26, 0x26, 0x26, 0 },
	};
	unsigned long sel;
	unsigned long mask;

	mp_arch_rf_i2c_enter();
	col[5][2] = phy_param[_PAR_PLL_CAP];
	mask = mp_arch_rf_i2c_mst0_mask(MP_ARCH_RF_HOST_SAR) & 0xffff0UL & 0x1fff0UL;
	sel = mp_arch_rf_read(_REG_I2C_SEL);
	mp_arch_rf_write(_REG_I2C_SEL, (sel & _SEL_KEEP) | mask);
	i2c_paral_write_num(col[0], col[1], col[2], col[3], col[4], col[5], 7, 0);
	sel = mp_arch_rf_read(_REG_I2C_SEL);
	mp_arch_rf_write(_REG_I2C_SEL, (sel & _SEL_KEEP) | _SEL_MODE);
	mp_arch_rf_i2c_exit();
}

/**
 * Fill the command memory with the radio setup and the saved coefficients.
 */
void phy_i2c_master_cmd_mem_init(void) {
	_cmd_store(0, MP_ARCH_RF_HOST_FILTER, 2, 0x06);
	_cmd_store(1, MP_ARCH_RF_HOST_SAR, 2, 0x72);
	_cmd_store(2, MP_ARCH_RF_HOST_SAR, 3, 0x3b);
	_cmd_store(3, MP_ARCH_RF_HOST_SAR, 4, 0xa4);
	_cmd_store(4, MP_ARCH_RF_HOST_SAR, 5, 2);
	_cmd_store(5, MP_ARCH_RF_HOST_SAR, 6, 0x88);
	_cmd_store(6, MP_ARCH_RF_HOST_SAR, 7, 0xb8);
	_cmd_store(7, MP_ARCH_RF_HOST_SAR, 8, 0x81);
	_cmd_store(8, MP_ARCH_RF_HOST_RFPLL, 0, 0x68);
	_cmd_store(9, MP_ARCH_RF_HOST_RFPLL, 4, 0x28);
	_cmd_store(10, MP_ARCH_RF_HOST_RFPLL, 0xf, phy_param[_PAR_PLL_CAP]);
	_cmd_store(11, MP_ARCH_RF_HOST_FILTER, 2, 0x26);
	_cmd_store(12, MP_ARCH_RF_HOST_FILTER, 4, phy_param[_PAR_DCAP_A]);
	_cmd_store(13, MP_ARCH_RF_HOST_FILTER, 5, phy_param[_PAR_DCAP_A]);
	_cmd_store(14, MP_ARCH_RF_HOST_FILTER, 6, phy_param[_PAR_DCAP_B]);
	_cmd_store(15, MP_ARCH_RF_HOST_FILTER, 7, phy_param[_PAR_DCAP_B]);
	_cmd_store(16, MP_ARCH_RF_HOST_FILTER, 0xc, phy_param[_PAR_DCAP_A]);
	_cmd_store(17, MP_ARCH_RF_HOST_FILTER, 0xd, phy_param[_PAR_DCAP_A]);
	_cmd_store(18, MP_ARCH_RF_HOST_FILTER, 0xe, phy_param[_PAR_DCAP_B]);
	_cmd_store(19, MP_ARCH_RF_HOST_FILTER, 0xf, phy_param[_PAR_DCAP_B]);
	_cmd_store(20, MP_ARCH_RF_HOST_FILTER, 0x14, phy_param[_PAR_FILT_A]);
	_cmd_store(21, MP_ARCH_RF_HOST_FILTER, 0x15, phy_param[_PAR_FILT_A]);
	_cmd_store(22, MP_ARCH_RF_HOST_FILTER, 0x16, phy_param[_PAR_FILT_B]);
	_cmd_store(23, MP_ARCH_RF_HOST_FILTER, 0x17, phy_param[_PAR_FILT_B]);
	_cmd_store(24, MP_ARCH_RF_HOST_FILTER, 0x18, phy_param[_PAR_FILT_A]);
	_cmd_store(25, MP_ARCH_RF_HOST_FILTER, 0x19, phy_param[_PAR_FILT_A]);
	_cmd_store(26, MP_ARCH_RF_HOST_FILTER, 0x1c, phy_param[_PAR_FILT_C]);
	_cmd_store(27, MP_ARCH_RF_HOST_FILTER, 0x1d, phy_param[_PAR_FILT_C]);
	_cmd_store(28, MP_ARCH_RF_HOST_FILTER, 0x1e, (uint32_t)phy_param[_PAR_FILT_C] | 0x40);
	_cmd_store(29, MP_ARCH_RF_HOST_FILTER, 0x1f, phy_param[_PAR_FILT_C]);
}

/**
 * Describe an eight-byte command and its word count.
 * @param config Eight command bytes
 * @param count Receives 2
 */
void phy_i2c_master_command_mem_cfg(uint8_t *config, uint32_t *count) {
	config[3] = 1;
	config[4] = 1;
	config[5] = 1;
	config[7] = 1;
	config[0] = 0;
	config[1] = 0;
	config[2] = 0;
	config[6] = 0x1d;
	*count = 2;
}

/**
 * Store one word in the command memory. The index counts words, not bytes.
 * @param index Word index
 * @param value Word to store
 */
void phy_i2c_master_fill(int index, uint32_t value) {
	mp_arch_rf_write(_REG_I2C_CMD + (unsigned long)index * 4UL, value);
}

/**
 * Describe a six-byte command.
 * @param config Six command bytes
 */
void phy_i2c_master_mem_cfg(uint8_t *config) {
	config[0] = 0;
	config[1] = 0;
	config[3] = 1;
	config[4] = 0x1d;
	config[2] = 1;
	config[5] = 1;
}

/**
 * Copy SAR register 2 into command word 1.
 */
void phy_i2c_master_mem_txcap(void) {
	uint32_t value;

	value = mp_arch_rf_reg_read(MP_ARCH_RF_HOST_SAR, 1, 2);
	_cmd_store(1, MP_ARCH_RF_HOST_SAR, 2, value);
}

const mp_arch_rf_i2c_t mp_arch_rf_i2c = {
	.base = 0x600AF800UL,
	.sel_off = 0x20UL,
	.sel_mask = 0xffffe00fUL,
	.sel_value = 0x1f00UL,
	.mst1_hosts = 0x223UL,
	.rmask_off = 0x1cUL,
};

/**
 * Read one analog register through the master.
 * @param block Analog block
 * @param rmask Read mask
 * @param host Master host index
 * @param reg Register index
 * @returns Bits 23:16 of the result
 */
uint32_t ram_chip_i2c_readReg_org(uint32_t block, uint32_t rmask, int host, int reg) {
	return(mp_arch_rf_i2c_read(&mp_arch_rf_i2c, block, rmask, host, reg));
}

/**
 * Select the analog master for a block.
 * @param block Analog block
 * @returns 1 when the block sits on master 1, otherwise 0
 */
bool ram_get_i2c_hostid(int block) {
	return(mp_arch_rf_i2c_hostid(&mp_arch_rf_i2c, block));
}

/* Halfword per analog host 0x61 through 0x6d. */
static const uint16_t _i2c_mst0_mask[13] = {
	0x0400, 0x0080, 0x0040, 0x0000, 0x0000, 0x0200, 0x0010,
	0x0000, 0x0800, 0x0100, 0x0020, 0x0000, 0x1000,
};

static const uint16_t _i2c_read_mask[13] = {
	0x0100, 0x0020, 0x0010, 0x0000, 0x0000, 0x0080, 0x0004,
	0x0000, 0x0200, 0x0040, 0x0008, 0x0000, 0x0400,
};

/**
 * Master-0 mask for one host.
 * @param host Analog host
 * @returns The mask, or 0 outside 0x61..0x6d
 */
uint16_t ram_get_i2c_mst0_mask(int host) {
	uint32_t index;

	index = (uint32_t)(host - 0x61) & 0xff;
	if(index > 12)
		return(0);
	return(_i2c_mst0_mask[index]);
}

/**
 * Read mask for one host.
 * @param host Analog host
 * @returns The mask, or 0 outside 0x61..0x6d
 */
uint16_t ram_get_i2c_read_mask(int host) {
	uint32_t index;

	index = (uint32_t)(host - 0x61) & 0xff;
	if(index > 12)
		return(0);
	return(_i2c_read_mask[index]);
}

/**
 * Pick the frequency field from the temperature and the channel frequency.
 * @param freq Channel frequency from chan_to_freq
 */
void set_freq_i2c(uint32_t freq) {
	int16_t temp;
	uint32_t field;

	tsens_temp_read_new();
	temp = (int16_t)mp_arch_rf_u16(0);
	if(temp > 54)
		field = 7;
	else if(temp < 0)
		field = 5;
	else
		field = 8;
	if(freq > 0x9a6)
		field = 5;
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 7, 3, 0, field);
}

/**
 * Point the filter band registers at the test byte or the normal byte.
 * @param enable Non-zero selects the test coefficient
 */
void test_filter_band_set(int enable) {
	unsigned off;

	off = enable == 0 ? _PAR_FILT_C : _PAR_FILT_TEST;
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 0x1c, phy_param[off]);
	mp_arch_rf_reg_write(MP_ARCH_RF_HOST_FILTER, 1, 0x1d, phy_param[off]);
}

/**@}*/
