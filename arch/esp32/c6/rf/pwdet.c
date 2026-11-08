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

Power detector bring-up, the detector tone, and the SAR2 voltage read.

@{
*/

#define _REG_SARADC    0x6000E000UL
#define _REG_TX_TONE   0x600A0420UL
#define _REG_PWDET_A   0x600A0810UL
#define _REG_PWDET_B   0x600A0814UL
#define _REG_PWDET_C   0x600A0818UL
#define _REG_PWDET_D   0x600A081CUL
#define _REG_PWDET_E   0x600A0820UL
#define _REG_SAR_DOUT  0x600A0824UL
#define _REG_PMU_PWDET 0x600B1054UL

#define _PAR_TONE_STOP 0x1a1

#define _SARADC_ON  0x20000000UL
#define _TONE_BIT   0x40000UL
#define _FIELD_MASK 0xffffcfffUL

/**
 * Read the SAR2 code for one detector source.
 * @param sel Source field, bits 1:0
 * @returns The code from read_sar2_code
 */
uint32_t get_sar2_vol_new(uint32_t sel) {
	unsigned long saved;
	unsigned long word;
	uint32_t code;

	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_PERIF, 0, 7, 6, 6, 1);
	mp_arch_rf_set(_REG_SARADC, _SARADC_ON);
	saved = mp_arch_rf_read(_REG_PWDET_B);
	word = mp_arch_rf_read(_REG_PWDET_B);
	mp_arch_rf_write(_REG_PWDET_B, (word & _FIELD_MASK) | (((unsigned long)sel & 3UL) << 12));
	code = read_sar2_code();
	word = mp_arch_rf_read(_REG_PWDET_B);
	mp_arch_rf_write(_REG_PWDET_B, (saved & 0x3000UL) | (word & _FIELD_MASK));
	mp_arch_rf_clr(_REG_SARADC, _SARADC_ON);
	mp_arch_rf_reg_write_mask(MP_ARCH_RF_HOST_PERIF, 0, 7, 6, 6, 0);
	return(code);
}

/**
 * Program the power-detector timing registers and turn the SARADC on.
 */
void pwdet_reg_init_new(void) {
	unsigned long word;

	mp_arch_rf_write(_REG_PWDET_C, 0x0f0f0fffUL);
	mp_arch_rf_write(_REG_PWDET_D, 0x00ff0f64UL);
	word = mp_arch_rf_read(_REG_PWDET_A);
	mp_arch_rf_write(_REG_PWDET_A, (word & 0xfffff00fUL) | 0x500UL);
	mp_arch_rf_write(_REG_PWDET_E, 0xaaaaUL);
	word = mp_arch_rf_read(_REG_PWDET_A);
	mp_arch_rf_write(_REG_PWDET_A, (word & 0xff8fffffUL) | 0x200000UL);
	word = mp_arch_rf_read(_REG_PWDET_B);
	mp_arch_rf_write(_REG_PWDET_B, (word & 0xffffff00UL) | 8UL);
	mp_arch_rf_set(_REG_SARADC, _SARADC_ON);
	word = mp_arch_rf_read(_REG_PMU_PWDET);
	mp_arch_rf_write(_REG_PMU_PWDET, (word & 0x1fffffffUL) | 0x80000000UL);
}

/**
 * Start the detector tone and wait until the status field is 7.
 */
void ram_pwdet_tone_start(void) {
	mp_arch_rf_set(_REG_SARADC, _SARADC_ON);
	mp_arch_rf_set(_REG_TX_TONE, _TONE_BIT);
	mp_arch_rf_delay_us(1);
	mp_arch_rf_clr(_REG_PWDET_A, 1);
	mp_arch_rf_set(_REG_PWDET_A, 1);
	mp_arch_rf_delay_us(2);
	while(((mp_arch_rf_read(_REG_PWDET_B) >> 14) & 7UL) != 7UL)
		;
	if(phy_param[_PAR_TONE_STOP] != 0)
		mp_arch_rf_clr(_REG_TX_TONE, _TONE_BIT);
}

/**
 * Wait until the detector status field is 7.
 */
void ram_pwdet_wait_idle(void) {
	while(((mp_arch_rf_read(_REG_PWDET_B) >> 14) & 7UL) != 7UL)
		;
}

/**
 * Copy four SAR sample pairs out of the detector result registers.
 * @param out Eight halfwords, two per result register
 */
void ram_read_sar_dout(uint16_t *out) {
	unsigned long addr;
	uint32_t word;

	addr = _REG_SAR_DOUT;
	while(addr != _REG_SAR_DOUT + 0x10UL) {
		word = (uint32_t)mp_arch_rf_read(addr);
		addr += 4UL;
		out[0] = (uint16_t)((word >> 1) & 0x1fff);
		out[1] = (uint16_t)((word >> 17) & 0x1fff);
		out += 2;
	}
}

/**@}*/
