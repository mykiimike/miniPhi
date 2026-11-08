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

Public radio entry points: channel selection, antenna hooks, watchdog and parameter bytes.

@{
*/

#define _REG_WDT     0x600A7C40UL
#define _REG_WDT_ST  0x600A7C08UL

#define _PAR_TPW     0x06
#define _PAR_BT      0x0b
#define _PAR_LEVEL   0x2c
#define _PAR_DBM     0x4e
#define _PAR_BLE     0x19b
#define _PAR_TSENS   0x19d
#define _PAR_INIT    0x19e
#define _PAR_TRACK   0x1a5
#define _PAR_LIMIT   0x1a6
#define _PAR_SCALE   0x1a7

#define _DBM_BYTE    0x5c

/**
 * Select a channel.
 * @param channel Channel or frequency
 * @param mode Mode byte
 */
void RFChannelSel(uint32_t channel, int mode) {
	phy_chan_set(channel, mode);
}

/**
 * Change channel from a megahertz value. The middle two arguments are unused.
 * @param mhz Frequency in megahertz
 * @param unused_a Unused
 * @param unused_b Unused
 * @param mode Mode forwarded to the channel routine
 * @returns 0
 */
uint32_t phy_change_channel(uint32_t mhz, uint32_t unused_a, uint32_t unused_b, uint32_t mode) {
	(void)unused_a;
	(void)unused_b;
	set_chanfreq(mhz, mode);
	return(0);
}

/** Enable RIFS reception. */
void phy_rx_rifs_en(void) {
	wifi_rifs_mode_en();
}

/**
 * Store the current-level byte.
 * @param level Value whose low byte is stored
 */
void phy_current_level_set(mp_arch_rf_code_t level) {
	phy_param[_PAR_LEVEL] = (uint8_t)(uintptr_t)level;
}

/** Retained empty entry. */
void phy_bbpll_en_usb(void) {
}

/**
 * Store the Bluetooth power-track byte.
 * @param enable Byte stored at phy_param + 11
 */
void phy_bt_power_track(uint8_t enable) {
	phy_param[_PAR_BT] = enable;
}

/** Power the temperature sensor once. */
void phy_xpd_tsens(void) {
	if(phy_param[_PAR_TSENS] == 0)
		phy_set_tsens_power(0);
	phy_param[_PAR_TSENS] = 1;
}

/**
 * Put bit 0 of enable into the watchdog reset bit.
 * @param enable Low bit is written to bit 31
 */
void bb_wdt_rst_enable(int enable) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_WDT);
	mp_arch_rf_write(_REG_WDT, (word & 0x7fffffffUL) | ((unsigned long)enable << 31));
}

/**
 * Put bit 0 of enable into the watchdog interrupt bit.
 * @param enable Low bit is written to bit 30
 */
void bb_wdt_int_enable(uint32_t enable) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_WDT);
	mp_arch_rf_write(_REG_WDT, (word & 0xbfffffffUL) | (((unsigned long)enable & 1UL) << 30));
}

/** Set the watchdog timeout bit. */
void bb_wdt_timeout_clear(void) {
	mp_arch_rf_set(_REG_WDT, 0x20000000UL);
}

/**
 * @returns The watchdog status register
 */
uint32_t bb_wdt_get_status(void) {
	return((uint32_t)mp_arch_rf_read(_REG_WDT_ST));
}

/**
 * @returns 345
 */
uint32_t phy_get_rf_cal_version(void) {
	return(0x159);
}

/** Retained empty entry. */
void noise_check_loop(void) {
}

/** Run the wake initialisation. */
void phy_wakeup_init(void) {
	phy_wakeup_init_();
}

/** Close the radio. */
void phy_close_rf(void) {
	phy_close_rf_();
}

/** Run one background power-control pass. */
void tx_pwctrl_background(void) {
	tx_pwctrl_background_();
}

/** Publish the transmit state. */
void esp_tx_state_out(void) {
	esp_tx_state_out_rom();
}

/** Apply the default antenna configuration. */
void ant_dft_cfg(void) {
	ant_dft_cfg_rom();
}

/** Apply the Wi-Fi transmit antenna configuration. */
void ant_wifitx_cfg(void) {
	ant_wifitx_cfg_rom();
}

/** Apply the Wi-Fi receive antenna configuration. */
void ant_wifirx_cfg(void) {
	ant_wifirx_cfg_rom();
}

/** Apply the Bluetooth transmit antenna configuration. */
void ant_bttx_cfg(void) {
	ant_bttx_cfg_rom();
}

/** Apply the Bluetooth receive antenna configuration. */
void ant_btrx_cfg(void) {
	ant_btrx_cfg_rom();
}

/**
 * Apply both transmit antenna configurations.
 * @param sel Selection passed to both ROM entries
 */
void ant_tx_cfg(uint32_t sel) {
	ant_wifitx_cfg_rom(sel, sel);
	ant_bttx_cfg_rom(sel);
}

/**
 * Apply both receive antenna configurations.
 * @param a First selection
 * @param b Second selection
 * @param c Third selection
 */
void ant_rx_cfg(uint32_t a, uint32_t b, uint32_t c) {
	ant_wifirx_cfg_rom(a, b, c);
	ant_btrx_cfg_rom(a, b, c);
}

/**
 * Dump one channel through the ROM entry.
 * @param channel Channel forwarded as the second argument
 */
void phy_chan_dump_cfg(uint32_t channel) {
	phy_chan_dump_cfg_rom(1, channel, 1, 1, 1);
}

/**
 * Save or restore the digital registers. Both arguments are forwarded.
 * @param save Zero writes the buffer back
 * @param buf Register image
 * @returns The word count from the backup routine
 */
uint32_t phy_dig_reg_backup(int save, uint32_t *buf) {
	return(phy_dig_reg_backup_new(save, buf));
}

/** Program the channel filter. */
void phy_chan_filt_set(void) {
	phy_chan_filt_set_rom();
}

/** Apply the CCA thresholds. */
void set_cca(void) {
	set_cca_rom();
}

/** Apply the receive sensitivity. */
void set_rx_sense(void) {
	set_rx_sense_rom();
}

/** Force the receive gain. */
void rx_gain_force(void) {
	rx_gain_force_rom();
}

/**
 * Store the BLE channel base.
 * @param base Byte stored at phy_param + 0x19b
 */
void phy_ble_set_chan_base(uint8_t base) {
	phy_param[_PAR_BLE] = base;
}

/**
 * Store bit 0 of the init parameter.
 * @param flag Low bit stored at phy_param + 0x19e
 */
void phy_init_param_set(uint8_t flag) {
	phy_param[_PAR_INIT] = (uint8_t)(flag & 1);
}

/**
 * @returns 440
 */
uint32_t phy_get_rfdata_num(void) {
	return(0x1b8);
}

/**
 * Store the 23 dBm enable byte.
 * @param enable Byte stored at phy_param + 0x4e
 */
void phy_set_23dbm_en(uint8_t enable) {
	phy_param[_PAR_DBM] = enable;
}

/**
 * Force three power bytes to 92 when 23 dBm is enabled.
 * @param image Image whose bytes 2 and 3 are updated
 */
void phy_set_23dbm(uint8_t *image) {
	if(phy_param[_PAR_DBM] != 0) {
		image[2] = _DBM_BYTE;
		image[3] = _DBM_BYTE;
		phy_param[_PAR_TPW] = _DBM_BYTE;
	}
}

/**
 * Store the temperature-track debug bytes.
 * @param mask Byte stored at phy_param + 0x1a5
 * @param limit Byte stored at phy_param + 0x1a6
 */
void phy_track_temp_debug(uint8_t mask, uint8_t limit) {
	phy_param[_PAR_TRACK] = mask;
	phy_param[_PAR_LIMIT] = limit;
}

/**
 * Store the 0.1 kHz step enable.
 * @param enable Byte stored at phy_param + 0x1a7
 */
void phy_set_step_01k(uint8_t enable) {
	phy_param[_PAR_SCALE] = enable;
}

/** Wait until a hardware frequency hop finishes. */
void phy_wait_freq_hw_hop_done(void) {
	phy_wait_freq_set_busy();
}

/**@}*/
