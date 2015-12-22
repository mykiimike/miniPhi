/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2015  Michael VERGOZ                                      *
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
 * Inc., 51 Franklin STreet, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_DRV_NRF8001

static void _mp_drv_nRF8001_evt_null(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("nRF8001(%p) Warning NULL event executed opcode=%x", nRF8001, queue->evt.evt_opcode);
}

static void _mp_drv_nRF8001_evt_pipe_null(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("nRF8001(%p) Warning NULL pipe RX", nRF8001);
}


static void _mp_drv_nRF8001_evt_deviceStarted(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_drv_nRF8001_aci_queue_t *q;

	aci_evt_params_device_started_t *ptr = &queue->evt.params.device_started;

	switch(ptr->device_mode) {
		case ACI_DEVICE_INVALID:
			break;

		case ACI_DEVICE_TEST:
			mp_printk("nRF8001(%p) Starting ECHO test", nRF8001);
			q = mp_drv_nRF8001_cmd_echo(nRF8001, "ABCDEFGH", 8);
			mp_drv_nRF8001_send_queue(nRF8001, q);
			break;

		case ACI_DEVICE_SETUP:
			if(!(nRF8001->intSrc & MP_NRF8001_INTSRC_ECHO)) {
				q = mp_drv_nRF8001_cmd_test(nRF8001, ACI_TEST_MODE_DTM_ACI);
				mp_drv_nRF8001_send_queue(nRF8001, q);

				nRF8001->intSrc |= MP_NRF8001_INTSRC_ECHO;
			}
			else {
				q = mp_drv_nRF8001_cmd_get_device_version(nRF8001);
				mp_drv_nRF8001_send_queue(nRF8001, q);
			}

			break;

		case ACI_DEVICE_STANDBY:
			/* update data credits */
			nRF8001->dataCredits = ptr->credit_available;

			if(nRF8001->onReady)
				nRF8001->onReady(nRF8001);
			break;

		case ACI_DEVICE_SLEEP:
			break;
	}



	mp_printk("nRF8001(%p) Device status error=%x mode=%x, credit=%d", nRF8001, ptr->hw_error, ptr->device_mode, ptr->credit_available);


}

static void _mp_drv_nRF8001_evt_echo(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_drv_nRF8001_aci_queue_t *q;

	int size = queue->evt.len-1;
	queue->evt.params.echo.echo_data[size] = '\0';

	mp_printk("nRF8001(%p) receiving echo DTM %s", nRF8001, queue->evt.params.echo.echo_data);

	if(strncmp((const char *)queue->evt.params.echo.echo_data, "ABCDEFGH", 8) != 0) {
		mp_printk("nRF8001(%p) ECHO test error at packet %d", nRF8001, nRF8001->echoTest);
	}

	if(nRF8001->echoTest >= 2) {
		nRF8001->echoTest++;
		mp_printk("nRF8001(%p) ECHO test done with %d messages", nRF8001, nRF8001->echoTest);
		q = mp_drv_nRF8001_cmd_test(nRF8001, ACI_TEST_MODE_EXIT);
		mp_drv_nRF8001_send_queue(nRF8001, q);
	}
	else {
		q = mp_drv_nRF8001_cmd_echo(nRF8001, "ABCDEFGH", 8);
		mp_drv_nRF8001_send_queue(nRF8001, q);
		nRF8001->echoTest++;
	}

}

static void _mp_drv_nRF8001_evt_hwError(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("nRF8001(%p) FATAL Hardware error line %d in %s", nRF8001, queue->evt.params.hw_error.line_num, queue->evt.params.hw_error.file_name);
}

static void _mp_drv_nRF8001_evt_cmdResponse(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	aci_evt_params_cmd_rsp_t *res = &queue->evt.params.cmd_rsp;
	aci_evt_cmd_rsp_params_get_device_version_t *gdv;
	aci_evt_cmd_rsp_params_get_device_address_t *gda;
	mp_drv_nRF8001_aci_queue_t *q;

	switch(res->cmd_opcode) {
		case ACI_CMD_GET_DEVICE_VERSION:
			gdv = &res->params.get_device_version;
			mp_printk("nRF8001(%p) ACI version %d, configuration HW/FW %x format=%d id=%x status=%x",
					nRF8001, gdv->aci_version, gdv->configuration_id, gdv->setup_format, gdv->setup_id, gdv->setup_status);

			q = mp_drv_nRF8001_cmd_get_device_address(nRF8001);
			mp_drv_nRF8001_send_queue(nRF8001, q);

			break;

		case ACI_CMD_GET_DEVICE_ADDRESS:
			gda = &res->params.get_device_address;
			mp_printk(
					"nRF8001(%p) Device BLE address %02x%02x%02x%02x%02x%02x type %x", nRF8001,
					gda->bd_addr_own[0],
					gda->bd_addr_own[1],
					gda->bd_addr_own[2],
					gda->bd_addr_own[3],
					gda->bd_addr_own[4],
					gda->bd_addr_own[5],
					gda->bd_addr_type
			);

		case ACI_CMD_SETUP:
			if(res->cmd_status == ACI_STATUS_SUCCESS) {
				q = mp_drv_nRF8001_cmd_setup(nRF8001);
				mp_drv_nRF8001_send_queue(nRF8001, q);
				mp_printk("nRF8001(%p) Starting setup", nRF8001);
			}
			else if(res->cmd_status == ACI_STATUS_TRANSACTION_CONTINUE) {
				q = mp_drv_nRF8001_cmd_setup(nRF8001);
				mp_drv_nRF8001_send_queue(nRF8001, q);
				//mp_printk("nRF8001(%p) Setup continues message #%d", nRF8001, nRF8001->setup_idx);
			}
			else if(res->cmd_status == ACI_STATUS_TRANSACTION_COMPLETE) {
				mp_printk("nRF8001(%p) Setup completed with %d messages", nRF8001, nRF8001->setup_size);
			}
			else
				mp_printk("nRF8001(%p) setup %x", nRF8001, res->cmd_status);
			break;

		case ACI_CMD_BOND:
			nRF8001->intSrc |= MP_NRF8001_INTSRC_BOND;
			mp_printk("nRF8001(%p) Bonding mode status %x", nRF8001, res->cmd_status);
			break;

		case ACI_CMD_CONNECT:
			nRF8001->intSrc |= MP_NRF8001_INTSRC_CONNECT;
			mp_printk("nRF8001(%p) Connect mode status %x", nRF8001, res->cmd_status);
			break;

		case ACI_CMD_SET_LOCAL_DATA:
			if(nRF8001->onLocalData)
				nRF8001->onLocalData(nRF8001);
			break;

		default:
			mp_printk("nRF8001(%p) Response event command opcode %x w/ status %x", nRF8001, res->cmd_opcode, res->cmd_status);
			break;
	}
}

static void _mp_drv_nRF8001_evt_connected(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	aci_evt_params_connected_t *conn = &queue->evt.params.connected;

	if(nRF8001->onConnect)
		nRF8001->onConnect(nRF8001);

	mp_printk(
		"nRF8001(%p) New connection from %02x%02x%02x%02x%02x%02x type=%d interval=%d latency=%d timeout=%d clock=%x ",
		nRF8001,
		conn->dev_addr[0], conn->dev_addr[1], conn->dev_addr[2],
		conn->dev_addr[3], conn->dev_addr[4], conn->dev_addr[5],
		conn->dev_addr_type,
		conn->conn_rf_interval,
		conn->conn_slave_rf_latency,
		conn->conn_rf_timeout
	);
}

static void _mp_drv_nRF8001_evt_disconnected(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	aci_evt_params_disconnected_t *disconn = &queue->evt.params.disconnected;
	int a;

	/* remove flag */
	nRF8001->intSrc &= ~(MP_NRF8001_INTSRC_BOND);
	nRF8001->intSrc &= ~(MP_NRF8001_INTSRC_CONNECT);
	nRF8001->intSrc &= ~(MP_NRF8001_INTSRC_BCAST);

	/* reset pipe handler */
	for(a=0; a<64; a++)
		nRF8001->pipeRxHandler[a] = _mp_drv_nRF8001_evt_pipe_null;

	/* reset pipe status */
	memset(&nRF8001->pipeStatus, 0, sizeof(nRF8001->pipeStatus));

	if(nRF8001->onDisconnect)
		nRF8001->onDisconnect(nRF8001);

	mp_printk(
		"nRF8001(%p) Disconnection ACI status %x BTLE %x",
		nRF8001,
		disconn->aci_status, disconn->btle_status
	);

}

static void _mp_drv_nRF8001_evt_bondStatus(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("_mp_drv_nRF8001_evt_bondStatus");
}

static void _mp_drv_nRF8001_evt_pipeStatus(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	aci_evt_params_pipe_status_t *pipe = &queue->evt.params.pipe_status;

	/* just copy the pipe */
	memcpy(&nRF8001->pipeStatus, pipe, sizeof(*pipe));

	mp_printk("nRF8001(%p) Pipe map changed availability %x %x %x %x %x %x %x %x",
		nRF8001,
		pipe->pipes_open_bitmap[0],pipe->pipes_open_bitmap[1],
		pipe->pipes_open_bitmap[2],pipe->pipes_open_bitmap[3],
		pipe->pipes_open_bitmap[4],pipe->pipes_open_bitmap[5],
		pipe->pipes_open_bitmap[6],pipe->pipes_open_bitmap[7]
	);
}

static void _mp_drv_nRF8001_evt_timing(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("_mp_drv_nRF8001_evt_timing");
}

static void _mp_drv_nRF8001_evt_dataCredits(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	nRF8001->dataCredits += queue->evt.params.data_credit.credit;
}

static void _mp_drv_nRF8001_evt_dataAck(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("_mp_drv_nRF8001_evt_dataAck");
}

static void _mp_drv_nRF8001_evt_dataReceived(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	aci_evt_params_data_received_t *rx = &queue->evt.params.data_received;

	if(rx->rx_data.pipe_number >=64) {
		mp_printk("nRF8001(%p) Strange receiving too large pipe number");
		return;
	}

	/* Execute end user callback */
	nRF8001->pipeRxHandler[rx->rx_data.pipe_number](nRF8001, queue);
}

static void _mp_drv_nRF8001_evt_pipeError(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	aci_evt_params_pipe_error_t *error = &queue->evt.params.pipe_error;
	mp_printk("nRF8001(%p) Pipe #%d error code 0x%x with %d credit", nRF8001, error->pipe_number, error->error_code, nRF8001->dataCredits);
	nRF8001->dataCredits = 1;
}

static void _mp_drv_nRF8001_evt_displayPassKey(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("_mp_drv_nRF8001_evt_displayPassKey");
}

static void _mp_drv_nRF8001_evt_keyRequest(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	mp_printk("_mp_drv_nRF8001_evt_keyRequest");
}

void mp_drv_nRF8001_evt_init(mp_drv_nRF8001_t *nRF8001) {
	int a;

	/* nullize evt */
	for(a=0; a<256; a++)
		nRF8001->evts[a] = _mp_drv_nRF8001_evt_null;

	nRF8001->evts[ACI_EVT_DEVICE_STARTED] = _mp_drv_nRF8001_evt_deviceStarted;
	nRF8001->evts[ACI_EVT_ECHO] = _mp_drv_nRF8001_evt_echo;
	nRF8001->evts[ACI_EVT_HW_ERROR] = _mp_drv_nRF8001_evt_hwError;
	nRF8001->evts[ACI_EVT_CMD_RSP] = _mp_drv_nRF8001_evt_cmdResponse;
	nRF8001->evts[ACI_EVT_CONNECTED] = _mp_drv_nRF8001_evt_connected;
	nRF8001->evts[ACI_EVT_DISCONNECTED] = _mp_drv_nRF8001_evt_disconnected;
	nRF8001->evts[ACI_EVT_BOND_STATUS] = _mp_drv_nRF8001_evt_bondStatus;
	nRF8001->evts[ACI_EVT_PIPE_STATUS] = _mp_drv_nRF8001_evt_pipeStatus;
	nRF8001->evts[ACI_EVT_TIMING] = _mp_drv_nRF8001_evt_timing;
	nRF8001->evts[ACI_EVT_DATA_CREDIT] = _mp_drv_nRF8001_evt_dataCredits;
	nRF8001->evts[ACI_EVT_DATA_ACK] = _mp_drv_nRF8001_evt_dataAck;
	nRF8001->evts[ACI_EVT_DATA_RECEIVED] = _mp_drv_nRF8001_evt_dataReceived;
	nRF8001->evts[ACI_EVT_PIPE_ERROR] = _mp_drv_nRF8001_evt_pipeError;
	nRF8001->evts[ACI_EVT_DISPLAY_PASSKEY] = _mp_drv_nRF8001_evt_displayPassKey;
	nRF8001->evts[ACI_EVT_KEY_REQUEST] = _mp_drv_nRF8001_evt_keyRequest;

	/* nullize pipe RX */
	for(a=0; a<64; a++)
		nRF8001->pipeRxHandler[a] = _mp_drv_nRF8001_evt_pipe_null;
}

#endif
