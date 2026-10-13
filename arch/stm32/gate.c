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
#if defined(STM32F10X_HD) || defined(STM32F10X_XL)
	{ "USART3", NO, NULL, USART3, USART3_IRQn, -1, &RCC->APB1ENR, RCC_APB1_USART3, 1, "pb10", "pb11", NULL },
	{ "UART4", NO, NULL, UART4, UART4_IRQn, -1, &RCC->APB1ENR, RCC_APB1_UART4, 1, "pc10", "pc11", NULL },
	{ "UART5", NO, NULL, UART5, UART5_IRQn, -1, &RCC->APB1ENR, RCC_APB1_UART5, 1, "pc12", "pd2", NULL },
	{ "SPI2", NO, NULL, SPI2, SPI2_IRQn, -1, &RCC->APB1ENR, RCC_APB1_SPI2, 1, "pb15", "pb14", "pb13" },
	{ "SPI3", NO, NULL, SPI3, SPI3_IRQn, -1, &RCC->APB1ENR, RCC_APB1_SPI3, 1, "pb5", "pb4", "pb3" },
	{ "I2C2", NO, NULL, I2C2, I2C2_EV_IRQn, I2C2_ER_IRQn, &RCC->APB1ENR, RCC_APB1_I2C2, 1, "pb11", "pb10", NULL },
	{ "TIM5", NO, NULL, TIM5, TIM5_IRQn, -1, &RCC->APB1ENR, RCC_APB1_TIM5, 1, NULL, NULL, NULL },
	{ "TIM6", NO, NULL, TIM6, TIM6_IRQn, -1, &RCC->APB1ENR, RCC_APB1_TIM6, 1, NULL, NULL, NULL },
	{ "TIM7", NO, NULL, TIM7, TIM7_IRQn, -1, &RCC->APB1ENR, RCC_APB1_TIM7, 1, NULL, NULL, NULL },
	{ "TIM8", NO, NULL, TIM8, TIM8_UP_IRQn, -1, &RCC->APB2ENR, RCC_APB2_TIM8, 2, NULL, NULL, NULL },
	/* on AHB: mp_gate_pclk() does not apply */
	{ "FSMC", NO, NULL, FSMC_Bank1, -1, -1, &RCC->AHBENR, RCC_AHB_FSMC, 0, NULL, NULL, NULL },
#endif
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
