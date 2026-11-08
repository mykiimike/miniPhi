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

Print the calibration image, the analog register file and the
baseband blocks. These helpers only read the radio and the image.

@{
*/

	#define _PAR_STATUS   0xa4
	#define _PAR_TEMP     0x00
	#define _PAR_SAR_DC   0x1a
	#define _PAR_SAR_REF  0x1c
	#define _PAR_CHAN     0x11c
	#define _PAR_TEMP_0   0x12e
	#define _PAR_RC       0xe8
	#define _PAR_DCAP1    0xdc
	#define _PAR_WIFI_PWR 0xf7
	#define _PAR_BT_PWR   0xfe
	#define _PAR_RX_SAT   0x1a4
	#define _PAR_WIFI_FIX 0x123
	#define _PAR_BT_FIX   0x124
	#define _PAR_TXDC     0xa8
	#define _PAR_RXDC_BT  0x132
	#define _PAR_RXDC_WF  0x14e
	#define _PAR_RXDC_CH  0x172
	#define _PAR_PWDET    0x18e

	#define _REV_ID   "552b044"
	#define _REV_DATE "Jul 14 2026"
	#define _REV_TIME "10:37:00"

static int _s8(unsigned off) {
	return((int)(int8_t)mp_arch_rf_u8(off));
}

static int _s16(unsigned off) {
	return((int)(int16_t)mp_arch_rf_u16(off));
}

static int _u16(unsigned off) {
	return((int)mp_arch_rf_u16(off));
}

static uint32_t _bus(uint32_t sel, uint32_t part) {
	return((uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS_RD)(sel, part));
}

static void _comma(int value) {
	phy_printf("%d,", value);
}

static void _spaced(int value) {
	phy_printf("%d, ", value);
}

static void _hex(unsigned value) {
	phy_printf("0x%x,", value);
}

static void _iq(unsigned off) {
	uint8_t	pair[2];

	get_iq_value(pair, mp_arch_rf_u16(off));
	phy_printf("%d, %d; ", (int)(int8_t)pair[0], (int)(int8_t)pair[1]);
}

static void _quad(unsigned off) {
	phy_printf("%d, %d, %d, %d; ",
		_u16(off), _u16(off + 2), _u16(off + 4), _u16(off + 6));
}

static void _pairs(unsigned off, int count) {
	int	index;

	for(index = 0; index < count; index++)
		phy_printf("%d, %d; ",
			_s16(off + (unsigned)index * 4),
			_s16(off + (unsigned)index * 4 + 2));
}

static void _span(char *fmt, uint32_t start, uint32_t end) {
	uint32_t	addr;

	for(addr = start; addr < end; addr += 4)
		phy_printf(fmt, addr - start, mp_arch_rf_read(addr));
}

static void _i2c(char *fmt, uint32_t host, uint32_t block,
		uint32_t first, uint32_t last) {
	uint32_t	reg;

	for(reg = first; reg < last; reg++)
		phy_printf(fmt, reg, mp_arch_rf_reg_read(host, block, reg));
}

/**
 * Print one labelled integer.
 * @param text Label pointer
 * @param value Value printed after the label
 */
void debug_print_line(uint32_t text, uint32_t value) {
	phy_printf("%s, %d\n", (const char *)text, value);
}

/**
 * Bias reference code. This build always reports zero.
 * @returns 0
 */
uint32_t get_bias_ref_code(void) {
	return(0);
}

/**
 * Split a packed word into its two halfwords.
 * @param out Destination of two halfwords, high half first
 * @param packed Word to split
 */
void get_dc_value(uint16_t *out, uint32_t packed) {
	out[0] = (uint16_t)(packed >> 16);
	out[1] = (uint16_t)packed;
}

/**
 * Format the PHY revision into phy_version_str.
 * @returns The version buffer
 */
char *get_phy_version_str(void) {
	sprintf(phy_version_str, "%d,%s,%s,%s",
		phy_get_rf_cal_version(), _REV_ID, _REV_DATE, _REV_TIME);
	return(phy_version_str);
}

/**
 * Print the PHY revision.
 */
void phy_version_print(void) {
	phy_printf("phy_version: %d, %s, %s, %s\n",
		phy_get_rf_cal_version(), _REV_ID, _REV_DATE, _REV_TIME);
}

/**
 * Read the 3.3 V rail through the SAR and scale it.
 * @returns The scaled reading, low 16 bits
 */
uint32_t phy_get_vdd33(void) {
	int	vol;

	pbus_debugmode();
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS)(5, 1, 0x80);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 13, 7, 7, 1);
	vol = (int)get_sar2_vol_new(3);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_SAR, 1, 13, 7, 7, 0);
	MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_PBUS)(5, 1, 0);
	pbus_workmode();
	return((uint32_t)((uint16_t)((vol * 206) / 100)));
}

/**
 * Print the PLL cap and DAC byte of every 2.4 GHz channel.
 * @param verbose Non-zero prints the two rows
 */
void rfpll_cap_check(int verbose) {
	uint16_t	cap[14];
	uint8_t		dac[14];
	int		channel;

	for(channel = 1; channel <= 14; channel++) {
		phy_chan_set((uint32_t)(uint16_t)channel, 0);
		cap[channel - 1] = (uint16_t)read_pll_cap();
		dac[channel - 1] = (uint8_t)mp_arch_rf_reg_read(MP_ARCH_RF_HOST_RFPLL, 1, 6);
	}
	if(verbose == 0)
		return;
	phy_printf("ir_cap_ext:");
	for(channel = 0; channel < 14; channel++)
		_comma((int)cap[channel]);
	phy_printf("\n");
	phy_printf("ir_dac_ext:");
	for(channel = 0; channel < 14; channel++)
		_comma((int)dac[channel]);
	phy_printf("\n");
}

/**
 * Dump every analog register the bring-up reads back.
 */
void phy_i2c_check(void) {
	_i2c("i2c_bias %02d: 0x%x\n", MP_ARCH_RF_HOST_BIAS, 1, 0, 4);
	_i2c("i2c_bbpll %02d: 0x%x\n", MP_ARCH_RF_HOST_BBPLL, 0, 0, 11);
	_i2c("i2c_txrf %02d: 0x%x\n", MP_ARCH_RF_HOST_SAR, 1, 1, 16);
	_i2c("i2c_bbtop %02d: 0x%x\n", MP_ARCH_RF_HOST_FILTER, 1, 0, 57);
	_i2c("i2c_rfpll %02d: 0x%x\n", MP_ARCH_RF_HOST_RFPLL, 1, 0, 19);
	_i2c("i2c_rfpll_sdm %02d: 0x%x\n", MP_ARCH_RF_HOST_RFPLL_SDM, 1, 0, 7);
	_i2c("i2c_dig_reg %02d: 0x%x\n", MP_ARCH_RF_HOST_DIG, 0, 0, 15);
	_i2c("i2c_ulp %02d: 0x%x\n", MP_ARCH_RF_HOST_ULP, 0, 0, 11);
	_i2c("i2c_perif %02d: 0x%x\n", MP_ARCH_RF_HOST_PERIF, 0, 0, 9);
}

/**
 * Read every word of the radio register blocks and print it.
 */
void phy_reg_check(void) {
	_span("pmu 0x%x: 0x%x\n", 0x600B0000, 0x600B01A4);
	_span("i2c_mst 0x%x: 0x%x\n", 0x600AF800, 0x600AF838);
	_span("FECTRL 0x%x: 0x%x\n", 0x600A0800, 0x600A09CC);
	_span("FECOEX 0x%x: 0x%x\n", 0x600A0000, 0x600A00E8);
	_span("FEDATA 0x%x: 0x%x\n", 0x600A0400, 0x600A04A4);
	_span("PCR 0x%x: 0x%x\n", 0x60096000, 0x60096128);
	_span("MODEM_SYSCON 0x%x: 0x%x\n", 0x600A9800, 0x600A9828);
	_span("MODEM_LPCON 0x%x: 0x%x\n", 0x600AF000, 0x600AF030);
	_span("agc 0x%x: 0x%x\n", 0x600A7000, 0x600A71E4);
	_span("bb 0x%x: 0x%x\n", 0x600A7C00, 0x600A7CF0);
	_span("bbtx 0x%x: 0x%x\n", 0x600A7400, 0x600A743C);
	_span("brx 0x%x: 0x%x\n", 0x600A8000, 0x600A809C);
	_span("nrx 0x%x: 0x%x\n", 0x600A7800, 0x600A7A2C);
	_span("btagc 0x%x: 0x%x\n", 0x600A2800, 0x600A28FC);
	_span("bt_v3_2 0x%x: 0x%x\n", 0x600A2000, 0x600A2158);
	_span("btmac 0x%x: 0x%x\n", 0x600A1000, 0x600A1B30);
	_span("802154_reg 0x%x: 0x%x\n", 0x600A3000, 0x600A3184);
	_span("zbbb 0x%x: 0x%x\n", 0x600A2C00, 0x600A2C4C);
	_span("apb 0x%x: 0x%x\n", 0x6000E000, 0x6000E06C);
}

/**
 * Print the wifi and Bluetooth transmit gain tables.
 */
void phy_tx_gain_print(void) {
	uint8_t		wifi_bytes[32];
	uint16_t	wifi_second[32];
	uint16_t	wifi_code[32];
	uint16_t	bt_code[16];
	uint16_t	bt_second[16];
	uint8_t		bt_bytes[16];
	int		index;

	wifi_get_tx_tab(mp_arch_rf_u16(_PAR_CHAN), (uint32_t)(uintptr_t)wifi_bytes,
		(uint32_t)(uintptr_t)wifi_second, (uint32_t)(uintptr_t)wifi_code, 0);
	bt_get_tx_tab((uint32_t)(uintptr_t)bt_code, (uint32_t)(uintptr_t)bt_second,
		(uint32_t)(uintptr_t)bt_bytes, 0);
	phy_printf("wifi_init_power:");
	for(index = 0; index < 18; index++)
		_comma(_s8(0x52 + (unsigned)index));
	phy_printf("\nwifi_pagain:");
	for(index = 0; index < 32; index++)
		_hex(wifi_code[index]);
	phy_printf("\nwifi_bbgain:");
	for(index = 0; index < 32; index++)
		_hex(wifi_second[index]);
	phy_printf("\nwifi_diggain:");
	for(index = 0; index < 32; index++)
		_comma((int)(int8_t)wifi_bytes[index]);
	phy_printf("\nwifi_correct_power:%d\n", _s8(_PAR_WIFI_FIX));
	phy_printf("bt_pagain:");
	for(index = 0; index < 16; index++)
		_hex(bt_code[index]);
	phy_printf("\nbt_bbgain:");
	for(index = 0; index < 16; index++)
		_hex(bt_second[index]);
	phy_printf("\nbt_diggain:");
	for(index = 0; index < 16; index++)
		_comma((int)(int8_t)bt_bytes[index]);
	phy_printf("\nbt_correct_power:%d\n", _s8(_PAR_BT_FIX));
}

/**
 * Print the calibration image: noise, power, IQ and DC rows.
 */
void phy_cal_print(void) {
	int16_t	noise[3];
	int	index;
	int	vdd;

	for(index = 0; index < 3; index++) {
		ets_delay_us(1000);
		noise[index] = (int16_t)read_hw_noisefloor();
	}
	phy_version_print();
	vdd = (int)phy_get_vdd33();
	tsens_temp_read_new();
	phy_printf("param_flag: 0x%x\n", mp_arch_rf_u32(_PAR_STATUS));
	phy_printf("vdd33=%d, temp_code=%d, offset=%d, temp_code_init=%d\n",
		vdd, _s16(_PAR_TEMP), 0, _s16(_PAR_TEMP_0));
	phy_printf("rc_dout, %d; wifi: %d, %d, %d, %d, bt: %d, %d, %d, %d\n",
		mp_arch_rf_u8(_PAR_RC), mp_arch_rf_u8(_PAR_RC + 1),
		mp_arch_rf_u8(_PAR_RC + 2), mp_arch_rf_u8(_PAR_RC + 3),
		mp_arch_rf_u8(_PAR_RC + 4), mp_arch_rf_u8(_PAR_RC + 5),
		mp_arch_rf_u8(_PAR_RC + 6), mp_arch_rf_u8(_PAR_RC + 7),
		mp_arch_rf_u8(_PAR_RC + 8));
	phy_printf("RX_NOISEFLOOR, ");
	for(index = 0; index < 3; index++)
		_comma((int)noise[index]);
	phy_printf("\n");
	phy_printf("DCAP_STG1, ");
	_spaced((int)mp_arch_rf_u8(_PAR_DCAP1));
	_spaced((int)mp_arch_rf_u8(_PAR_DCAP1 + 2));
	_spaced((int)mp_arch_rf_u8(_PAR_DCAP1 + 4));
	phy_printf("\n");
	phy_printf("DCAP_STG2, ");
	_spaced((int)mp_arch_rf_u8(_PAR_DCAP1 + 1));
	_spaced((int)mp_arch_rf_u8(_PAR_DCAP1 + 3));
	_spaced((int)mp_arch_rf_u8(_PAR_DCAP1 + 5));
	phy_printf("\n");
	phy_printf("wifi_cal_power, %d\n", _s8(_PAR_WIFI_PWR));
	phy_printf("wifi_cal_atten, ");
	_spaced(_s8(0xf4));
	_spaced(_s8(0xf5));
	_spaced(_s8(0xf6));
	phy_printf("\n");
	phy_printf("wifi_pwctrl_atten, ");
	_spaced(_s8(0xf1));
	_spaced(_s8(0xf2));
	_spaced(_s8(0xf3));
	phy_printf("\n");
	phy_printf("TXIQ, ");
	_iq(0xd0);
	_iq(0xd2);
	phy_printf("\n");
	phy_printf("LB_TXIQ, ");
	_iq(0xe6);
	phy_printf("\n");
	phy_printf("TXDC, ");
	for(index = 0; index < 5; index++)
		_quad(_PAR_TXDC + (unsigned)index * 8);
	phy_printf("\n");
	phy_printf("bt_cal_power, %d\n", _s8(_PAR_BT_PWR));
	phy_printf("bt_cal_atten, ");
	_spaced(_s8(0xf8));
	_spaced(_s8(0xf9));
	_spaced(_s8(0xfa));
	phy_printf("\n");
	phy_printf("bt_pwctrl_atten, ");
	_spaced(_s8(0xfb));
	_spaced(_s8(0xfc));
	_spaced(_s8(0xfd));
	phy_printf("\n");
	phy_printf("BT_TXIQ, ");
	_iq(0x102);
	phy_printf("\n");
	phy_printf("BT_TXDC, ");
	_quad(0x104);
	phy_printf("\n");
	phy_printf("RXIQ, ");
	_iq(0xd4);
	_iq(0xd6);
	phy_printf("\n");
	phy_printf("RXDC_RFRX_BT, ");
	_pairs(_PAR_RXDC_BT, 7);
	phy_printf("\n");
	phy_printf("RXDC_RFRX_WIFI, ");
	_pairs(_PAR_RXDC_WF, 8);
	phy_printf("\n");
	phy_printf("RXDC_CHAN_WIFI, ");
	_pairs(_PAR_RXDC_CH, 7);
	phy_printf("\n");
	wifi_set_tx_gain(mp_arch_rf_u16(_PAR_CHAN), 0);
	phy_printf("sar_dc_code=%d, sar_ref_code=%d\n",
		_u16(_PAR_SAR_DC), _u16(_PAR_SAR_REF));
	phy_tx_gain_print();
	phy_printf("txdc_cal_pwdet,");
	for(index = 0; index < 4; index++)
		_comma(_u16(_PAR_PWDET + (unsigned)index * 2));
	phy_printf("\n");
	phy_printf("rx_sat=%d\n", (int)mp_arch_rf_u8(_PAR_RX_SAT));
}

/**
 * Print the parallel-bus receiver, transmitter and DC words.
 */
void pbus_print(void) {
	uint32_t	a;
	uint32_t	b;
	uint32_t	c;
	uint32_t	d;

	a = _bus(0, 1);
	b = _bus(0, 2);
	phy_printf("rfrx:0x%x,0x%x;", a, b);
	a = _bus(4, 1);
	b = _bus(4, 2);
	c = _bus(5, 1);
	phy_printf("rftx:0x%x,0x%x,0x%x;", a, b, c);
	a = _bus(1, 1);
	b = _bus(1, 2);
	phy_printf("bb:0x%x,0x%x;", a, b);
	a = _bus(2, 1);
	b = _bus(3, 1);
	c = _bus(2, 2);
	d = _bus(3, 2);
	phy_printf("dco:%d,%d,%d,%d\n", (int)a, (int)b, (int)c, (int)d);
}

/** @} */
