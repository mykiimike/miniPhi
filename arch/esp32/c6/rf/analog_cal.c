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

RC calibration: one raw measurement and six saturated coefficients.

@{
*/

#define _PAR_STATUS 0xa4
#define _PAR_RC_RAW 0xe8
#define _PAR_RC_A   0xe9
#define _PAR_RC_B   0xed

#define _RC_DONE 0x00800000UL

/**
 * Signed divide. A zero denominator yields all-ones, the same result as the divide instruction.
 * @param num Numerator
 * @param den Denominator
 * @returns The quotient
 */
static int _div(int num, int den) {
	if(den == 0)
		return(-1);
	return(num / den);
}

/**
 * Sign-extend the low half of (quotient - 8).
 * @param quot Quotient
 * @returns The adjusted value
 */
static int _adjust(int quot) {
	return((int)(int16_t)(quot - 8));
}

/**
 * Fill the RC coefficient bytes and mark calibration done.
 * @param limits Four saturation limits
 * @param divisors_pair Two divisors for the first pair
 * @param divisors_quad Address of four divisors for the second group
 */
void rc_cal_new(uint8_t *limits, uint8_t *divisors_pair, int divisors_quad) {
	const uint8_t *quad;
	uint32_t raw;
	int measure;
	int base;
	int pair_num;
	int quad_num;
	int quot;
	unsigned i;

	if((mp_arch_rf_u32(_PAR_STATUS) & _RC_DONE) != 0)
		return;
	quad = (const uint8_t *)(uintptr_t)(uint32_t)divisors_quad;
	raw = get_rc_dout();
	phy_param[_PAR_RC_RAW] = (uint8_t)raw;
	measure = (int)(raw & 0xff);
	if(raw > 40)
		measure = 40;
	base = measure + 56;
	pair_num = base * 82;
	for(i = 0; i < 2; i++) {
		quot = _div(pair_num, (int)divisors_pair[i] * 10);
		phy_param[_PAR_RC_A + i] = (uint8_t)get_data_sat(_adjust(quot), limits[i], 2);
	}
	quad_num = base * 820;
	for(i = 0; i < 4; i++) {
		quot = _div(quad_num, (int)quad[i] * 104);
		phy_param[_PAR_RC_B + i] = (uint8_t)get_data_sat(_adjust(quot), limits[(i & 1) + 2], 0);
	}
	mp_arch_rf_set_u32(_PAR_STATUS, mp_arch_rf_u32(_PAR_STATUS) | _RC_DONE);
}

/**@}*/
