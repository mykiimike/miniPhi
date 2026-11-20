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
@defgroup mpArchESP32ClassicGate Peripheral gates

@ingroup mpArchESP32

UART0 is the console, GPIO1 and GPIO3. UART1 and UART2 keep their
IO_MUX pins. SPI2 is HSPI and SPI3 is VSPI. I2C is matrix-only; the
pins are the conventional ones. The clock bits live in
DPORT_PERIP_CLK_EN and the reset bits in DPORT_PERIP_RST_EN.

@{
*/

static mp_gate_t _gates[] = {
	{ "UART0", NO, NULL, (void *)CL_UART0, CL_SRC_UART0, "pa1", "pa3", NULL },
	{ "UART1", NO, NULL, (void *)CL_UART1, CL_SRC_UART1, "pa10", "pa9", NULL },
	{ "UART2", NO, NULL, (void *)CL_UART2, CL_SRC_UART2, "pa17", "pa16", NULL },
	{ "SPI2", NO, NULL, (void *)CL_SPI2, CL_SRC_SPI2, "pa13", "pa12", "pa14" },
	{ "SPI3", NO, NULL, (void *)CL_SPI3, CL_SRC_SPI3, "pa23", "pa19", "pa18" },
	{ "I2C0", NO, NULL, (void *)CL_I2C0, CL_SRC_I2C0, "pa21", "pa22", NULL },
	{ "I2C1", NO, NULL, (void *)CL_I2C1, CL_SRC_I2C1, "pa25", "pa26", NULL },
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
	return(cl_apb_hz());
}

/**@}*/
