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

#ifndef _HAVE_ESP32_C6_RF_PHY_H
	#define _HAVE_ESP32_C6_RF_PHY_H

	uint32_t get_sar2_vol_new(uint32_t sel);
	void pwdet_reg_init_new(void);
	void ram_pwdet_tone_start(void);
	void ram_pwdet_wait_idle(void);
	void ram_read_sar_dout(uint16_t *out);

	void chan14_mic_cfg(int enable);
	void chan14_mic_enable(int enable, int cap);
	void ram_bb_bss_cbw40(uint32_t mode);
	int ram_set_chan_cal_interp(int8_t *samples, int index);

	void rc_cal_new(uint8_t *limits, uint8_t *divisors_pair, int divisors_quad);

	uint8_t wifi_11g_rate_chg_new(int rate);
	void wifi_get_target_power_new(uint32_t channel, int table);
	uint32_t rate_to_index(uint32_t rate);
	void get_target_pwr(uint32_t rate, int8_t *out);
	uint32_t phy_get_max_pwr(uint32_t rate, int8_t *out);
	int phy_get_pwr_index(uint32_t rate);

	void get_temp_init_new(int copy_base, int copy_span);
	uint32_t phy_get_tsens_value(void);
	void phy_set_tsens_power(uint32_t on);
	uint8_t phy_set_tsens_range(short temp);
	int ram_temp_to_power(int now, int base, int mode);
	uint32_t ram_tsens_temp_read_new(void);
	uint8_t tsens_dac_cal_new(int temp, int index);
	uint32_t tsens_dac_to_index(int field);
	void tsens_read_init_new(int on, int index);
	void tsens_temp_read_new(void);

	void bt_track_pll_cap(void);
	void bt_track_tx_power(int log, int flag);
	void phy_cal_param_track(int log);
	void phy_param_track(uint32_t log, uint32_t flag);
	void phy_param_track_tot(int wifi, int bt);
	void phy_rfpll_cap_track_new(int log);
	void phy_txpwr_track_slow(uint8_t slow);
	void tx_i2c_track(void);
	void tx_pwctrl_background_(void);
	void txpwr_cal_track(int bt, int apply, int log);
	void txpwr_correct(int bt, int8_t *step, int16_t *span, int limit, int ref);
	void wifi_track_tx_power(int apply, int log);

	void phy_11p_set(int mode, int band);
	void phy_freq_mem_backup(void);
	uint32_t phy_ftm_comp(void);
	void phy_get_adc_rand(int enable);
	int phy_get_cca(void);
	uint32_t phy_get_cca_cnt(uint32_t *out);
	uint32_t phy_get_most_tpw(void);
	uint32_t phy_get_noise_floor(void);
	int phy_get_rssi(void);
	int phy_get_rx_freq(uint32_t rate, uint32_t packed);
	uint32_t phy_internal_delay(void);
	void phy_set_cca_cnt(uint32_t count, int start);
	void phy_set_most_tpw(uint8_t power);
	void phy_set_rate(void);

	void bt_get_tx_gain_new(int base, int start, int span, uint32_t codes, uint32_t seconds,
		uint32_t powers, int bytes, int seconds_out, int codes_out, int8_t origin,
		int8_t slope, int8_t log);
	void bt_get_tx_tab(uint32_t codes_out, uint32_t seconds_out, uint32_t bytes_out, uint32_t log);
	void bt_set_tx_gain(uint32_t log);
	void bt_tx_gain_init(void);
	void get_chan_target_power_new(uint32_t channel, int cap, int out, int limits,
		int fcc, uint32_t fcc_b, uint32_t fcc_a);
	uint32_t get_tx_gain_value_new(uint32_t index, uint32_t target, uint16_t *code,
		uint16_t *second, short *remain, int codes, int seconds, int powers,
		uint8_t limit, short adjust, short thresh);
	void set_tx_gain_mem(int bt, int count, int codes, uint16_t *bb, int bytes, int quad,
		uint16_t *mask);
	void wifi_get_tx_gain_new(uint32_t channel, uint32_t samples, int ref, int extra,
		uint32_t codes, uint32_t seconds, uint32_t powers, int bytes, int seconds_out,
		int codes_out, int8_t log);
	void wifi_get_tx_tab(uint32_t channel, uint32_t bytes_out, uint32_t seconds_out,
		uint32_t codes_out, uint32_t log);
	void wifi_set_tx_gain(uint32_t channel, uint32_t log);

	void phy_chan_set(uint32_t channel, int mode);
	void phy_chan_set_misc(uint32_t channel);
	void phy_chan_set_offset(short offset);
	void phy_force_freq_set(int hw, uint32_t freq, short offset);
	int phy_rfpll_cap_correct_new(int log);
	int phy_rfpll_cap_init_cal_new(void);
	void phy_rfpll_set_freq_offset(int freq, int mode, int offset, uint8_t *out);
	void phy_set_freq(uint32_t freq, uint32_t offset);
	void ram_rfpll_set_freq(int delta, int mode, uint32_t freq, uint8_t *out);
	int rfpll_cap_correct_new(int log, uint32_t unused_a, uint32_t unused_b,
		uint32_t unused_c, uint32_t unused_d);
	void rfpll_chgp_cal(void);
	uint32_t set_chanfreq(uint32_t mhz, uint32_t mode);

	void RFChannelSel(uint32_t channel, int mode);
	uint32_t phy_change_channel(uint32_t mhz, uint32_t unused_a, uint32_t unused_b, uint32_t mode);
	void phy_rx_rifs_en(void);
	void phy_current_level_set(mp_arch_rf_code_t level);
	void phy_bt_power_track(uint8_t enable);
	void phy_xpd_tsens(void);
	void bb_wdt_rst_enable(int enable);
	void bb_wdt_int_enable(uint32_t enable);
	void bb_wdt_timeout_clear(void);
	uint32_t bb_wdt_get_status(void);
	uint32_t phy_get_rf_cal_version(void);
	void noise_check_loop(void);
	void phy_wakeup_init(void);
	void phy_close_rf(void);
	void tx_pwctrl_background(void);
	void esp_tx_state_out(void);
	void ant_dft_cfg(void);
	void ant_wifitx_cfg(void);
	void ant_wifirx_cfg(void);
	void ant_bttx_cfg(void);
	void ant_btrx_cfg(void);
	void ant_tx_cfg(uint32_t sel);
	void ant_rx_cfg(uint32_t a, uint32_t b, uint32_t c);
	void phy_chan_dump_cfg(uint32_t channel);
	uint32_t phy_dig_reg_backup(int save, uint32_t *buf);
	uint32_t phy_dig_reg_backup_new(int save, uint32_t *buf);
	void phy_chan_filt_set(void);
	void set_cca(void);
	void set_rx_sense(void);
	void rx_gain_force(void);
	void phy_ble_set_chan_base(uint8_t base);
	void phy_init_param_set(uint8_t flag);
	uint32_t phy_get_rfdata_num(void);
	void phy_set_23dbm_en(uint8_t enable);
	void phy_set_23dbm(uint8_t *image);
	void phy_track_temp_debug(uint8_t mask, uint8_t limit);
	void phy_set_step_01k(uint8_t enable);
	void phy_wait_freq_hw_hop_done(void);

	void rx_11b_opt(int enable);
	void tx_paon_set_new(void);
	void set_rx_comp_new(void);
	void agc_reg_init_new(uint32_t span, int extra);
	void btbb_wifi_bb_cfg2(void);
	void bb_reg_init_new(void);
	void open_i2c_xpd_new(int cold);
	void force_txon(int on);
	void force_txon_mode(uint32_t on, uint32_t apply, uint32_t index);
	void force_txrx_off(int rx);
	void mac_tx_chan_offset_new(int mode);
	void noise_floor_auto_set_new(void);
	void bt_filter_reg_new(void);
	void phy_bbpll_recal(void);
	void phy_disable_cca_new(void);
	void phy_enable_cca_new(void);
	void phy_fft_scale_force(uint32_t bit, int scale);
	void phy_force_rx_gain(uint32_t bit, int gain);
	void phy_wifi_enable_set(int enable);
	void phy_tx_tone(int on, int bt, uint32_t level);
	void ram_start_tx_tone_step(int on, uint32_t freq, int phase, int on2,
		uint32_t freq2, int phase2);
	void ram_stop_tx_tone(void);
	void rfrx_sat_rst(int wide);
	void write_gain_mem_new(uint32_t a, uint32_t b, uint32_t c, int slot);

	void pbus_clear_reg(void);
	void ram_pbus_force_mode(int on);
	uint32_t ram_pbus_rd_addr(uint32_t block, int half);
	uint32_t ram_pbus_rd_shift(uint32_t block, int half);
	void ram_pbus_xpd_rx_on(uint32_t gain, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
	void ram_pbus_xpd_tx_on(short gain, uint32_t second, uint32_t c, uint32_t d);
	void ram_set_loopback_gain(short gain, uint32_t first, uint32_t second, uint32_t d, uint32_t e);
	void ram_txcal_debuge_mode(void);
	void rx_pbus_reset(int wide);
	void save_pbus_reg(void);
	void set_pbus_mem(void);
	uint32_t gen_rx_gain_table_new(uint32_t *out, uint32_t limit, const uint16_t *codes,
		const int8_t *steps, const uint8_t *curve, int count, int verbose);
	uint32_t get_rxbb_dc(const uint16_t *samples, uint32_t slot, int scale);
	uint32_t phy_get_interp_data(const uint16_t *y, const uint16_t *x, uint32_t at);
	void phy_rx_table_init(void);
	void rxiq_cal_init(int tone, uint32_t unused, int keep);
	void rx_table_track(int verbose);
	void set_rx_gain_table(uint32_t tag, uint32_t unused);
	void wr_rx_gain_mem(int bank, uint32_t count, const uint32_t *rows,
		const uint16_t *dc, const uint16_t *pair, const uint16_t *alt,
		int span, int synth);
	uint32_t rfrx_gain_index_new(int bank, uint32_t code);

	void bt_txpwr_freq(int table);
	void freq_get_i2c_data(uint8_t *host, uint8_t *block, int8_t *shift,
		int words, uint32_t count);
	void freq_i2c_data_write_new(void);
	void freq_i2c_mem_write_new(uint32_t addr, uint32_t data, int bytes);
	void freq_i2c_write_set_new(const uint8_t *host, const uint8_t *block,
		const uint8_t *shift, const uint32_t *word, int count);
	void freq_reg_init(uint32_t sel, int mode);
	void get_rf_freq_init_new(uint32_t count, uint32_t mode);
	void phy_dis_hw_set_freq_1(void);
	void phy_dis_hw_set_freq_new(void);
	void phy_dis_hw_set_freq_reg(void);
	void phy_en_hw_set_freq_new(void);
	uint32_t phy_wait_freq_set_busy(void);
	uint32_t phy_wait_freq_set_busy_new(void);
	void pll_cap_mem_update_new(int delta);
	void pll_dac_mem_update_new(uint32_t value);
	void ram_set_chan_freq_sw_start(void);
	uint32_t read_rf_freq_mem_new(uint32_t addr, uint32_t bytes);
	void set_chan_freq_hw_init(uint32_t sel, int mode);
	void wr_rf_freq_mem_new(uint32_t index, const uint32_t *word);

	void debug_print_line(uint32_t text, uint32_t value);
	uint32_t get_bias_ref_code(void);
	void get_dc_value(uint16_t *out, uint32_t packed);
	void get_iq_value(uint8_t *out, uint32_t sample);
	void dc_iq_est_high(uint32_t enable, int count, int *out);
	void dc_iq_est_new(uint32_t enable, int count, int *out, int scale);
	uint32_t get_rfcal_rxiq_data_new(uint32_t tone, uint32_t gain, int verbose);
	void pbus_rx_dco_cal_1step_new(int kind, int stage, uint32_t samples,
		uint16_t *row, int8_t *flag);
	void pbus_rx_dco_cal_new(uint32_t samples, int16_t *row, uint32_t delay,
		int wide, int verbose, uint32_t passes);
	void phy_check_rx_sat(void);
	int64_t phy_corr_get_pwr(void);
	void phy_force_rx_gain_trig(void);
	uint32_t ram_bb_gain_index(uint32_t code);
	void ram_iq_est_enable(uint32_t unused, uint32_t length);
	void rfcal_rxiq_new(uint32_t shift, uint32_t tone, uint32_t gain,
		uint8_t *out, uint32_t verbose);
	void rx_chan_dc_sort(int16_t *pairs, uint8_t *flags);
	void rxdc_est_min_new(uint32_t samples, uint32_t unused, int *out, uint32_t scale);
	void rxiq_cover_mg_mp_new(uint32_t shift, int8_t *i_out, int8_t *q_out,
		uint32_t verbose);
	void rxiq_get_mis_new(uint32_t shift, uint8_t *out, int verbose);
	void set_lb_txiq_new(uint32_t packed);
	void set_rx_gain_cal_dc_new(int kind, int wide, uint16_t *dc, uint16_t *bb,
		uint16_t *wide_out);
	void set_rx_gain_cal_iq_new(int wide, uint32_t tone, uint16_t *out, int verbose);
	char *get_phy_version_str(void);
	void pbus_print(void);
	void phy_cal_print(void);
	uint32_t phy_get_vdd33(void);
	void phy_i2c_check(void);
	void phy_reg_check(void);
	void phy_tx_gain_print(void);
	void phy_version_print(void);
	void rfpll_cap_check(int verbose);

	void bb_init(void);
	void esp_phy_efuse_get_mac(uint32_t *out);
	void phy_close_rf_(void);
	void phy_get_mac_addr(uint8_t *out);
	void phy_get_romfunc_addr(void);
	void phy_get_xtal_freq(void);
	void phy_reg_init(void);
	uint32_t phy_rfcal_data_check(int mode, uint32_t image, uint32_t unused, uint32_t version);
	short phy_rfcal_data_check_value(uint32_t image, uint16_t *out, int compare);
	void phy_rfcal_data_sub(uint32_t image, int outward);
	void phy_set_mac_data(uint8_t *image, uint32_t version);
	void phy_wakeup_init_(void);
	void phy_xpd_rf(void);
	void rc_cal_init(void);
	uint32_t phy_register(uint8_t *init, uint32_t image, int mode);
	void phy_register_param(uint8_t *init);
	void reg_set_new(void);
	uint32_t rf_cal_data_backup(uint32_t image);
	void rf_cal_data_recovery(uint32_t image);
	void rf_init(void);
	void wait_i2c_sdm_stable(void);
	void txdc_cal_init_new(uint16_t *row, uint32_t span, uint32_t unused, int flag);
	void bt_txdc_cal_new(void);
	void bt_tx_pwctrl_init_new(void);
	void rfcal_pwrctrl_new(uint32_t tone, uint8_t *table, int count, int base,
		uint8_t *dest, uint32_t power, int target, int verbose);
	void rfcal_txcap_loop(int verbose, uint8_t *out);
	void rfcal_txiq_new(uint32_t tone, uint32_t dco, uint16_t *iq, int atten,
		uint32_t power, int mode);
	void tx_cap_init_loop(void);
	void txdc_cal_new(uint16_t *row, uint32_t unused_1, uint32_t unused_2,
		uint32_t unused_3, uint32_t unused_4);
	void txdc_cal_pwdet(uint16_t *row, int verbose);
	void txdc_cal_pwdet_init(uint32_t unused, int keep);
	void txiq_cal_init_new(void);
	void txiq_cover_new(int code, uint32_t power, uint8_t *out, int dump, int verbose);
	void tx_pwctrl_init_cal_new(int kind, int8_t *atten, int8_t *power, uint8_t *dest);
	void tx_pwctrl_init_new(int unused);

	void bias_reg_reset(void);
	void bias_reg_set(int enable);
	void filter_dcap_set(void);
	void i2c_bbpll_set(int enable);
	void i2c_clk_sel(uint32_t clock);
	void i2c_rc_cal_set(uint32_t field_a, uint32_t field_b);
	void phy_i2c_init1(void);
	void phy_i2c_init2(void);
	void phy_i2c_master_cmd_mem_init(void);
	void phy_i2c_master_mem_txcap(void);
	void set_freq_i2c(uint32_t freq);

	void write_pbus_mem(const uint32_t *a, const uint32_t *b, const uint32_t *c,
		const uint32_t *d, const uint32_t *e, const uint32_t *f,
		const uint32_t *g, const uint32_t *h, const uint32_t *i,
		const uint32_t *j, const uint32_t *k, const uint32_t *l);

#endif
