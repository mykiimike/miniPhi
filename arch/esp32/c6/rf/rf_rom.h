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

#ifndef _HAVE_ESP32_RF_ROM_H
#define _HAVE_ESP32_RF_ROM_H

#include "../../rf/host.h"

/* Official C6 PHY ROM jump table, esp32c6.rom.phy.ld */
#define abs_temp ((mp_arch_rf_rom_t)0x40001130UL)
#define agc_reg_init ((mp_arch_rf_rom_t)0x4000137CUL)
#define ant_btrx_cfg_rom ((mp_arch_rf_rom_t)0x400011F0UL)
#define ant_bttx_cfg_rom ((mp_arch_rf_rom_t)0x400011ECUL)
#define ant_dft_cfg_rom ((mp_arch_rf_rom_t)0x400011E0UL)
#define ant_wifirx_cfg_rom ((mp_arch_rf_rom_t)0x400011E8UL)
#define ant_wifitx_cfg_rom ((mp_arch_rf_rom_t)0x400011E4UL)
#define bb_bss_cbw40_dig ((mp_arch_rf_rom_t)0x4000134CUL)
#define bb_reg_init ((mp_arch_rf_rom_t)0x40001380UL)
#define bb_wdg_cfg ((mp_arch_rf_rom_t)0x40001370UL)
#define bt_bb_to_index ((mp_arch_rf_rom_t)0x40001468UL)
#define bt_chan_pwr_interp ((mp_arch_rf_rom_t)0x4000147CUL)
#define bt_gain_offset ((mp_arch_rf_rom_t)0x40001364UL)
#define cbw2040_cfg ((mp_arch_rf_rom_t)0x40001350UL)
#define chan_to_freq ((mp_arch_rf_rom_t)0x40001224UL)
#define phy_chan_ana ((mp_arch_rf_rom_t)0x40001250UL)
#define code_to_temp ((mp_arch_rf_rom_t)0x4000143CUL)
#define dc_iq_est ((mp_arch_rf_rom_t)0x400013F4UL)
#define disable_agc ((mp_arch_rf_rom_t)0x40001338UL)
#define enable_agc ((mp_arch_rf_rom_t)0x4000133CUL)
#define esp_tx_state_out_rom ((mp_arch_rf_rom_t)0x400011DCUL)
#define fe_reg_init ((mp_arch_rf_rom_t)0x40001368UL)
#define fe_txrx_reset ((mp_arch_rf_rom_t)0x40001374UL)
#define freq_chan_en_sw ((mp_arch_rf_rom_t)0x4000114CUL)
#define freq_i2c_num_addr ((mp_arch_rf_rom_t)0x40001170UL)
#define freq_module_resetn ((mp_arch_rf_rom_t)0x40001148UL)
#define get_data_sat ((mp_arch_rf_rom_t)0x40001134UL)
#define get_dco_comp ((mp_arch_rf_rom_t)0x4000140CUL)
#define get_freq_mem_addr ((mp_arch_rf_rom_t)0x40001158UL)
#define get_freq_mem_param ((mp_arch_rf_rom_t)0x40001154UL)
#define get_power_atten ((mp_arch_rf_rom_t)0x4000127CUL)
#define get_power_db ((mp_arch_rf_rom_t)0x400011B8UL)
#define get_rate_fcc_index ((mp_arch_rf_rom_t)0x40001480UL)
#define get_rc_dout ((mp_arch_rf_rom_t)0x40001114UL)
#define get_temp_init ((mp_arch_rf_rom_t)0x40001458UL)
#define get_tone_sar_dout ((mp_arch_rf_rom_t)0x400011A8UL)
#define i2c_master_reset ((mp_arch_rf_rom_t)0x40001140UL)
#define i2c_paral_write_num ((mp_arch_rf_rom_t)0x400012E4UL)
#define i2c_sar2_init_code ((mp_arch_rf_rom_t)0x400012ECUL)
#define i2cmst_reg_init ((mp_arch_rf_rom_t)0x40001360UL)
#define index_to_txbbgain ((mp_arch_rf_rom_t)0x40001460UL)
#define iq_corr_enable ((mp_arch_rf_rom_t)0x400013A4UL)
#define linear_to_db ((mp_arch_rf_rom_t)0x400011B4UL)
#define mac_enable_bb ((mp_arch_rf_rom_t)0x4000136CUL)
#define mhz2ieee ((mp_arch_rf_rom_t)0x40001220UL)
#define noise_floor_auto_set ((mp_arch_rf_rom_t)0x4000139CUL)
#define open_i2c_xpd ((mp_arch_rf_rom_t)0x40001384UL)
#define pbus_debugmode ((mp_arch_rf_rom_t)0x40001320UL)
#define pbus_rx_dco_cal ((mp_arch_rf_rom_t)0x40001410UL)
#define pbus_set_dco ((mp_arch_rf_rom_t)0x40001328UL)
#define pbus_workmode ((mp_arch_rf_rom_t)0x40001324UL)
#define phy_ant_init ((mp_arch_rf_rom_t)0x400013B0UL)
void phy_bbpll_en_usb(void);
#define phy_bbpll_cal ((mp_arch_rf_rom_t)0x400013ACUL)
#define phy_byte_to_word ((mp_arch_rf_rom_t)0x40001138UL)
#define phy_chan_dump_cfg_rom ((mp_arch_rf_rom_t)0x400011F4UL)
#define phy_chan_filt_set_rom ((mp_arch_rf_rom_t)0x40001208UL)
#define phy_dis_hw_set_freq ((mp_arch_rf_rom_t)0x4000118CUL)
#define phy_en_hw_set_freq ((mp_arch_rf_rom_t)0x40001188UL)
#define phy_force_pwr_index ((mp_arch_rf_rom_t)0x400013E0UL)
#define phy_freq_correct ((mp_arch_rf_rom_t)0x400013CCUL)
#define phy_get_romfuncs ((mp_arch_rf_rom_t)0x40001108UL)
#define phy_param_addr ((mp_arch_rf_rom_t)0x40001104UL)
#define phy_rx_sense_set ((mp_arch_rf_rom_t)0x400013C0UL)
#define pwdet_code_cal ((mp_arch_rf_rom_t)0x40001284UL)
#define pwdet_ref_code ((mp_arch_rf_rom_t)0x40001280UL)
#define read_hw_noisefloor ((mp_arch_rf_rom_t)0x400013A0UL)
#define read_pll_cap ((mp_arch_rf_rom_t)0x4000124CUL)
#define read_sar2_code ((mp_arch_rf_rom_t)0x400011C4UL)
#define restart_cal ((mp_arch_rf_rom_t)0x40001228UL)
#define rx_gain_force_rom ((mp_arch_rf_rom_t)0x40001218UL)
#define rxiq_set_reg ((mp_arch_rf_rom_t)0x4000138CUL)
#define set_cca_rom ((mp_arch_rf_rom_t)0x40001210UL)
#define set_chan_reg ((mp_arch_rf_rom_t)0x4000113CUL)
void set_channel_rfpll_freq(uint32_t param_1, uint32_t param_2, uint32_t param_3);
#define set_pbus_reg ((mp_arch_rf_rom_t)0x400013D0UL)
#define set_rf_freq_offset ((mp_arch_rf_rom_t)0x40001234UL)
#define set_rfpll_freq ((mp_arch_rf_rom_t)0x40001238UL)
#define set_rx_comp ((mp_arch_rf_rom_t)0x40001378UL)
#define set_rx_sense_rom ((mp_arch_rf_rom_t)0x40001214UL)
#define set_rxclk_en ((mp_arch_rf_rom_t)0x40001394UL)
#define set_txclk_en ((mp_arch_rf_rom_t)0x40001390UL)
#define tsens_code_read ((mp_arch_rf_rom_t)0x4000144CUL)
#define tsens_temp_read ((mp_arch_rf_rom_t)0x40001450UL)
#define tx_paon_set ((mp_arch_rf_rom_t)0x40001358UL)
#define tx_pwctrl_bg_init ((mp_arch_rf_rom_t)0x400011D0UL)
#define txbbgain_to_index ((mp_arch_rf_rom_t)0x4000145CUL)
#define txcal_work_mode ((mp_arch_rf_rom_t)0x4000132CUL)
#define txiq_get_mis_pwr ((mp_arch_rf_rom_t)0x40001270UL)
#define txiq_set_reg ((mp_arch_rf_rom_t)0x40001388UL)
void wait_rfpll_cal_end(void);
#define wifi_11g_rate_chg ((mp_arch_rf_rom_t)0x40001478UL)
#define wifi_agc_sat_gain ((mp_arch_rf_rom_t)0x400013A8UL)
#define wifi_fbw_sel ((mp_arch_rf_rom_t)0x400013B8UL)
#define wifi_get_target_power ((mp_arch_rf_rom_t)0x4000148CUL)
#define wifi_get_tx_gain ((mp_arch_rf_rom_t)0x40001474UL)
#define wifi_rifs_mode_en ((mp_arch_rf_rom_t)0x400013D4UL)
#define write_chan_freq ((mp_arch_rf_rom_t)0x40001150UL)
#define write_pll_cap ((mp_arch_rf_rom_t)0x40001248UL)
#define write_rfpll_sdm ((mp_arch_rf_rom_t)0x4000122CUL)

#define CONCAT11(a, b) ((uint16_t)(((uint16_t)(uint8_t)(a) << 8) | (uint8_t)(b)))
#define CONCAT12(a, b) ((uint32_t)(((uint32_t)(uint8_t)(a) << 16) | (uint16_t)(b)))
#define CONCAT13(a, b) ((uint32_t)(((uint32_t)(uint8_t)(a) << 24) | (uint32_t)(b)))
#define SUB41(a, n) ((uint8_t)((uint32_t)(a) >> (8 * (n))))

extern mp_arch_rf_code_t UNRECOVERED_JUMPTABLE;

#endif
