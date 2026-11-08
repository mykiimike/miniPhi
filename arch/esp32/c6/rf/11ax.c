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

802.11ax target power: the rate index and the two published bytes.

@{
*/

#define _REG_BRX 0x600A8060UL

#define _PAR_SPLIT  0x50
#define _PAR_CHAN   0x11c
#define _PAR_SELECT 0x11e

#define _PWR_CAP 84
#define _PWR_BIAS 86

/**
 * Map a rate code onto an index in the 18-byte target table.
 * @param rate PHY rate
 * @returns Table index
 */
uint32_t rate_to_index(uint32_t rate) {
	uint32_t changed;

	changed = wifi_11g_rate_chg_new((int)rate);
	if(rate <= 7)
		return((rate >> 1) & 1);
	if((mp_arch_rf_read(_REG_BRX) & 0x400UL) != 0)
		return(0);
	if(rate <= 15)
		return((uint8_t)((changed >> 1) + 2));
	if(rate <= 23)
		return((uint32_t)((((int32_t)rate) >> 1) & 7) + 6);
	return((uint8_t)(rate - 14));
}

/**
 * Two target-power bytes for one rate. Each byte above 84 is stored as 84.
 * The comparison is signed, so a byte above 127 stays below the cap.
 * @param rate PHY rate
 * @param out Two signed bytes
 */
void get_target_pwr(uint32_t rate, int8_t *out) {
	int8_t target[18];
	uint32_t index;
	int8_t first;
	int8_t second;
	int pair;

	wifi_get_target_power_new(mp_arch_rf_u16(_PAR_CHAN), (int)(uintptr_t)target);
	index = rate_to_index(rate);
	first = target[index];
	second = first;
	pair = ((index - 6) & 0xff) < 4;
	if(pair)
		second = target[index + 8];
	out[0] = first;
	out[1] = second;
	if(!pair && first <= _PWR_CAP)
		return;
	if(first > _PWR_CAP)
		out[0] = (int8_t)_PWR_CAP;
	if(second > _PWR_CAP)
		out[1] = (int8_t)_PWR_CAP;
}

/**
 * The two target bytes, each divided by 4.
 * @param rate PHY rate
 * @param out Two signed quarters
 * @returns 0
 */
uint32_t phy_get_max_pwr(uint32_t rate, int8_t *out) {
	int8_t sample[2];
	int8_t quarter;

	get_target_pwr(rate, sample);
	quarter = (int8_t)(sample[0] >> 2);
	out[0] = quarter;
	if(phy_param[_PAR_SPLIT] > 9)
		quarter = (int8_t)(sample[1] >> 2);
	out[1] = quarter;
	return(0);
}

/**
 * Power index from one target byte.
 * @param rate PHY rate
 * @returns Arithmetic (86 - byte) / 4
 */
int phy_get_pwr_index(uint32_t rate) {
	int8_t sample[2];
	int8_t selected;

	get_target_pwr(rate, sample);
	selected = sample[0];
	if(phy_param[_PAR_SELECT] != 0)
		selected = sample[1];
	return((_PWR_BIAS - (int)selected) >> 2);
}

/**@}*/
