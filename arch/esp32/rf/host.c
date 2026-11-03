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
#include "host.h"

/**
@addtogroup mpArchESP32RF

Calls every Espressif libphy makes into the kernel: the wait and
the interrupt mask. phy_printf() is mp_printk(). The open path is
arch/esp32/rf/rf.c and the analog master is arch/esp32/rf/i2c.c.
The series keeps the calibration image, the crystal, the power hook
and the ready test.

@{
*/

static mp_bool_t _crit_state;
static int _crit_depth;

void mp_arch_rf_delay_us(uint32_t us) {
	unsigned long start;

	if(us == 0)
		return;
	start = mp_clock_us();
	while((long)(mp_clock_us() - start) < (long)us)
		;
}

static void _enter(void) {
	if(_crit_depth++ == 0)
		_crit_state = mp_interrupt_state();
	mp_interrupt_disable();
}

static void _exit(void) {
	if(--_crit_depth <= 0) {
		_crit_depth = 0;
		mp_interrupt_restore(_crit_state);
	}
}

void mp_arch_rf_i2c_enter_critical(void) {
	_enter();
}

void mp_arch_rf_i2c_exit_critical(void) {
	_exit();
}

void mp_arch_rf_enter_critical(void) {
	_enter();
}

void mp_arch_rf_exit_critical(void) {
	_exit();
}

/**@}*/
