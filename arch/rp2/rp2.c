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
@defgroup mpArchRP2350Machine Machine

@ingroup mpArchRP2350

@{
*/

static void _mp_machine_name(mp_kernel_t *kernel) {
	snprintf(kernel->arch.mcuName, sizeof(kernel->arch.mcuName), "RP2350A, 16384 KB flash");
	kernel->mcuVendor = "Raspberry Pi";
	kernel->mcuName = kernel->arch.mcuName;
}

const char *mp_machine_serial(void) {
	static char text[20];
	uint32_t id;

	/* SYSINFO.CHIP_ID is the part and revision, not a wafer id. */
	if(text[0])
		return(text);
	id = *(volatile uint32_t *)RP2350_SYSINFO_BASE;
	snprintf(text, sizeof(text), "rp2350-%08lx", (unsigned long)id);
	return(text);
}

mp_ret_t mp_machine_init(mp_kernel_t *kernel) {
	_mp_machine_name(kernel);

	mp_gate_init(kernel);
	mp_interrupt_init();
	mp_gpio_init();
	mp_timer_init(kernel);
	mp_clock_init(kernel);
	mp_rp2350_pixel(kernel);
	mp_uart_init();
	mp_spi_init();
	mp_i2c_init();
	mp_interrupt_enable();
	return(TRUE);
}

mp_ret_t mp_machine_fini(mp_kernel_t *kernel) {
	mp_i2c_fini();
	mp_spi_fini();
	mp_uart_fini();
	mp_clock_fini(kernel);
	mp_timer_fini(kernel);
	mp_rp2350_pixel_fini(kernel);
	mp_gpio_fini();
	mp_interrupt_fini();
	mp_gate_fini(kernel);
	return(TRUE);
}

void mp_machine_reset(void) {
	__asm volatile ("dsb" ::: "memory");
	SCB->AIRCR = SCB_AIRCR_RESET;
	__asm volatile ("dsb" ::: "memory");
	while(1);
}

void mp_machine_state_set(mp_kernel_t *kernel) {
	(void)kernel;
}

void mp_machine_state_unset(mp_kernel_t *kernel) {
	(void)kernel;
}

/**@}*/
