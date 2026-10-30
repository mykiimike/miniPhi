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
@defgroup mpArchESP32S3Machine Machine

@ingroup mpArchESP32

CPU0 only. The APP CPU stays in the reset the ROM left it in.

@{
*/

static void _name(mp_kernel_t *kernel) {
	snprintf(kernel->arch.mcuName, sizeof(kernel->arch.mcuName), "ESP32-S3, 512 KB SRAM");
	kernel->mcuVendor = "Espressif";
	kernel->mcuName = kernel->arch.mcuName;
}

const char *mp_machine_serial(void) {
	static char text[13];
	unsigned long lo;
	unsigned long hi;

	if(text[0])
		return(text);
	lo = s3_read(S3_EFUSE + 0x44UL);
	hi = s3_read(S3_EFUSE + 0x48UL);
	snprintf(text, sizeof(text), "%02lx%02lx%02lx%02lx%02lx%02lx",
		lo & 0xffUL, (lo >> 8) & 0xffUL, (lo >> 16) & 0xffUL, (lo >> 24) & 0xffUL,
		hi & 0xffUL, (hi >> 8) & 0xffUL);
	return(text);
}

mp_ret_t mp_machine_init(mp_kernel_t *kernel) {
	_name(kernel);

	mp_gate_init(kernel);
	mp_interrupt_init();
	mp_gpio_init();
	mp_timer_init(kernel);
	mp_clock_init(kernel);
	mp_uart_init();
	mp_spi_init();
	mp_i2c_init();
	mp_interrupt_enable();
	return(TRUE);
}

mp_ret_t mp_machine_fini(mp_kernel_t *kernel) {
	mp_interrupt_disable();
	mp_i2c_fini();
	mp_spi_fini();
	mp_uart_fini();
	mp_clock_fini(kernel);
	mp_timer_fini(kernel);
	mp_gpio_fini();
	mp_interrupt_fini();
	mp_gate_fini(kernel);
	return(TRUE);
}

void mp_machine_reset(void) {
	s3_write(S3_RTC, 1UL << 31);
	for(;;)
		;
}

void mp_machine_state_set(mp_kernel_t *kernel) {
	(void)kernel;
}

void mp_machine_state_unset(mp_kernel_t *kernel) {
	(void)kernel;
}

/**@}*/
