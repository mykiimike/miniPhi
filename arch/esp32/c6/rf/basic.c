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

Channel-14 microphone gain, the 20/40 MHz switch, and channel interpolation.

@{
*/

#define _REG_MIC 0x600A7400UL
#define _REG_CBW 0x600A780CUL

#define _PAR_TPW     0x06
#define _PAR_MIC_CAP 0x24
#define _PAR_MIC_EN  0x26

/**
 * Apply the channel-14 microphone target power, or the normal one.
 * @param enable Non-zero selects the channel-14 byte
 */
void chan14_mic_cfg(int enable) {
	unsigned long word;
	int8_t power;

	if(enable == 1) {
		word = mp_arch_rf_read(_REG_MIC);
		mp_arch_rf_write(_REG_MIC, (word & 0xffff9fffUL) | 0x2000UL);
		power = (int8_t)phy_param[_PAR_MIC_CAP];
	} else {
		mp_arch_rf_set(_REG_MIC, 0x6000UL);
		power = (int8_t)phy_param[_PAR_TPW];
	}
	phy_set_most_tpw((int)power);
}

/**
 * Record the channel-14 microphone request.
 * @param enable Zero restores the normal target power
 * @param cap Requested cap, stored at most as 48
 */
void chan14_mic_enable(int enable, int cap) {
	phy_param[_PAR_MIC_EN] = (uint8_t)enable;
	if(enable == 0) {
		chan14_mic_cfg(0);
		return;
	}
	if(cap > 48)
		cap = 48;
	phy_param[_PAR_MIC_CAP] = (uint8_t)cap;
}

/**
 * Select the baseband channel width and the TX offset that goes with it.
 * @param mode 0 is 20 MHz, 1 is the upper 40 MHz, 3 sets the extra bit
 */
void ram_bb_bss_cbw40(uint32_t mode) {
	mac_tx_chan_offset_new((int)(mode & 0xff));
	if(mode == 0) {
		bb_bss_cbw40_dig(0);
		wifi_fbw_sel(0);
		return;
	}
	bb_bss_cbw40_dig(1);
	if(mode == 1) {
		cbw2040_cfg(1);
		wifi_fbw_sel(1);
	} else {
		cbw2040_cfg(0);
		wifi_fbw_sel(1);
		if(mode == 3) {
			mp_arch_rf_set(_REG_CBW, 1);
			return;
		}
	}
	mp_arch_rf_clr(_REG_CBW, 1);
}

/**
 * Interpolate one signed sample across the 1..13 channel span.
 * @param samples Three signed anchors
 * @param index Channel number
 * @returns The interpolated byte, sign-extended
 */
int ram_set_chan_cal_interp(int8_t *samples, int index) {
	unsigned span;
	int value;

	span = (unsigned)(index - 1) & 0xffff;
	if(span < 6) {
		value = (int)samples[0] + (((int)samples[1] - (int)samples[0]) * (int)span) / 5;
	} else if(span < 11) {
		value = (int)samples[1] + (((int)samples[2] - (int)samples[1]) * (int)(span - 5)) / 5;
	} else if(span == 11) {
		return((int)samples[2]);
	} else {
		value = (int)samples[2] + 2;
	}
	return((int)(int8_t)value);
}

/**@}*/
