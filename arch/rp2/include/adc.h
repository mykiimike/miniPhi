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

#ifndef _HAVE_RP2350_ADC_H
	#define _HAVE_RP2350_ADC_H

	#include "rp2350.h"

	/* RP2350A (QFN-60): 0..3 are GPIO26..29, 4 is the die temperature.
	   pico-sdk hardware/adc.h. The SAR has no bandgap input; its reference
	   is the analog supply, so MP_ADC_CHANNEL_VREF is not a mux channel. */
	#define MP_ADC_CHANNEL_TEMP  4
	#define MP_ADC_CHANNEL_VREF  (-1)
	#define MP_ADC_CHANNEL_MAX   4

	typedef struct mp_adc_s mp_adc_t;

	typedef void (*mp_adc_callback_t)(mp_adc_t *adc);

	struct mp_adc_s {
		mp_gpio_port_t *port;

		mp_kernel_t *kernel;

		char state;

		char channelId;

		/* 12-bit result, 0 to 4095 of the analog supply */
		int result;

		mp_adc_callback_t callback;

		void *user;

		mp_task_t *task;

		mp_list_item_t item;
	};

	/* options: channel "A0" to "A4", port (puts the pin in analog mode), delay between captures in ticks */
	mp_ret_t mp_adc_create(mp_kernel_t *kernel, mp_adc_t *adc, mp_options_t *options, const char *who);
	mp_ret_t mp_adc_remove(mp_adc_t *adc);
	void mp_adc_enable(mp_adc_t *adc);
	void mp_adc_disable(mp_adc_t *adc);
	void mp_adc_start(void);
	void mp_adc_stop(void);
	mp_ret_t mp_adc_state(void);
	void mp_adc_restore(mp_bool_t status);

	/** keeps the ADC on when no channel owns it; paired with mp_adc_close() */
	mp_ret_t mp_adc_open(mp_kernel_t *kernel);
	void mp_adc_close(void);

	/**
	 * One polled conversion, 0 to 4095.
	 * -1 when the channel is invalid or a conversion is already running.
	 */
	int mp_adc_convert(int channel);

	/**
	 * ADC channel of a GPIO pin.
	 * GPIO26..GPIO29 are channels 0..3.
	 * -1 when the pin is not an ADC input.
	 */
	int mp_adc_channel(mp_gpio_port_t *pin);

	void mp_adc_idle(void);
	mp_bool_t mp_adc_quiet(void);

	/* clk_adc is the 48 MHz USB domain. pclk2 is not its parent. */
	void mp_adc_prescale(unsigned long pclk2);

	/* ADC 0x400a0000. pico-sdk rp2350 hardware/regs/adc.h. */
	#define RP2350_ADC_BASE 0x400a0000UL

	#define ADC_CS     RP2350_REG(RP2350_ADC_BASE, 0x00)
	#define ADC_RESULT RP2350_REG(RP2350_ADC_BASE, 0x04)
	#define ADC_FCS    RP2350_REG(RP2350_ADC_BASE, 0x08)
	#define ADC_FIFO   RP2350_REG(RP2350_ADC_BASE, 0x0C)
	#define ADC_DIV    RP2350_REG(RP2350_ADC_BASE, 0x10)
	#define ADC_INTR   RP2350_REG(RP2350_ADC_BASE, 0x14)
	#define ADC_INTE   RP2350_REG(RP2350_ADC_BASE, 0x18)
	#define ADC_INTF   RP2350_REG(RP2350_ADC_BASE, 0x1C)
	#define ADC_INTS   RP2350_REG(RP2350_ADC_BASE, 0x20)

	#define ADC_CS_AINSEL_LSB  12
	#define ADC_CS_ERR_STICKY  (1UL << 10)
	#define ADC_CS_ERR         (1UL << 9)
	#define ADC_CS_READY       (1UL << 8)
	#define ADC_CS_START_MANY  (1UL << 3)
	#define ADC_CS_START_ONCE  (1UL << 2)
	#define ADC_CS_TS_EN       (1UL << 1)
	#define ADC_CS_EN          (1UL << 0)

	#define ADC_FCS_THRESH_LSB 24
	#define ADC_FCS_OVER       (1UL << 11)
	#define ADC_FCS_UNDER      (1UL << 10)
	#define ADC_FCS_EMPTY      (1UL << 8)
	#define ADC_FCS_EN         (1UL << 0)

	#define ADC_INT_FIFO       (1UL << 0)

	/* clk_adc. pico-sdk rp2350 clocks.h: CTRL 0x6c, DIV 0x70.
	   AUXSRC 0 is clksrc_pll_usb. ENABLE is bit 11, ENABLED is bit 28.
	   No glitchless mux. Integer divisor is bits 19:16, same place as CLOCKS_DIV_INT1. */
	#define CLOCKS_CLK_ADC_CTRL RP2350_REG(RP2350_CLOCKS_BASE, 0x6C)
	#define CLOCKS_CLK_ADC_DIV  RP2350_REG(RP2350_CLOCKS_BASE, 0x70)
	#define CLOCKS_ADC_ENABLE   (1UL << 11)
	#define CLOCKS_ADC_ENABLED  (1UL << 28)
	#define CLOCKS_ADC_AUX_USB  0UL

#endif
