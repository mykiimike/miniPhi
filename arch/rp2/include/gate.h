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

#ifndef _HAVE_RP2350_GATE_H
	#define _HAVE_RP2350_GATE_H

	/**
	 * A gate is one peripheral block (UART0, SPI0, I2C0).
	 * rccEnable points at RESETS_RESET: the bit is set while the block is held
	 * in reset, so clock-on clears it and waits for RESET_DONE.
	 */
	typedef struct mp_gate_s mp_gate_t;

	struct mp_gate_s {
		char *portDevice;
		mp_bool_t isBusy;
		char *byWho;

		void *base;

		int irq;
		int irq2;

		volatile uint32_t *rccEnable;
		uint32_t rccBit;

		/** 0: clk_peri. Every UART, SPI and I2C on the RP2350 sits there. */
		unsigned char apb;

		const char *pin0;
		const char *pin1;
		const char *pin2;
	};

	void mp_gate_init(mp_kernel_t *kernel);
	void mp_gate_fini(mp_kernel_t *kernel);
	mp_gate_t *mp_gate_handle(char *id, char *who);
	void mp_gate_release(mp_gate_t *gate);

	unsigned long mp_gate_pclk(mp_gate_t *gate);

	static inline void mp_gate_clock_on(mp_gate_t *gate) {
		*gate->rccEnable &= ~gate->rccBit;
		while((RESETS_RESET_DONE & gate->rccBit) != gate->rccBit);
	}

	static inline void mp_gate_clock_off(mp_gate_t *gate) {
		*gate->rccEnable |= gate->rccBit;
	}

#endif
