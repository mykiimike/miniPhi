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
@defgroup mpArchSTM32Gate Peripheral gates

@ingroup mpArchSTM32

Pins are the remap 0 wiring. For a USART pin0 is TX and pin1 RX, for
SPI MOSI, MISO and SCK, for I2C SDA and SCL.

@{
*/

static mp_gate_t __gates[] = {
	{ "USART1", NO, NULL, USART1, USART1_IRQn, -1, &RCC->APB2ENR, RCC_APB2_USART1, 2, "pa9", "pa10", NULL },
	{ "USART2", NO, NULL, USART2, USART2_IRQn, -1, &RCC->APB1ENR, RCC_APB1_USART2, 1, "pa2", "pa3", NULL },
	{ "SPI1", NO, NULL, SPI1, SPI1_IRQn, -1, &RCC->APB2ENR, RCC_APB2_SPI1, 2, "pa7", "pa6", "pa5" },
	{ "I2C1", NO, NULL, I2C1, I2C1_EV_IRQn, I2C1_ER_IRQn, &RCC->APB1ENR, RCC_APB1_I2C1, 1, "pb7", "pb6", NULL },
	{ "TIM1", NO, NULL, TIM1, TIM1_UP_IRQn, -1, &RCC->APB2ENR, RCC_APB2_TIM1, 2, NULL, NULL, NULL },
	{ "TIM2", NO, NULL, TIM2, TIM2_IRQn, -1, &RCC->APB1ENR, RCC_APB1_TIM2, 1, NULL, NULL, NULL },
	{ "TIM3", NO, NULL, TIM3, TIM3_IRQn, -1, &RCC->APB1ENR, RCC_APB1_TIM3, 1, NULL, NULL, NULL },
	{ "TIM4", NO, NULL, TIM4, TIM4_IRQn, -1, &RCC->APB1ENR, RCC_APB1_TIM4, 1, NULL, NULL, NULL },
	{ NULL }
};

void mp_gate_init(mp_kernel_t *kernel) {
	mp_gate_t *gate;

	for(gate = __gates; gate->portDevice; gate++) {
		gate->isBusy = NO;
		gate->byWho = NULL;
	}
}

void mp_gate_fini(mp_kernel_t *kernel) {
	mp_gate_t *gate;

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
	return(gate->apb == 1 ? mp_clock_pclk1() : mp_clock_pclk2());
}

/**@}*/
