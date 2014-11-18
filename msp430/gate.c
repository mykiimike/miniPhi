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

/*
 * Ti msp430 gate features
 */

#include <mp.h>

static mp_gate_t __gate[10];
static unsigned char __gate_count = 0;

void mp_gate_init(mp_kernel_t *kernel) {

	mp_gate_t *gate;

	memset(&__gate, 0, sizeof(__gate));
	__gate_count = 0;

#ifdef __MSP430_HAS_USCI_A0__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_A0";
	gate->_baseAddress = USCI_A0_BASE;
	gate->_ISRVector = USCI_A0_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = NO;
	__gate_count++;
#endif

#ifdef __MSP430_HAS_USCI_B0__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_B0";
	gate->_baseAddress = USCI_B0_BASE;
	gate->_ISRVector = USCI_B0_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = YES;
	__gate_count++;
#endif

#ifdef __MSP430_HAS_USCI_A1__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_A1";
	gate->_baseAddress = USCI_A1_BASE;
	gate->_ISRVector = USCI_A1_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = NO;
	__gate_count++;
#endif

#ifdef __MSP430_HAS_USCI_B1__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_B1";
	gate->_baseAddress = USCI_B1_BASE;
	gate->_ISRVector = USCI_B1_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = YES;
	__gate_count++;
#endif

#ifdef __MSP430_HAS_USCI_A2__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_A2";
	gate->_baseAddress = USCI_A2_BASE;
	gate->_ISRVector = USCI_A2_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = NO;
	__gate_count++;
#endif

#ifdef __MSP430_HAS_USCI_B2__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_B2";
	gate->_baseAddress = USCI_B2_BASE;
	gate->_ISRVector = USCI_B2_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = YES;
	__gate_count++;
#endif

#ifdef __MSP430_HAS_USCI_A3__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_A3";
	gate->_baseAddress = USCI_A3_BASE;
	gate->_ISRVector = USCI_A3_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = NO;
	__gate_count++;
#endif

#ifdef __MSP430_HAS_USCI_B3__
	gate = &__gate[__gate_count];
	gate->portDevice = "USCI_B3";
	gate->_baseAddress = USCI_B3_BASE;
	gate->_ISRVector = USCI_B3_VECTOR;
	gate->isBusy = NO;
	gate->_registersB = YES;
	__gate_count++;
#endif

}

void mp_gate_fini(mp_kernel_t *kernel) {
	/* none */

}

mp_gate_t *mp_gate_handle(char *id, char *who) {
	mp_gate_t *gate;
	int a;

	/* lookup gate */
	for(a=0; a<__gate_count; a++) {
		gate = &__gate[a];
		if(strcmp(id, gate->portDevice) == 0) {
			if(gate->isBusy == YES)
				return(NULL);
			gate->isBusy = YES;
			gate->byWho = who;
			return(gate);
		}
	}

	return(NULL);
}

void mp_gate_release(mp_gate_t *gate) {
	gate->isBusy = NO;
	gate->byWho = NULL;
}

