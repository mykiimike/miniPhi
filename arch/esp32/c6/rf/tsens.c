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

Temperature sensor: power, the DAC range, and the halfword stored at phy_param.

@{
*/

#define _REG_PCR_TSENS 0x60096088UL
#define _REG_TSENS_A   0x6000E058UL
#define _REG_TSENS_B   0x6000E05CUL

#define _PAR_TEMP  0x00
#define _PAR_BASE  0x04
#define _PAR_RANGE 0x16
#define _PAR_SPAN  0x48
#define _PAR_COPY  0x12e
#define _PAR_HOLD  0x130

/**
 * Signed halfword in the temperature attribute table.
 * @param off Byte offset
 * @returns The halfword
 */
static int16_t _attr_h(unsigned off) {
	int16_t v;

	memcpy(&v, phy_tsens_attribute + off, sizeof(v));
	return(v);
}

/**
 * Map a DAC field onto a row of the attribute table.
 * @param field Low nibble of the peripheral register
 * @returns Row 0..5
 */
uint32_t tsens_dac_to_index(int field) {
	if(field == 5)
		return(0);
	if(field == 7)
		return(1);
	if(field == 0xf)
		return(2);
	if(field == 0xb)
		return(3);
	if(field == 10)
		return(4);
	return(5);
}

/**
 * Pick a DAC field for this temperature and write it when the row does not fit.
 * @param temp Temperature
 * @param index Attribute row
 * @returns The field that applies
 */
uint8_t tsens_dac_cal_new(int temp, int index) {
	unsigned row;
	uint8_t field;

	row = (unsigned)index * 6;
	if(temp >= _attr_h(row + 2) && temp <= _attr_h(row + 4))
		return(phy_tsens_attribute[row + 1]);
	if(temp > 99)
		field = 5;
	else if(temp > 79)
		field = 7;
	else if(temp >= -9)
		field = 0xf;
	else if(temp < -29)
		field = 10;
	else
		field = 0xb;
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_PERIF, 0, 6, 3, 0, field);
	return(field);
}

/**
 * Apply the DAC field for the temperature and the current range byte.
 * @param temp Temperature, sign-extended from 16 bits
 * @returns The field written or the row's own field
 */
uint8_t phy_set_tsens_range(short temp) {
	return(tsens_dac_cal_new((int)temp, phy_param[_PAR_RANGE]));
}

/**
 * Read the sensor, record the range, and convert the code.
 * @returns The temperature from code_to_temp
 */
uint32_t ram_tsens_temp_read_new(void) {
	uint32_t field;
	uint32_t temp;
	unsigned row;

	field = mp_arch_rf_reg_read(MP_ARCH_RF_HOST_PERIF, 0, 6);
	phy_param[_PAR_RANGE] = (uint8_t)tsens_dac_to_index((int)(field & 0xf));
	temp = tsens_code_read();
	row = (unsigned)phy_param[_PAR_RANGE] * 6;
	temp = code_to_temp(temp, (int)(int8_t)phy_tsens_attribute[row]);
	tsens_dac_cal_new((int)temp, phy_param[_PAR_RANGE]);
	return(temp);
}

/**
 * Read the temperature. The value comes back in the return register.
 * @returns The temperature
 */
uint32_t phy_get_tsens_value(void) {
	return(ram_tsens_temp_read_new());
}

/**
 * Read the temperature and store it as a halfword at the start of phy_param.
 */
void tsens_temp_read_new(void) {
	mp_arch_rf_set_u16(_PAR_TEMP, (uint16_t)phy_get_tsens_value());
}

/**
 * Copy the stored temperature into the calibration halfwords.
 * @param copy_base Non-zero also copies the temperature to offset 0x130
 * @param copy_span Non-zero copies the temperature to offsets 0x12e and 0x48
 */
void get_temp_init_new(int copy_base, int copy_span) {
	uint16_t temp;

	tsens_temp_read_new();
	if(copy_span != 0) {
		temp = mp_arch_rf_u16(_PAR_TEMP);
		mp_arch_rf_set_u16(_PAR_COPY, temp);
		mp_arch_rf_set_u16(_PAR_SPAN, temp);
	}
	mp_arch_rf_set_u16(_PAR_BASE, mp_arch_rf_u16(_PAR_COPY));
	if(copy_base != 0)
		mp_arch_rf_set_u16(_PAR_HOLD, mp_arch_rf_u16(_PAR_TEMP));
}

/**
 * Turn the temperature sensor supply bit on or off.
 * @param on Bit 0 selects the on value
 */
void phy_set_tsens_power(uint32_t on) {
	unsigned long word;

	word = mp_arch_rf_read(_REG_TSENS_A);
	mp_arch_rf_write(_REG_TSENS_A, (word & 0xffbfffffUL) | (((unsigned long)on & 1UL) << 22));
}

/**
 * Enable the sensor clock and turn the supply on.
 * The two arguments are what rf_init passes; this function sets the supply itself.
 * @param on Ignored here
 * @param index Ignored here
 */
void tsens_read_init_new(int on, int index) {
	(void)on;
	(void)index;
	mp_arch_rf_set(_REG_PCR_TSENS, 0x400000UL);
	mp_arch_rf_clr(_REG_PCR_TSENS, 0x800000UL);
	mp_arch_rf_set(_REG_TSENS_B, 0x8000UL);
	phy_set_tsens_power(1);
}

/**
 * Scale a temperature delta into a signed power step.
 * @param now Current temperature
 * @param base Reference temperature
 * @param mode Zero divides by 4 or 6; any other value shifts or divides by 5
 * @returns The step, sign-extended from 8 bits
 */
int ram_temp_to_power(int now, int base, int mode) {
	int diff;
	int step;

	diff = (int)(int16_t)(now - base);
	if(mode != 0) {
		if(diff > 0)
			step = diff >> 2;
		else
			step = diff / 5;
	} else if(diff > 0) {
		step = diff / 6;
	} else {
		step = diff / 4;
	}
	return((int)(int8_t)step);
}

/**@}*/
