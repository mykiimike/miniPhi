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
#include "host.h"
#include "param.h"
#include "rf_series.h"

/**
@defgroup mpArchESP32RF Radio

@ingroup mpArchESP32

arch/esp32/rf/ answers the chip_v7 radio: the wait, the interrupt
mask, the open path and the analog I2C master. phy_printf() is
mp_printk(). The series supplies the calibration image, the crystal,
the power hook and the ready test.

The 480 MHz PLL stays as mp_clock_init() left it. GPIO3 and GPIO14
stay as the board set them.

Boot does not call mp_arch_rf_open().

@{
*/

void phy_bbpll_en_usb(void);
void phy_wifi_enable_set(int param_1);
uint32_t register_chipv7_phy(uint8_t *param_1, uint32_t param_2, int param_3);
uint32_t phy_change_channel(uint32_t param_1, uint32_t param_2, uint32_t param_3, uint32_t param_4);
void phy_close_rf(void);

static uint8_t _cal[MP_ARCH_RF_CAL];

/**
 * @param kernel Running kernel
 * @returns TRUE when the frequency block reports ready
 */
mp_ret_t mp_arch_rf_open(mp_kernel_t *kernel) {
	(void)kernel;
	memset(_cal, 0, sizeof(_cal));
	mp_arch_rf_power(TRUE);
	mp_arch_rf_set_u32(MP_ARCH_RF_STATUS, 0);
	mp_arch_rf_set_u8(MP_ARCH_RF_UP, 0);
	phy_bbpll_en_usb();
	register_chipv7_phy((uint8_t *)0, (uint32_t)(uintptr_t)_cal, MP_ARCH_RF_MODE);
	phy_wifi_enable_set(1);
	return(mp_arch_rf_ready());
}

/**
 * @param mhz Channel frequency in MHz
 * @returns TRUE when the synthesizer accepted the frequency
 */
mp_ret_t mp_arch_rf_channel(unsigned long mhz) {
	return(phy_change_channel(mhz, 0, 0, 0) == 0 ? TRUE : FALSE);
}

void mp_arch_rf_close(void) {
	if(mp_arch_rf_u8(MP_ARCH_RF_UP) == 0)
		return;
	phy_close_rf();
}

/**@}*/
