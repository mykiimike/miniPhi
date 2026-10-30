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

#ifndef _HAVE_ESP32_GPIO_H
	#define _HAVE_ESP32_GPIO_H

	typedef enum {
		MP_GPIO_INPUT = 1,
		MP_GPIO_OUTPUT = 2,
	} mp_gpio_direction_t;

	typedef enum {
		MP_GPIO_MODE_INPUT = 1,
		MP_GPIO_MODE_INPUT_PULL = 2,
		MP_GPIO_MODE_OUTPUT = 3,
		MP_GPIO_MODE_OUTPUT_OD = 4,
	} mp_gpio_mode_t;

	typedef struct mp_gpio_port_s mp_gpio_port_t;

	struct mp_gpio_port_s {
		unsigned int port;
		unsigned int pin;
		char *who;
		mp_bool_t used;
		char direction;
		mp_interrupt_cb_t callback;
		void *user;
		char reverse;
	};

	void mp_gpio_init(void);
	void mp_gpio_fini(void);
	mp_gpio_port_t *mp_gpio_handle(unsigned int port, unsigned int slot, char *who);

	/* "pa15", "PA15" or "pa.15". Port A is the only port; the number is the GPIO. */
	mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who);
	mp_ret_t mp_gpio_release(mp_gpio_port_t *port);

	mp_ret_t mp_gpio_direction(mp_gpio_port_t *port, mp_gpio_direction_t direction);
	void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode);
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

	void mp_gpio_interrupt_enable(mp_gpio_port_t *port);
	void mp_gpio_interrupt_disable(mp_gpio_port_t *port);
	void mp_gpio_interrupt_lo2hi(mp_gpio_port_t *port);
	void mp_gpio_interrupt_hi2lo(mp_gpio_port_t *port);
	void mp_gpio_interrupt_hilo_switch(mp_gpio_port_t *port);

#endif
