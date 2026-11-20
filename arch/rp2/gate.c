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
@defgroup mpArchRP2350Gate Peripheral gates

@ingroup mpArchRP2350

rccEnable is RESETS_RESET. A set bit holds the block in reset, so
clock-on clears it. pin0/pin1/pin2 match the series gate defaults.
UART is TX, RX. SPI is MOSI, MISO, SCK. I2C is SDA, SCL.

@{
*/

static mp_gate_t __gates[] = {
	{ "USB", NO, NULL, (void *)RP2350_USBCTRL_REGS_BASE, USBCTRL_IRQ, -1, &RESETS_RESET, RESETS_USBCTRL, 0, NULL, NULL, NULL },
	{ "UART0", NO, NULL, UART0, UART0_IRQn, -1, &RESETS_RESET, RESETS_UART0, 0, "pa12", "pa13", NULL },
	{ "UART1", NO, NULL, UART1, UART1_IRQn, -1, &RESETS_RESET, RESETS_UART1, 0, "pa8", "pa9", NULL },
	{ "SPI0", NO, NULL, SPI0, SPI0_IRQn, -1, &RESETS_RESET, RESETS_SPI0, 0, "pa3", "pa4", "pa2" },
	{ "SPI1", NO, NULL, SPI1, SPI1_IRQn, -1, &RESETS_RESET, RESETS_SPI1, 0, "pa27", "pa24", "pa26" },
	{ "I2C0", NO, NULL, (void *)RP2350_I2C0_BASE, I2C0_IRQn, -1, &RESETS_RESET, RESETS_I2C0, 0, "pa0", "pa1", NULL },
	{ "I2C1", NO, NULL, (void *)RP2350_I2C1_BASE, I2C1_IRQn, -1, &RESETS_RESET, RESETS_I2C1, 0, "pa14", "pa15", NULL },
	{ NULL }
};

void mp_gate_init(mp_kernel_t *kernel) {
	mp_gate_t *gate;

	(void)kernel;
	for(gate = __gates; gate->portDevice; gate++) {
		gate->isBusy = NO;
		gate->byWho = NULL;
	}
}

void mp_gate_fini(mp_kernel_t *kernel) {
	mp_gate_t *gate;

	(void)kernel;
	for(gate = __gates; gate->portDevice; gate++) {
		if(gate->isBusy == YES)
			mp_gate_release(gate);
	}
}

mp_gate_t *mp_gate_handle(char *id, char *who) {
	mp_gate_t *gate;

	if(!id)
		return(NULL);

	for(gate = __gates; gate->portDevice; gate++) {
		if(mp_options_cmp(gate->portDevice, id) == FALSE)
			continue;

		if(gate->isBusy == YES) {
			mp_printk("Gate %s is used by %s, %s can not have it", gate->portDevice, gate->byWho, who);
			return(NULL);
		}

		gate->isBusy = YES;
		gate->byWho = who;
		mp_gate_clock_on(gate);
		return(gate);
	}

	mp_printk("Gate %s does not exist on this MCU", id);
	return(NULL);
}

void mp_gate_release(mp_gate_t *gate) {
	mp_gate_clock_off(gate);
	gate->isBusy = NO;
	gate->byWho = NULL;
}

unsigned long mp_gate_pclk(mp_gate_t *gate) {
	(void)gate;
	return(mp_clock_peri());
}

/**@}*/
