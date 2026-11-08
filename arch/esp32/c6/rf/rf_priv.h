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

#ifndef _HAVE_ESP32_RF_PRIV_H
	#define _HAVE_ESP32_RF_PRIV_H

	#include "rf_rom.h"
	#include "../../rf/phyfuns.h"
	#include "../../rf/param.h"
	#include "../../rf/i2c.h"
	#include "../../rf/rf_series.h"
	#include "phy.h"

	#define MP_ARCH_RF_SLOT_CHAN_INTERP   0x08
	#define MP_ARCH_RF_SLOT_CRIT_ENTER    0x00
	#define MP_ARCH_RF_SLOT_CRIT_EXIT     0x04
	#define MP_ARCH_RF_SLOT_CBW40         0x10
	#define MP_ARCH_RF_SLOT_PBUS          0x74
	#define MP_ARCH_RF_SLOT_PBUS_RD       0x78
	#define MP_ARCH_RF_SLOT_TX_TONE       0x98
	#define MP_ARCH_RF_SLOT_STOP_TONE     0x9c
	#define MP_ARCH_RF_SLOT_TXCAL_DBG     0x94
	#define MP_ARCH_RF_SLOT_LOOPBACK      0x90
	#define MP_ARCH_RF_SLOT_XPD_TX        0x8c
	#define MP_ARCH_RF_SLOT_XPD_RX        0x84
	#define MP_ARCH_RF_SLOT_RFPLL         0x30
	#define MP_ARCH_RF_SLOT_READREG       0x50
	#define MP_ARCH_RF_SLOT_WRITEREG      0x58
	#define MP_ARCH_RF_SLOT_READREG_MASK  0x5c
	#define MP_ARCH_RF_SLOT_WRITEREG_MASK 0x60
	#define MP_ARCH_RF_SLOT_READREG_ORG   0x48
	#define MP_ARCH_RF_SLOT_HOSTID        0x44
	#define MP_ARCH_RF_SLOT_READ_MASK     0x3c
	#define MP_ARCH_RF_SLOT_MST0_MASK     0x40
	#define MP_ARCH_RF_SLOT_ENTER         0x34
	#define MP_ARCH_RF_SLOT_EXIT          0x38

	/* Analog hosts, named from the role each one plays. */
	#define MP_ARCH_RF_HOST_ULP       0x61
	#define MP_ARCH_RF_HOST_RFPLL     0x62 /* charge pump and calibration end */
	#define MP_ARCH_RF_HOST_RFPLL_SDM 0x63
	#define MP_ARCH_RF_HOST_64        0x64
	#define MP_ARCH_RF_HOST_65        0x65
	#define MP_ARCH_RF_HOST_BBPLL     0x66
	#define MP_ARCH_RF_HOST_FILTER    0x67 /* baseband filter */
	#define MP_ARCH_RF_HOST_68        0x68
	#define MP_ARCH_RF_HOST_PERIF     0x69
	#define MP_ARCH_RF_HOST_BIAS      0x6a
	#define MP_ARCH_RF_HOST_SAR       0x6b /* RC calibration and transmit registers */
	#define MP_ARCH_RF_HOST_6C        0x6c
	#define MP_ARCH_RF_HOST_DIG       0x6d

	/**
	 * Read one analog register.
	 * @param host Analog host
	 * @param block Register block
	 * @param reg Register index
	 * @returns The register byte
	 */
	static inline uint32_t mp_arch_rf_reg_read(uint32_t host, uint32_t block, uint32_t reg) {
		return((uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_READREG)(host, block, reg));
	}

	/**
	 * Write one analog register.
	 * @param host Analog host
	 * @param block Register block
	 * @param reg Register index
	 * @param data Value to store
	 */
	static inline void mp_arch_rf_reg_write(uint32_t host, uint32_t block, uint32_t reg, uint32_t data) {
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_WRITEREG)(host, block, reg, data);
	}

	/**
	 * Read a bit field of an analog register.
	 * @param host Analog host
	 * @param block Register block
	 * @param reg Register index
	 * @param msb Highest bit of the field
	 * @param lsb Lowest bit of the field
	 * @returns The field
	 */
	static inline uint32_t mp_arch_rf_reg_read_mask(uint32_t host, uint32_t block, uint32_t reg, uint32_t msb, uint32_t lsb) {
		return((uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_READREG_MASK)(host, block, reg, msb, lsb));
	}

	/**
	 * Write a bit field of an analog register.
	 * @param host Analog host
	 * @param block Register block
	 * @param reg Register index
	 * @param msb Highest bit of the field
	 * @param lsb Lowest bit of the field
	 * @param data Field value
	 */
	static inline void mp_arch_rf_reg_write_mask(uint32_t host, uint32_t block, uint32_t reg, uint32_t msb, uint32_t lsb, uint32_t data) {
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_WRITEREG_MASK)(host, block, reg, msb, lsb, data);
	}

	/**
	 * Take the analog bus.
	 */
	static inline void mp_arch_rf_i2c_enter(void) {
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_ENTER)();
	}

	/**
	 * Release the analog bus.
	 */
	static inline void mp_arch_rf_i2c_exit(void) {
		MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_EXIT)();
	}

	/**
	 * Master-0 bit for one host.
	 * @param host Analog host
	 * @returns The mask bit, or 0 when the host has none
	 */
	static inline uint32_t mp_arch_rf_i2c_mst0_mask(int host) {
		return((uint32_t)MP_ARCH_RF_FN(MP_ARCH_RF_SLOT_MST0_MASK)(host));
	}

	extern char phy_version_str[0x28];
	extern const uint8_t phy_tsens_attribute[0x1e];

	void rfpll_cap_track();

#endif
