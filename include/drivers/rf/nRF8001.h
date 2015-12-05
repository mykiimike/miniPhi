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
 * Some parts of this source has been ported from Kris Winer code's.
 * https://github.com/kriswiner/NRF8001
 */

#ifdef SUPPORT_DRV_NRF8001

#ifndef _HAVE_MP_DRV_nRF8001_H
	#define _HAVE_MP_DRV_nRF8001_H

	#include "nRF8001/aci.h"
	#include "nRF8001/aci_cmds.h"
	#include "nRF8001/aci_evts.h"
	#include "nRF8001/aci_protocol_defines.h"

	#include "nRF8001/acilib_defs.h"
	#include "nRF8001/acilib_if.h"
	#include "nRF8001/acilib_types.h"
	#include "nRF8001/acilib.h"
	/**
	 * @defgroup mpDriverNRF8001
	 * @{
	 */

	typedef struct mp_drv_nRF8001_s mp_drv_nRF8001_t;

	typedef struct mp_drv_nRF8001_aci_pkt_s mp_drv_nRF8001_aci_pkt_t;
	typedef struct mp_drv_nRF8001_aci_queue_s mp_drv_nRF8001_aci_queue_t;

	#define MP_NRF8001_STATE_TX_LENGTH 1
	#define MP_NRF8001_STATE_TX 	   2
	#define MP_NRF8001_STATE_RX_DEBUG  10
	#define MP_NRF8001_STATE_RX_LENGTH 11
	#define MP_NRF8001_STATE_RX        12

	struct mp_drv_nRF8001_aci_pkt_s {
		unsigned char length;
		unsigned char opcode;
		unsigned char payload[30];
	};

	struct mp_drv_nRF8001_aci_queue_s {
		unsigned char state;
		mp_drv_nRF8001_aci_pkt_t packet;
		unsigned char rest;

		//mp_drv_nRF8001_aci_cb_t callback;
		void *user;

		mp_list_item_t item;
	};



	struct mp_drv_nRF8001_s {
		unsigned char init;
		unsigned char intSrc;
		unsigned char duplexStatus;

		mp_kernel_t *kernel;

		mp_list_t executing;
		mp_list_t pendingRX;
		mp_list_t pendingTX;

		mp_drv_nRF8001_aci_queue_t *current;

		mp_task_t *task;

		mp_gpio_port_t *reqn;
		mp_gpio_port_t *rdyn;
		mp_gpio_port_t *active;
		mp_gpio_port_t *reset;

		mp_spi_t spi;

	};

	/** @} */

	mp_ret_t mp_drv_nRF8001_init(mp_kernel_t *kernel, mp_drv_nRF8001_t *NRF8001, mp_options_t *options, char *who);
	mp_ret_t mp_drv_nRF8001_fini(mp_drv_nRF8001_t *NRF8001);

	mp_ret_t mp_drv_nRF8001_start(mp_drv_nRF8001_t *nRF8001);
	mp_ret_t mp_drv_nRF8001_stop(mp_drv_nRF8001_t *nRF8001);


#endif

#endif
