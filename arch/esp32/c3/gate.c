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

/**
@defgroup mpArchESP32C3Gate Peripheral gates

@ingroup mpArchESP32

USB is the Serial/JTAG block and has no pins. UART0 is the ROM default,
TX on GPIO21 and RX on GPIO20. SPI2 is the FSPI mux: clock GPIO6, MOSI
GPIO7, MISO GPIO2. I2C0 has no dedicated mux, so the default pair is
GPIO5 and GPIO4.

@{
*/

static mp_gate_t _gates[] = {
	{ "USB", NO, NULL, (void *)C3_USB, C3_SRC_USB, NULL, NULL, NULL },
	{ "UART0", NO, NULL, (void *)C3_UART0, C3_SRC_UART0, "pa21", "pa20", NULL },
	{ "SPI2", NO, NULL, (void *)C3_SPI2, C3_SRC_SPI2, "pa7", "pa2", "pa6" },
	{ "I2C0", NO, NULL, (void *)C3_I2C, C3_SRC_I2C, "pa5", "pa4", NULL },
	{ NULL }
};

void mp_gate_init(mp_kernel_t *kernel) {
	mp_gate_t *gate;

	(void)kernel;
	for(gate = _gates; gate->portDevice; gate++) {
		gate->isBusy = NO;
		gate->byWho = NULL;
	}
}

void mp_gate_fini(mp_kernel_t *kernel) {
	mp_gate_t *gate;

	(void)kernel;
	for(gate = _gates; gate->portDevice; gate++) {
		if(gate->isBusy == YES)
			mp_gate_release(gate);
	}
}

mp_gate_t *mp_gate_handle(char *id, char *who) {
	mp_gate_t *gate;

	if(!id)
		return(NULL);
	for(gate = _gates; gate->portDevice; gate++) {
		if(strcmp(gate->portDevice, id) != 0)
			continue;
		if(gate->isBusy == YES) {
			mp_printk("Gate %s is already used by %s", id, gate->byWho ? gate->byWho : "?");
			return(NULL);
		}
		gate->isBusy = YES;
		gate->byWho = who;
		return(gate);
	}
	mp_printk("Gate %s does not exist", id);
	return(NULL);
}

void mp_gate_release(mp_gate_t *gate) {
	if(!gate)
		return;
	gate->isBusy = NO;
	gate->byWho = NULL;
}

unsigned long mp_gate_pclk(mp_gate_t *gate) {
	(void)gate;
	return(C3_XTAL_HZ);
}

/**@}*/
