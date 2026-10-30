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

#ifndef _HAVE_ESP32_GATE_H
	#define _HAVE_ESP32_GATE_H

	typedef struct mp_gate_s mp_gate_t;

	struct mp_gate_s {
		char *portDevice;
		mp_bool_t isBusy;
		char *byWho;

		void *base;

		/** interrupt-matrix source, -1 when the block has none */
		int irq;

		/** default pins, "pa16" style, NULL when the block has none */
		const char *pin0;
		const char *pin1;
		const char *pin2;
	};

	void mp_gate_init(mp_kernel_t *kernel);
	void mp_gate_fini(mp_kernel_t *kernel);
	mp_gate_t *mp_gate_handle(char *id, char *who);
	void mp_gate_release(mp_gate_t *gate);

	/* AHB clock, 40 MHz on every CPU level this port uses */
	unsigned long mp_gate_pclk(mp_gate_t *gate);

	static inline void mp_gate_clock_on(mp_gate_t *gate) {
		(void)gate;
	}

	static inline void mp_gate_clock_off(mp_gate_t *gate) {
		(void)gate;
	}

#endif
