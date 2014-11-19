/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

mp_ret_t mp_machine_init(mp_kernel_t *kernel) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	kernel->mcuVendor = "Texas Instrument";

#if defined(__msp430x54xA)
	if ((*((char *)0x1A04) == 0x05) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5438A";
	else if ((*((char *)0x1A04) == 0x04) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5437A";
	else if ((*((char *)0x1A04) == 0x03) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5436A";
	else if ((*((char *)0x1A04) == 0x02) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5435A";
	else if ((*((char *)0x1A04) == 0x01) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5419A";
	else if ((*((char *)0x1A04) == 0x00) && (*((char *)0x1A05) == 0x80))
		kernel->mcuName = "MSP430F5418A";

#elif defined(__msp430x54x)
	if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x38))
		kernel->mcuName = "MSP430F5438";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x37))
		kernel->mcuName = "MSP430F5437";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x36))
		kernel->mcuName = "MSP430F5436";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x35))
		kernel->mcuName = "MSP430F5435";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x19))
		kernel->mcuName = "MSP430F5419";
	else if ((*((char *)0x1A04) == 0x54) && (*((char *)0x1A05) == 0x18))
			kernel->mcuName = "MSP430F5418";
#endif

	__disable_interrupt();

	/* initialize GATEs */
	mp_gate_init(kernel);

	/* initialize interrupts */
	mp_interrupt_init();

	/* initialize GPIO */
	mp_gpio_init();

	/* intialize clock */
	mp_clock_init(kernel);

	/* set high energy */
	mp_clock_high_energy(kernel);

	/* intialize UART */
	mp_uart_init();

	/* erase mem if necessary */
#ifdef _SUPPORT_MEM
	mp_mem_erase(kernel);
#endif

	__enable_interrupt();

	return(TRUE);
}

mp_ret_t mp_machine_fini(mp_kernel_t *kernel) {

	/* intialize UART */
	mp_uart_fini();

	/* terminate clock */
	mp_clock_fini(kernel);

	/* terminate GPIO */
	mp_gpio_fini();

	/* terminate interrupts */
	mp_interrupt_fini();

	/* terminate GATEs */
	mp_gate_fini(kernel);

	return(TRUE);
}


//mp_machine_stop
//mp_machine_
