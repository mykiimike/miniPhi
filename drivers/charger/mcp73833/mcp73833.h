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

#ifndef _HAVE_MP_DRV_MCP73833_H
	#define _HAVE_MP_DRV_MCP73833_H

	/**
	 * Microchip MCP73833 standalone Li-Ion charger. STAT1 and STAT2 are
	 * active-low open-drain inputs. STAT1 low is charging, STAT2 low is
	 * charge complete, both released is waiting. Both low leaves the device
	 * on its last state. The board pulls the lines up to the MCU rail: the
	 * charger's own LEDs pull toward its 5 V input.
	 *
	 * It samples one MP_DEVICE_CHARGER. The first subscriber starts a read
	 * every 500 ms. The device publishes a state once the same reading
	 * comes back.
	 *
	 * options: model, serial, stat1, stat2.
	 */

	typedef struct mp_drv_mcp73833_s mp_drv_mcp73833_t;

	struct mp_drv_mcp73833_s {
		mp_kernel_t *kernel;
		mp_gpio_port_t *stat1;
		mp_gpio_port_t *stat2;
		mp_task_t *task;

		mp_device_driver_t binding;
		mp_device_registry_t dev;
		mp_device_charge_t charge;

		const char *model;
		const char *serial;
	};

	mp_ret_t mp_drv_mcp73833_init(mp_kernel_t *kernel, mp_drv_mcp73833_t *chg, mp_options_t *options, char *who);
	void mp_drv_mcp73833_fini(mp_drv_mcp73833_t *chg);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_CHARGER */
	extern const mp_device_ops_t mp_drv_mcp73833_device;
#endif

#endif
