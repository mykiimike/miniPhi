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

#ifndef _HAVE_RP2350_GPIO_H
	#define _HAVE_RP2350_GPIO_H

	typedef enum {
		MP_GPIO_INPUT = 1,
		MP_GPIO_OUTPUT = 2,
	} mp_gpio_direction_t;

	/* same names the kernel and the bit-bang buses pass to mp_gpio_mode() */
	typedef enum {
		MP_GPIO_MODE_ANALOG = 0x0,
		MP_GPIO_MODE_INPUT = 0x4,
		MP_GPIO_MODE_INPUT_PULL = 0x8,
		MP_GPIO_MODE_OUTPUT = 0x2,
		MP_GPIO_MODE_OUTPUT_OD = 0x6,
		MP_GPIO_MODE_AF = 0xB,
		MP_GPIO_MODE_AF_OD = 0xF,
	} mp_gpio_mode_t;

	typedef struct mp_gpio_port_s mp_gpio_port_t;

	struct mp_gpio_port_s {
		/** always 0: the RP2350A user pins are one bank, pa0..pa29 */
		unsigned int port;
		unsigned int pin;
		char *who;
		mp_bool_t used;
		char direction;
		mp_interrupt_cb_t callback;
		void *user;
		char reverse;
		unsigned int af;
	};

	void mp_gpio_init();
	void mp_gpio_fini();
	mp_gpio_port_t *mp_gpio_handle(unsigned int port, unsigned int slot, char *who);

	/* "pa12", "PA12" or "pa.12" */
	mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who);
	mp_ret_t mp_gpio_release(mp_gpio_port_t *port);

	mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction);
	void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode);

	/* FUNCSEL: 1 SPI, 2 UART, 3 I2C, 4 PWM, 5 SIO */
	void mp_gpio_af(mp_gpio_port_t *port, unsigned int af);

	void mp_gpio_pull_up(mp_gpio_port_t *port);
	mp_bool_t mp_gpio_read(mp_gpio_port_t *port);
	void mp_gpio_set(mp_gpio_port_t *port);
	void mp_gpio_unset(mp_gpio_port_t *port);
	void mp_gpio_turn(mp_gpio_port_t *port);

	mp_ret_t mp_gpio_interrupt_set(mp_gpio_port_t *port, mp_interrupt_cb_t in, void *user, char *who);
	mp_ret_t mp_gpio_interrupt_unset(mp_gpio_port_t *port);

	mp_bool_t mp_gpio_wake_arm(mp_gpio_port_t *port);
	void mp_gpio_wake_disarm(mp_gpio_port_t *port);

	static inline void mp_gpio_pin_set(unsigned int pin) {
		SIO_GPIO_OUT_SET = 1UL << pin;
	}

	static inline void mp_gpio_pin_reset(unsigned int pin) {
		SIO_GPIO_OUT_CLR = 1UL << pin;
	}

	static inline void mp_gpio_interrupt_enable(mp_gpio_port_t *port) {
		unsigned int shift = (port->pin & 7) * 4;
		IO_BANK0_PROC0_INTE(port->pin >> 3) |= IO_GPIO_IRQ_EDGE << shift;
	}

	static inline void mp_gpio_interrupt_disable(mp_gpio_port_t *port) {
		unsigned int shift = (port->pin & 7) * 4;
		IO_BANK0_PROC0_INTE(port->pin >> 3) &= ~(0xFUL << shift);
	}

	static inline void mp_gpio_interrupt_lo2hi(mp_gpio_port_t *port) {
		unsigned int shift = (port->pin & 7) * 4;
		uint32_t mask = 0xFUL << shift;
		IO_BANK0_PROC0_INTE(port->pin >> 3) = (IO_BANK0_PROC0_INTE(port->pin >> 3) & ~mask) | (IO_GPIO_IRQ_RISE << shift);
	}

	static inline void mp_gpio_interrupt_hi2lo(mp_gpio_port_t *port) {
		unsigned int shift = (port->pin & 7) * 4;
		uint32_t mask = 0xFUL << shift;
		IO_BANK0_PROC0_INTE(port->pin >> 3) = (IO_BANK0_PROC0_INTE(port->pin >> 3) & ~mask) | (IO_GPIO_IRQ_FALL << shift);
	}

	static inline void mp_gpio_interrupt_hilo_switch(mp_gpio_port_t *port) {
		unsigned int shift = (port->pin & 7) * 4;
		IO_BANK0_PROC0_INTE(port->pin >> 3) ^= (IO_GPIO_IRQ_EDGE << shift);
	}

#endif
