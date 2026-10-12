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

#ifndef _HAVE_STM32_GATE_H
	#define _HAVE_STM32_GATE_H

	/**
	 * A gate is one peripheral block (USART1, SPI1, TIM2...).
	 * The table lives in gate.c, one line per block bonded on the MCU.
	 */
	typedef struct mp_gate_s mp_gate_t;

	struct mp_gate_s {
		char *portDevice;
		mp_bool_t isBusy;
		char *byWho;

		/** register block */
		void *base;

		/** NVIC lines, irq2 is the error line of I2C (-1 when none) */
		int irq;
		int irq2;

		/** RCC enable register and bit */
		volatile uint32_t *rccEnable;
		uint32_t rccBit;

		/** 1 for APB1, 2 for APB2: picks the bus clock */
		unsigned char apb;

		/** default pins, "pa9" style, NULL when the block has none */
		const char *pin0;
		const char *pin1;
		const char *pin2;
	};

	void mp_gate_init(mp_kernel_t *kernel);
	void mp_gate_fini(mp_kernel_t *kernel);
	mp_gate_t *mp_gate_handle(char *id, char *who);
	void mp_gate_release(mp_gate_t *gate);

	/* clock of the bus the gate sits on, in Hz */
	unsigned long mp_gate_pclk(mp_gate_t *gate);

	static inline void mp_gate_clock_on(mp_gate_t *gate) {
		*gate->rccEnable |= gate->rccBit;
		(void)*gate->rccEnable;
	}

	static inline void mp_gate_clock_off(mp_gate_t *gate) {
		*gate->rccEnable &= ~gate->rccBit;
	}

#endif
