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

#ifndef _HAVE_STM32_FSMC_H
	#define _HAVE_STM32_FSMC_H

	/**
	 * 8080 parallel bus on FSMC bank 1, 16 bits, for LCD controllers.
	 * The chip select picks the sub-bank, and RS is one address line:
	 * a write at cmd is RS low, at data RS high.
	 */

	#define MP_FSMC_PINS 20

	typedef struct mp_fsmc_s mp_fsmc_t;

	struct mp_fsmc_s {
		mp_gate_t *gate;
		mp_gpio_port_t *pins[MP_FSMC_PINS];
		int bank;
		volatile uint16_t *cmd;
		volatile uint16_t *data;

		/* cycles in ns, kept to retime the bank */
		unsigned long readNs;
		unsigned long writeNs;
		mp_list_item_t item;
		mp_bool_t listed;
	};

	/**
	 * options: gate ("FSMC"), cs (pd7, pg9, pg10 or pg12 for NE1 to NE4),
	 * rs (the pin of an address line, pf0 for A0, pe3 for A19...),
	 * writeCycle and readCycle in ns (100 and 500 when missing).
	 */
	mp_ret_t mp_fsmc_open(mp_kernel_t *kernel, mp_fsmc_t *fsmc, mp_options_t *options, char *who);
	mp_ret_t mp_fsmc_close(mp_fsmc_t *fsmc);
	void mp_fsmc_retime(unsigned long hclk);

	/**
	 * ES0340 2.3.9: on the F103xC/D/E a running FSMC holds PB7 (I2C1 SDA)
	 * high. Stop its clock around a transfer on those pins; no bank may be
	 * touched meanwhile. Returns YES when it was running, for mp_fsmc_resume.
	 */
	mp_bool_t mp_fsmc_pause(void);
	void mp_fsmc_resume(mp_bool_t running);

	static inline void mp_fsmc_write_cmd(mp_fsmc_t *fsmc, uint16_t value) {
		*fsmc->cmd = value;
	}

	static inline void mp_fsmc_write_data(mp_fsmc_t *fsmc, uint16_t value) {
		*fsmc->data = value;
	}

	static inline uint16_t mp_fsmc_read_data(mp_fsmc_t *fsmc) {
		return(*fsmc->data);
	}

#endif
