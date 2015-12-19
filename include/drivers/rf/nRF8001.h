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

	typedef struct mp_drv_nRF8001_setup_s mp_drv_nRF8001_setup_t;
	typedef struct mp_drv_nRF8001_aci_pkt_s mp_drv_nRF8001_aci_pkt_t;
	typedef struct mp_drv_nRF8001_aci_queue_s mp_drv_nRF8001_aci_queue_t;

	typedef void (*mp_drv_nRF8001_evts_t)(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue);

	typedef void (*mp_drv_nRF8001_event_t)(mp_drv_nRF8001_t *nRF8001);

	#include "nRF8001/aci.h"
	#include "nRF8001/aci_cmds.h"
	#include "nRF8001/aci_evts.h"

	#define MP_NRF8001_STATE_TX_LENGTH 1
	#define MP_NRF8001_STATE_TX 	   2
	#define MP_NRF8001_STATE_RX_LENGTH 10
	#define MP_NRF8001_STATE_RX        11

	struct mp_drv_nRF8001_setup_s {
		unsigned char nullize;
		unsigned char payload[32];
	} __attribute__((packed));

	struct mp_drv_nRF8001_aci_pkt_s {
		unsigned char length;
		unsigned char payload[31];
	} __attribute__((packed));

	struct mp_drv_nRF8001_aci_queue_s {
		union {
			aci_evt_t evt;
			aci_cmd_t cmd;
			mp_drv_nRF8001_aci_pkt_t packet;
		};
		unsigned char rest;

		//mp_drv_nRF8001_aci_cb_t callback;
		void *user;

		mp_list_item_t item;
	};

	#define MP_NRF8001_ACI_QUEUE_SIZE 10

	struct mp_drv_nRF8001_s {
		mp_kernel_t *kernel;

		/** Initialisation status */
		unsigned char init;

		#define MP_NRF8001_INTSRC_ECHO     0x2
		#define MP_NRF8001_INTSRC_RDYN     0x4
		#define MP_NRF8001_INTSRC_REQN     0x8
		#define MP_NRF8001_INTSRC_BOND     0x10
		#define MP_NRF8001_INTSRC_CONNECT  0x20
		#define MP_NRF8001_INTSRC_BCAST    0x30
		/** Lines status */
		unsigned char intSrc;

		/** This define the number echo test processed */
		unsigned char echoTest;

		#define MP_NRF8001_DUPLEX_TX         0x1
		#define MP_NRF8001_DUPLEX_RX         0x2
		#define MP_NRF8001_DUPLEX_TX_PENDING 0x4
		#define MP_NRF8001_DUPLEX_RX_PENDING 0x8
		/** Duplex status */
		unsigned char duplexStatus;

		/** Dummy internal status byte from RX packet */
		unsigned char statusByte;

		/** Last pipe status received */
		aci_evt_params_pipe_status_t pipeStatus;

		/** Data credits available */
		signed char dataCredits;

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

		mp_drv_nRF8001_aci_queue_t inline_pkts[MP_NRF8001_ACI_QUEUE_SIZE+2];
		unsigned char inline_pkts_alloc;

		/* List of internal events handler */
		mp_drv_nRF8001_evts_t evts[256];

		/* List of external pipe RX handler */
		mp_drv_nRF8001_evts_t pipeRxHandler[64];

		/** nRF Go Setup message */
		mp_drv_nRF8001_setup_t *setup;
		unsigned char setup_size;
		unsigned char setup_idx;

		aci_services_pipe_type_mapping_t *pipe_map;
		unsigned char pipe_map_size;

		/*- user definition -*/

		/** On board is ready to use */
		mp_drv_nRF8001_event_t onReady;

		/** New connection */
		mp_drv_nRF8001_event_t onConnect;

		/** User disconnected or end of discovery */
		mp_drv_nRF8001_event_t onDisconnect;

		/** Embedded user pointer */
		void *user;

	};

	/** @} */

	mp_ret_t mp_drv_nRF8001_init(mp_kernel_t *kernel, mp_drv_nRF8001_t *NRF8001, mp_options_t *options, char *who);
	mp_ret_t mp_drv_nRF8001_fini(mp_drv_nRF8001_t *NRF8001);

	mp_ret_t mp_drv_nRF8001_go(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_setup_t *setup, int messages, aci_services_pipe_type_mapping_t *pipe, int pipe_size);

	mp_ret_t mp_drv_nRF8001_start(mp_drv_nRF8001_t *nRF8001);
	mp_ret_t mp_drv_nRF8001_stop(mp_drv_nRF8001_t *nRF8001);

	mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_send_alloc(mp_drv_nRF8001_t *nRF8001);
	mp_bool_t mp_drv_nRF8001_send_queue(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue);

	mp_ret_t mp_drv_nRF8001_is_pipe_available(mp_drv_nRF8001_t *nRF8001, uint8_t pipe);
	mp_ret_t mp_drv_nRF8001_is_pipe_closed(mp_drv_nRF8001_t *nRF8001, uint8_t pipe);
	mp_ret_t mp_drv_nRF8001_pipe_receive(mp_drv_nRF8001_t *nRF8001, uint8_t pipe, mp_drv_nRF8001_evts_t callback);


#endif

#endif
