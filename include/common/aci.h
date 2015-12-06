/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2015  Michael VERGOZ                                      *
 * Copyright (C) 2015  VERMAN                                              *
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

#ifndef _HAVE_MP_COMMON_ACI_H
	#define _HAVE_MP_COMMON_ACI_H

	#define MP_ACI_SPI 1
	#define MP_ACI_I2C 2

	#define MP_ACI_STATE_TX     1
	#define MP_ACI_STATE_RX     2
	#define MP_ACI_STATE_NULLRX 3
	#define MP_ACI_STATE_NULLTX 4
	/**
	 * @defgroup mpCommonRegMaster
	 * @{
	 */
	typedef struct mp_drv_nRF8001_aci_s mp_drv_nRF8001_aci_t;

	typedef void (*mp_drv_nRF8001_aci_cb_t)(mp_drv_nRF8001_aci_op_t *operand, mp_bool_t terminate);
	typedef void (*mp_drv_nRF8001_aci_int_t)(mp_drv_nRF8001_aci_t *aci);
	typedef void (*mp_drv_nRF8001_aci_read_t)(mp_drv_nRF8001_aci_t *aci, mp_drv_nRF8001_aci_queue_t *queue);

	typedef struct mp_drv_nRF8001_aci_pkt_s mp_drv_nRF8001_aci_pkt_t;
	typedef struct mp_drv_nRF8001_aci_queue_s mp_drv_nRF8001_aci_queue_t;

	struct mp_drv_nRF8001_aci_pkt_s {
		unsigned char length;
		unsigned char opcode;
		unsigned char payload[30];
	};

	struct mp_drv_nRF8001_aci_queue_s {
		mp_drv_nRF8001_aci_pkt_t packet;

		mp_drv_nRF8001_aci_cb_t callback;
		void *user;

		mp_list_item_t item;
	};

	struct mp_drv_nRF8001_aci_s {
		mp_kernel_t *kernel;

		mp_list_t executing;
		mp_list_t pending;

		mp_drv_nRF8001_aci_int_t enableREQN;
		mp_drv_nRF8001_aci_int_t disableREQN;

		mp_drv_nRF8001_aci_int_t enableRDYN;
		mp_drv_nRF8001_aci_int_t disableRDYN;

		mp_drv_nRF8001_aci_read_t onRead;

		mp_spi_t *spi;

		/** Activate swap */
		mp_bool_t swap;

		/* Protocol ASR */
		mp_drv_nRF8001_aci_asr_t asrCallback;

		void *user;

		mp_task_t *asr;
	};


	mp_ret_t mp_drv_nRF8001_aci_init_spi(
		mp_kernel_t *kernel, mp_drv_nRF8001_aci_t *aci,
		mp_spi_t *spi,
		void *user,
		char *who
	);
	void mp_drv_nRF8001_aci_fini(mp_drv_nRF8001_aci_t *aci);


	mp_ret_t mp_drv_nRF8001_aci_write(
		mp_drv_nRF8001_aci_t *aci,
		mp_drv_nRF8001_aci_pkt_t aciPkt,
		mp_drv_nRF8001_aci_cb_t callback, void *user
	);

	/** @} */
#endif
