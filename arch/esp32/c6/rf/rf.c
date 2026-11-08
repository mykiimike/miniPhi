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
#include "../../include/modem.h"
#include "rf_priv.h"

/**
@addtogroup mpArchESP32RF

The C6 calibration image and the clock hook. phy_get_xtal_freq()
records the crystal from mp_arch_rf_xtal_mhz(), which is C6_XTAL_HZ.
The radio functions live in the other files of this directory. Bodies that live only in
the chip ROM are called at the addresses published in esp32c6.rom.phy.ld.

@{
*/

mp_arch_rf_code_t UNRECOVERED_JUMPTABLE;
unsigned char *g_phyFuns;
char phy_version_str[0x28];

/* Image shipped with the C6 radio. */
uint8_t phy_param[MP_ARCH_RF_PARAM] = {
	[0x02] = 0xbf, [0x03] = 0x20,
	[0x06] = 0x54,
	[0x0a] = 0x01, [0x0b] = 0x01,
	[0x0e] = 0x6a, [0x0f] = 0x01,
	[0x12] = 0x16, [0x13] = 0x17,
	[0x14] = 0x01, [0x15] = 0x40, [0x16] = 0x02,
	[0x18] = 0x50,
	[0x24] = 0x30,
	[0x1a0] = 0x01,
};

const uint8_t phy_tsens_attribute[0x1e] = {
	0xfe, 0x05, 0x32, 0x00, 0x7d, 0x00, 0xff, 0x07,
	0x14, 0x00, 0x64, 0x00, 0x00, 0x0f, 0xf6, 0xff,
	0x50, 0x00, 0x01, 0x0b, 0xe2, 0xff, 0x32, 0x00,
	0x02, 0x0a, 0xd8, 0xff, 0x14, 0x00,
};

int mp_arch_rf_xtal_mhz(void) {
	return((int)(C6_XTAL_HZ / 1000000UL));
}

/* Analog I2C stays with the PHY. The modem domain saves and restores the rest. */
#define _PMU_I2C       (C6_PMU + 0x154UL)
#define _PMU_I2C_BITS  0xF0000000UL

void phy_get_romfunc_addr(void);

static unsigned long _saved_addr[12];
static unsigned long _saved_word[12];
static int _saved;

static void _keep(unsigned long addr) {
	_saved_addr[_saved] = addr;
	_saved_word[_saved] = mp_arch_rf_read(addr);
	_saved++;
}

/**
 * Bring the shared modem clocks up and remember every word this changes.
 * 0xD0 and 0xDC are pulses: they are not restored. Do not RMW 0x600B00CC.
 */
void mp_arch_modem_on(void) {
	_saved = 0;
	_keep(C6_PCR + 0x108UL);
	mp_arch_rf_set(C6_PCR + 0x108UL, 1UL);
	/* Modem ICG boots in SLEEP. Bits 31:30 become 2. */
	_keep(C6_PMU + 0x0CUL);
	mp_arch_rf_write(C6_PMU + 0x0CUL, (mp_arch_rf_read(C6_PMU + 0x0CUL) & ~(3UL << 30)) | (2UL << 30));
	mp_arch_rf_write(C6_PMU + 0xDCUL, 1UL << 31);
	mp_arch_rf_write(C6_PMU + 0xD0UL, 1UL << 28);
	_keep(C6_MODEM_SYSCON + 0x04UL);
	mp_arch_rf_write(C6_MODEM_SYSCON + 0x04UL, 0xFFFFFFFFUL);
	_keep(C6_MODEM_SYSCON + 0x14UL);
	mp_arch_rf_write(C6_MODEM_SYSCON + 0x14UL, 0x7FFFFUL);
	_keep(C6_MODEM_SYSCON + 0x0CUL);
	mp_arch_rf_write(C6_MODEM_SYSCON + 0x0CUL, 0xFFFFFFFFUL);
	_keep(C6_MODEM_LPCON + 0x18UL);
	mp_arch_rf_write(C6_MODEM_LPCON + 0x18UL, 0xFFFFFFFFUL);
	_keep(C6_MODEM_LPCON + 0x1CUL);
	mp_arch_rf_write(C6_MODEM_LPCON + 0x1CUL, 0xFFFFFFFFUL);
	_keep(C6_MODEM_LPCON + 0x20UL);
	mp_arch_rf_write(C6_MODEM_LPCON + 0x20UL, 0xFFFFFFFFUL);
	_keep(C6_MODEM_LPCON + 0x10UL);
	mp_arch_rf_set(C6_MODEM_LPCON + 0x10UL, 1UL);
	_keep(C6_HP_APM + 0xC4UL);
	mp_arch_rf_clr(C6_HP_APM + 0xC4UL, 0xFUL);
}

/** Write back the words mp_arch_modem_on changed, last first. */
void mp_arch_modem_off(void) {
	while(_saved > 0) {
		_saved--;
		mp_arch_rf_write(_saved_addr[_saved], _saved_word[_saved]);
	}
}

/**
 * @param on TRUE brings the modem clocks up and the analog I2C master out
 */
void mp_arch_rf_power(mp_bool_t on) {
	if(on == TRUE) {
		mp_arch_modem_get(MP_ARCH_MODEM_PHY);
		phy_get_romfunc_addr();
		/* PERIF_I2C_RSTB, XPD_PERIF_I2C, XPD_TXRF_I2C, XPD_RFRX_PBUS. Set these only. */
		mp_arch_rf_set(_PMU_I2C, _PMU_I2C_BITS);
		ets_delay_us(10);
		return;
	}
	mp_arch_rf_clr(_PMU_I2C, _PMU_I2C_BITS);
	mp_arch_modem_put(MP_ARCH_MODEM_PHY);
}

/**
 * @returns TRUE when bit 8 of 0x600A00CC is set
 */
mp_ret_t mp_arch_rf_ready(void) {
	return((mp_arch_rf_read(0x600A00CCUL) & 0x100UL) != 0 ? TRUE : FALSE);
}

void rfpll_cap_track(uint32_t arg) {
	phy_rfpll_cap_track_new(arg);
}

/**@}*/
