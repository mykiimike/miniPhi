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

#ifdef SUPPORT_DRV_NRF8001

#ifndef _HAVE_MP_DRV_nRF8001_H
	#define _HAVE_MP_DRV_nRF8001_H

	/**
	 * @defgroup mpDriverNRF8001
	 * @{
	 */

	typedef struct mp_drv_nRF8001_s mp_drv_nRF8001_t;

	typedef struct mp_drv_nRF8001_aci_pkt_s mp_drv_nRF8001_aci_pkt_t;
	typedef struct mp_drv_nRF8001_aci_queue_s mp_drv_nRF8001_aci_queue_t;

	typedef void (*mp_drv_nRF8001_evts_t)(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue);

	#include "nRF8001/aci.h"
	#include "nRF8001/aci_cmds.h"
	#include "nRF8001/aci_evts.h"
	#include "nRF8001/aci_protocol_defines.h"

	#include "nRF8001/acilib_defs.h"
	#include "nRF8001/acilib_if.h"
	#include "nRF8001/acilib_types.h"
	#include "nRF8001/acilib.h"

	#define MP_NRF8001_STATE_TX_LENGTH 1
	#define MP_NRF8001_STATE_TX 	   2
	#define MP_NRF8001_STATE_RX_LENGTH 10
	#define MP_NRF8001_STATE_RX        11

	struct mp_drv_nRF8001_aci_pkt_s {
		unsigned char length;
		unsigned char payload[31];
	} __attribute__((packed));

	struct mp_drv_nRF8001_aci_queue_s {
		union {
			aci_cmd_t aci;
			mp_drv_nRF8001_aci_pkt_t packet;
		};
		unsigned char rest;

		//mp_drv_nRF8001_aci_cb_t callback;
		void *user;

		mp_list_item_t item;
	};

	#define MP_NRF8001_ACI_QUEUE_SIZE 10

	struct mp_drv_nRF8001_s {
		unsigned char init;
		unsigned char intSrc;

		#define MP_NRF8001_DUPLEX_TX         0x1
		#define MP_NRF8001_DUPLEX_RX         0x2
		#define MP_NRF8001_DUPLEX_TX_PENDING 0x4
		#define MP_NRF8001_DUPLEX_RX_PENDING 0x8
		unsigned char duplexStatus;

		unsigned char statusByte;

		mp_kernel_t *kernel;

		mp_task_t *task;

		mp_gpio_port_t *reqn;
		mp_gpio_port_t *rdyn;
		mp_gpio_port_t *active;
		mp_gpio_port_t *reset;

		mp_spi_t spi;

		mp_list_t pending_tx_pkts;
		mp_list_t pending_rx_pkts;

		mp_drv_nRF8001_aci_queue_t *current_tx_pkts;
		mp_drv_nRF8001_aci_queue_t *current_rx_pkts;

		mp_list_t tx_pkts;
		mp_list_t rx_pkts;

		mp_drv_nRF8001_aci_queue_t inline_pkts[MP_NRF8001_ACI_QUEUE_SIZE*2+1];
		unsigned char inline_pkts_alloc;

		mp_drv_nRF8001_evts_t *evts;

	};

	/** @} */

	mp_ret_t mp_drv_nRF8001_init(mp_kernel_t *kernel, mp_drv_nRF8001_t *NRF8001, mp_options_t *options, char *who);
	mp_ret_t mp_drv_nRF8001_fini(mp_drv_nRF8001_t *NRF8001);

	mp_ret_t mp_drv_nRF8001_start(mp_drv_nRF8001_t *nRF8001);
	mp_ret_t mp_drv_nRF8001_stop(mp_drv_nRF8001_t *nRF8001);

	mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_send_alloc(mp_drv_nRF8001_t *nRF8001);
	mp_bool_t mp_drv_nRF8001_send_queue(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue);
#endif

#endif
