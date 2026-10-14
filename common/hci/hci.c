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

#include <mp.h>

#ifdef SUPPORT_COMMON_HCI

/**
@defgroup mpCommonHCI Bluetooth HCI over UART

@ingroup mpCommon

@brief Host side of the H4 transport

mp_hci_initUART() resets the controller, reads its ACL buffers, tells it
the host buffer size, then calls onReady. Until then commands and ACL
data are refused.

One command waits for its answer at a time, within the credits the
controller grants. ACL frames are cut to the controller buffer and sent
within its credits, which come back with Number Of Completed Packets.
Received ACL fragments are put back together into L2CAP frames, one per
connection.

H4 has no framing to resync on: a byte that is no packet indicator, a
Hardware Error event or a command without answer resets the controller.
The connections are gone after that, onReady tells the owner.

Give the UART rts and cts pins: RTS rises when the received bytes come
within MP_HCI_RX_SLACK of the queue limit, and falls again once the task
emptied half of it. Without them the line has to be slow enough for the
task, or the controller configured without flow control.

@{
*/

enum {
	_MP_HCI_STEP_RESET = 1,
	_MP_HCI_STEP_BUFFER,
	_MP_HCI_STEP_LE_BUFFER,
	_MP_HCI_STEP_HOST_BUFFER,
	_MP_HCI_STEP_DONE,
};

static void mp_hci_UART_rxInt(mp_uart_t *uart);
static void mp_hci_UART_txInt(mp_uart_t *uart);

static void mp_hci_UART_rxIntDisable(mp_circular_t *cir);
static void mp_hci_UART_rxIntEnable(mp_circular_t *cir);
static void mp_hci_UART_txIntDisable(mp_circular_t *cir);
static void mp_hci_UART_txIntEnable(mp_circular_t *cir);

static MP_TASK(_mp_hci_reader);
static void _mp_hci_step(mp_hci_t *hci, uint16_t opcode, int status, uint8_t *ret, int size, void *user);

static unsigned long _mp_hci_ticks(unsigned long ms) {
	unsigned long hz = mp_clock_tick_hz();
	unsigned long ticks;

	if(hz == 0 || ms == 0)
		return(1);

	ticks = (ms / 1000) * hz + ((ms % 1000) * hz) / 1000;
	return(ticks ? ticks : 1);
}

static mp_ret_t _mp_hci_step_cmd(mp_hci_t *hci, const mp_hci_cmd_t *cmd, ...) {
	va_list argptr;
	mp_ret_t ret;

	va_start(argptr, cmd);
	ret = _mp_hci_issue(hci, cmd, _mp_hci_step, NULL, argptr);
	va_end(argptr);

	return(ret);
}

static mp_hci_conn_t *_mp_hci_conn_find(mp_hci_t *hci, uint16_t handle) {
	int a;

	for(a=0; a<MP_HCI_CONNECTIONS; a++) {
		if(hci->conns[a].used == YES && hci->conns[a].handle == handle)
			return(&hci->conns[a]);
	}
	return(NULL);
}

static mp_hci_conn_t *_mp_hci_conn_get(mp_hci_t *hci, uint16_t handle) {
	mp_hci_conn_t *conn;
	int a;

	conn = _mp_hci_conn_find(hci, handle);
	if(conn)
		return(conn);

	for(a=0; a<MP_HCI_CONNECTIONS; a++) {
		conn = &hci->conns[a];
		if(conn->used == NO) {
			conn->used = YES;
			conn->handle = handle;
			conn->inFlight = 0;
			conn->have = 0;
			conn->need = 0;
			return(conn);
		}
	}
	return(NULL);
}

static void _mp_hci_acl_credit(mp_hci_t *hci, int count) {
	hci->aclCredits += count;
	if(hci->aclCredits > hci->aclTotal)
		hci->aclCredits = hci->aclTotal;
}

/* forget the controller state, drop what came in, and send a Reset */
static void _mp_hci_restart(mp_hci_t *hci, int reason) {
	mp_hci_on_complete_t onComplete = hci->cmdOnComplete;
	uint16_t opcode = hci->cmdOpcode;
	void *user = hci->cmdUser;
	mp_circular_buffer_t *buffer;

	hci->state &= ~MP_HCI_STATE_READY;
	hci->step = _MP_HCI_STEP_RESET;
	hci->resetPending = NO;

	hci->cmdOpcode = 0;
	hci->cmdOnComplete = NULL;
	hci->cmdUser = NULL;
	hci->cmdCredits = 1;

	hci->aclMtu = 0;
	hci->aclTotal = 0;
	hci->aclCredits = 0;
	memset(hci->conns, 0, sizeof(hci->conns));

	hci->readState.msgType = MP_HCI_MSG_UNKNOWN;
	while((buffer = mp_circular_read(&hci->rxCir)) != NULL)
		mp_mem_free(hci->kernel, buffer);

	if(opcode != 0 && onComplete && onComplete != _mp_hci_step)
		onComplete(hci, opcode, reason, NULL, 0, user);

	if(_mp_hci_step_cmd(hci, &mp_hci_cmd_reset) == FALSE)
		hci->resetPending = YES;
}

/* the reset sequence, one command after the other */
static void _mp_hci_step(mp_hci_t *hci, uint16_t opcode, int status, uint8_t *ret, int size, void *user) {
	int len;
	int total;

	(void)opcode;
	(void)user;

	/* a timeout or a reset already started over */
	if(status < 0)
		return;

	switch(hci->step) {
		case _MP_HCI_STEP_RESET:
			if(status != 0) {
				hci->resetPending = YES;
				return;
			}
			hci->step = _MP_HCI_STEP_BUFFER;
			if(_mp_hci_step_cmd(hci, &mp_hci_cmd_read_buffer_size) == FALSE)
				hci->resetPending = YES;
			return;

		case _MP_HCI_STEP_BUFFER:
			/* ACL length (2), SCO length (1), ACL packets (2), SCO packets (2) */
			if(status == 0 && size >= 5) {
				hci->aclMtu = ret[0] | (ret[1] << 8);
				hci->aclTotal = ret[3] | (ret[4] << 8);
			}
			hci->step = _MP_HCI_STEP_LE_BUFFER;
			if(_mp_hci_step_cmd(hci, &mp_hci_cmd_le_read_buffer_size) == FALSE)
				hci->resetPending = YES;
			return;

		case _MP_HCI_STEP_LE_BUFFER:
			/* LE ACL length (2), LE ACL packets (1); zero means the BR/EDR buffers are shared */
			if(status == 0 && size >= 3) {
				len = ret[0] | (ret[1] << 8);
				total = ret[2];
				if(len > 0 && total > 0) {
					hci->aclMtu = len;
					hci->aclTotal = total;
				}
			}
			hci->step = _MP_HCI_STEP_HOST_BUFFER;
			if(_mp_hci_step_cmd(hci, &mp_hci_cmd_host_buffer_size,
					MP_HCI_PAYLOAD_MAX, MP_HCI_PAYLOAD_MAX, MP_HCI_CONNECTIONS, 1) == FALSE)
				hci->resetPending = YES;
			return;

		case _MP_HCI_STEP_HOST_BUFFER:
			/* refused, the controller still cuts its packets to the parser */
			hci->step = _MP_HCI_STEP_DONE;
			hci->aclCredits = hci->aclTotal;
			hci->state |= MP_HCI_STATE_READY;
			if(hci->onReady)
				hci->onReady(hci);
			return;

		default:
			return;
	}
}

static void _mp_hci_complete(mp_hci_t *hci, uint16_t opcode, int status, uint8_t *ret, int size) {
	mp_hci_on_complete_t onComplete = hci->cmdOnComplete;
	void *user = hci->cmdUser;

	if(opcode == 0 || opcode != hci->cmdOpcode)
		return;

	hci->cmdOpcode = 0;
	hci->cmdOnComplete = NULL;
	hci->cmdUser = NULL;

	if(onComplete)
		onComplete(hci, opcode, status, ret, size, user);
}

static void _mp_hci_event(mp_hci_t *hci, uint8_t *packet, int size) {
	uint8_t *params = packet+2;
	int plen = packet[1];
	mp_hci_conn_t *conn;
	uint16_t handle;
	int count;
	int a;

	switch(packet[0]) {
		case MP_HCI_EVT_COMMAND_COMPLETE:
			/* credits (1), opcode (2), return parameters starting with the status */
			if(plen < 3)
				break;
			hci->cmdCredits = params[0];
			_mp_hci_complete(hci, params[1] | (params[2] << 8),
				plen >= 4 ? params[3] : 0,
				params+4, plen >= 4 ? plen-4 : 0);
			break;

		case MP_HCI_EVT_COMMAND_STATUS:
			/* status (1), credits (1), opcode (2) */
			if(plen < 4)
				break;
			hci->cmdCredits = params[1];
			_mp_hci_complete(hci, params[2] | (params[3] << 8), params[0], NULL, 0);
			break;

		case MP_HCI_EVT_NUMBER_OF_COMPLETED_PACKETS:
			/* handles (1), then a handle (2) and a count (2) for each */
			if(plen < 1 || plen < 1 + 4*params[0])
				break;
			for(a=0; a<params[0]; a++) {
				handle = (params[1+4*a] | (params[2+4*a] << 8)) & 0x0fff;
				count = params[3+4*a] | (params[4+4*a] << 8);
				conn = _mp_hci_conn_find(hci, handle);
				if(conn)
					conn->inFlight = conn->inFlight > count ? conn->inFlight - count : 0;
				_mp_hci_acl_credit(hci, count);
			}
			break;

		case MP_HCI_EVT_DISCONNECTION_COMPLETE:
			/* status (1), handle (2), reason (1): the packets in flight are flushed */
			if(plen < 4 || params[0] != 0)
				break;
			handle = (params[1] | (params[2] << 8)) & 0x0fff;
			conn = _mp_hci_conn_find(hci, handle);
			if(conn) {
				_mp_hci_acl_credit(hci, conn->inFlight);
				conn->used = NO;
			}
			break;

		case MP_HCI_EVT_HARDWARE_ERROR:
			hci->resetPending = YES;
			break;

		default:
			break;
	}

	if(hci->onPacket)
		hci->onPacket(hci, MP_HCI_MSG_EVENT, packet, size);
}

static void _mp_hci_acl(mp_hci_t *hci, uint8_t *packet, int size) {
	mp_hci_conn_t *conn;
	uint16_t handle;
	uint8_t *data;
	int boundary;
	int len;
	int total;

	if(size < 4)
		return;
	handle = (packet[0] | (packet[1] << 8)) & 0x0fff;
	boundary = (packet[1] >> 4) & 0x03;
	len = size-4;
	data = packet+4;

	conn = _mp_hci_conn_get(hci, handle);
	if(!conn) {
		hci->dropped++;
		return;
	}

	if(boundary == 0x01) {
		/* continuing fragment */
		if(conn->need == 0)
			return;
		if(conn->have + len > conn->need) {
			hci->dropped++;
			conn->need = 0;
			return;
		}
		memcpy(conn->frame + conn->have, data, len);
		conn->have += len;
	}
	else {
		/* first fragment, the L2CAP basic header gives the frame length */
		if(conn->need != 0)
			hci->dropped++;
		conn->need = 0;
		if(len < 4) {
			hci->dropped++;
			return;
		}
		total = (data[0] | (data[1] << 8)) + 4;
		if(total > MP_HCI_ACL_MAX || len > total) {
			hci->dropped++;
			return;
		}
		memcpy(conn->frame, data, len);
		conn->have = len;
		conn->need = total;
	}

	if(conn->need != 0 && conn->have == conn->need) {
		conn->need = 0;
		if(hci->onAcl)
			hci->onAcl(hci, handle, conn->frame, conn->have);
	}
}

/* bytes of header after the type byte, the last ones hold the payload length */
static int _mp_hci_header_size(int type) {
	switch(type) {
		case MP_HCI_MSG_ACL:
		case MP_HCI_MSG_ISO:
			return(4);
		case MP_HCI_MSG_SCO:
			return(3);
		case MP_HCI_MSG_EVENT:
			return(2);
		default:
			return(0);
	}
}

static int _mp_hci_payload_size(mp_hci_read_state_t *rs) {
	switch(rs->msgType) {
		case MP_HCI_MSG_ACL:
			return(rs->packet[2] | (rs->packet[3] << 8));
		case MP_HCI_MSG_ISO:
			return((rs->packet[2] | (rs->packet[3] << 8)) & 0x3fff);
		case MP_HCI_MSG_SCO:
			return(rs->packet[2]);
		case MP_HCI_MSG_EVENT:
			return(rs->packet[1]);
		default:
			return(0);
	}
}

static void _mp_hci_dispatch(mp_hci_t *hci, mp_hci_msg_type_t type, uint8_t *packet, int size) {
	switch(type) {
		case MP_HCI_MSG_EVENT:
			_mp_hci_event(hci, packet, size);
			break;
		case MP_HCI_MSG_ACL:
			_mp_hci_acl(hci, packet, size);
			break;
		default:
			if(hci->onPacket)
				hci->onPacket(hci, type, packet, size);
			break;
	}
}

static void _mp_hci_parse(mp_hci_t *hci, uint8_t c) {
	mp_hci_read_state_t *rs = &hci->readState;
	mp_hci_msg_type_t type;

	if(rs->msgType == MP_HCI_MSG_UNKNOWN) {
		rs->need = _mp_hci_header_size(c);
		if(rs->need == 0) {
			/* lost in the stream; during the reset sequence the leftovers of the old stream are skipped */
			if(hci->state & MP_HCI_STATE_READY)
				hci->resetPending = YES;
			return;
		}
		rs->msgType = c;
		rs->have = 0;
		rs->haveSize = NO;
		return;
	}

	if(rs->have < (int)sizeof(rs->packet))
		rs->packet[rs->have] = c;
	rs->have++;

	if(rs->haveSize == NO && rs->have == rs->need) {
		rs->need += _mp_hci_payload_size(rs);
		rs->haveSize = YES;
	}

	if(rs->haveSize == YES && rs->have == rs->need) {
		type = rs->msgType;
		rs->msgType = MP_HCI_MSG_UNKNOWN;
		if(rs->need > (int)sizeof(rs->packet))
			hci->dropped++;
		else
			_mp_hci_dispatch(hci, type, rs->packet, rs->need);
	}
}

static MP_TASK(_mp_hci_reader) {
	mp_hci_t *hci = task->user;
	mp_circular_buffer_t *buffer;
	int index;

	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	while(hci->resetPending == NO && (buffer = mp_circular_read(&hci->rxCir)) != NULL) {
		for(index = 0; index < buffer->size && hci->resetPending == NO; index++)
			_mp_hci_parse(hci, buffer->data[index]);
		mp_mem_free(hci->kernel, buffer);
	}

	if(hci->cmdOpcode != 0 && mp_clock_ticks() - hci->cmdSent >= _mp_hci_ticks(MP_HCI_CMD_TIMEOUT)) {
		hci->recoveries++;
		_mp_hci_restart(hci, MP_HCI_STATUS_TIMEOUT);
	}
	else if(hci->resetPending == YES) {
		hci->recoveries++;
		_mp_hci_restart(hci, MP_HCI_STATUS_RESET);
	}

	if(hci->rxHeld == YES && hci->rxCir.totalSize <= hci->rxCir.maxSize / 2) {
		hci->rxHeld = NO;
		mp_uart_enable_rx(hci->uart);
	}

	/* an answer to wait for keeps the clock watched, otherwise only a received byte wakes the task */
	if(hci->cmdOpcode != 0 || hci->resetPending == YES)
		mp_task_delay(task, _mp_hci_ticks(MP_HCI_POLL));
	else {
		mp_task_signal(task, MP_TASK_SIG_SLEEP);
		if(hci->rxCir.totalSize > 0)
			mp_task_wake(task);
	}
}

mp_ret_t mp_hci_initUART(mp_kernel_t *kernel, mp_hci_t *hci, mp_uart_t *uart, char *who) {
	memset(hci, 0, sizeof(*hci));

	hci->kernel = kernel;
	hci->uart = uart;

	mp_circular_init(kernel, &hci->txCir, mp_hci_UART_txIntEnable, mp_hci_UART_txIntDisable);
	mp_circular_init(kernel, &hci->rxCir, mp_hci_UART_rxIntEnable, mp_hci_UART_rxIntDisable);

	/* a whole packet, or a whole fragmented frame, has to fit */
	hci->txCir.maxSize = MP_HCI_ACL_TX_MAX > MP_HCI_PACKET_MAX ? MP_HCI_ACL_TX_MAX : MP_HCI_PACKET_MAX;
	hci->rxCir.maxSize = MP_HCI_PACKET_MAX;

	hci->txCir.user = hci;
	hci->rxCir.user = hci;
	mp_hci_UART_txIntDisable(&hci->txCir);
	mp_hci_UART_rxIntDisable(&hci->rxCir);

	hci->task = mp_task_create(&kernel->tasks, "HCI", _mp_hci_reader, hci, _mp_hci_ticks(MP_HCI_POLL));
	if(!hci->task) {
		mp_printk("HCI UART: no task slot for %s", who);
		return(FALSE);
	}

	/* setup interruption vectors */
	hci->uart->user = hci;
	hci->uart->onRead = mp_hci_UART_rxInt;
	hci->uart->onWrite = mp_hci_UART_txInt;

	mp_circular_start(&hci->rxCir);

	mp_printk("Initialize HCI over UART: %s", who);

	hci->state |= MP_HCI_STATE_OPENED;
	_mp_hci_restart(hci, MP_HCI_STATUS_RESET);
	return(TRUE);
}

mp_ret_t mp_hci_fini(mp_hci_t *hci) {
	if(!(hci->state & MP_HCI_STATE_OPENED))
		return(FALSE);
	hci->state = 0;

	mp_task_destroy(hci->task);
	hci->task = NULL;

	hci->uart->onRead = NULL;
	hci->uart->onWrite = NULL;
	hci->uart->user = NULL;

	mp_circular_fini(&hci->rxCir);
	mp_circular_fini(&hci->txCir);

	if(hci->rxHeld == YES) {
		hci->rxHeld = NO;
		mp_uart_enable_rx(hci->uart);
	}

	return(TRUE);
}

/**
 * @brief Reset the controller and run the reset sequence again
 *
 * A command waiting for its answer gets MP_HCI_STATUS_RESET.
 */
mp_ret_t mp_hci_reset(mp_hci_t *hci) {
	if(!(hci->state & MP_HCI_STATE_OPENED))
		return(FALSE);

	_mp_hci_restart(hci, MP_HCI_STATUS_RESET);
	return(TRUE);
}

/**
 * @brief Change the UART settings, after a vendor command changed the controller rate
 *
 * @param[in] options Options of mp_uart_setup(), baudRate first of all
 * @return FALSE while bytes are still leaving, the caller tries again
 */
mp_ret_t mp_hci_uart_setup(mp_hci_t *hci, mp_options_t *options) {
	mp_ret_t ret;

	if(!(hci->state & MP_HCI_STATE_OPENED))
		return(FALSE);
	if(hci->txCir.first || mp_uart_isBusy(hci->uart))
		return(FALSE);

	ret = mp_uart_setup(hci->uart, options);

	/* the setup turned the interrupts off */
	mp_circular_start(&hci->rxCir);

	return(ret);
}

/**
 * @brief Queue bytes already framed by the caller
 *
 * No credit is counted: the caller owns the flow control of what it sends here.
 *
 * @return FALSE when the module is closed or the bytes can not be queued whole
 */
mp_ret_t mp_hci_send_raw(mp_hci_t *hci, uint8_t *input, int size) {
	if(!(hci->state & MP_HCI_STATE_OPENED))
		return(FALSE);

	return(mp_circular_write(&hci->txCir, input, size));
}

/**
 * @brief Send an L2CAP frame on a connection
 *
 * The frame is cut to the controller ACL buffer, every fragment takes a
 * credit, and all the fragments are queued together.
 *
 * @param[in] handle Connection handle, 12 bits
 * @param[in] frame L2CAP frame, basic header included
 * @param[in] size Up to MP_HCI_ACL_MAX
 * @return FALSE before onReady, without enough credits, with too many
 * connections, or when the fragments can not be queued
 */
mp_ret_t mp_hci_send_acl(mp_hci_t *hci, uint16_t handle, uint8_t *frame, int size) {
	uint8_t packet[MP_HCI_ACL_TX_MAX];
	mp_hci_conn_t *conn;
	mp_bool_t fresh;
	int fragment;
	int count;
	int pos;
	int off;
	int len;

	if(!(hci->state & MP_HCI_STATE_READY))
		return(FALSE);
	if(size <= 0 || size > MP_HCI_ACL_MAX || hci->aclMtu <= 0)
		return(FALSE);

	fragment = hci->aclMtu < MP_HCI_PAYLOAD_MAX ? hci->aclMtu : MP_HCI_PAYLOAD_MAX;
	count = (size + fragment - 1) / fragment;
	if(size + 5*count > (int)sizeof(packet) || count > hci->aclCredits)
		return(FALSE);

	handle &= 0x0fff;
	fresh = _mp_hci_conn_find(hci, handle) ? NO : YES;
	conn = _mp_hci_conn_get(hci, handle);
	if(!conn)
		return(FALSE);

	pos = 0;
	for(off = 0; off < size; off += len) {
		len = size - off > fragment ? fragment : size - off;
		packet[pos++] = MP_HCI_MSG_ACL;
		packet[pos++] = handle & 0xff;
		/* first non-flushable fragment, then continuing fragments */
		packet[pos++] = (handle >> 8) | (off ? 0x10 : 0x00);
		packet[pos++] = len & 0xff;
		packet[pos++] = (len >> 8) & 0xff;
		memcpy(packet+pos, frame+off, len);
		pos += len;
	}

	if(mp_hci_send_raw(hci, packet, pos) == FALSE) {
		if(fresh == YES)
			conn->used = NO;
		return(FALSE);
	}

	hci->aclCredits -= count;
	conn->inFlight += count;

	return(TRUE);
}

/**
 * @brief Queue a SCO packet
 *
 * SCO flow control is off by default on the controllers: no credit is counted.
 */
mp_ret_t mp_hci_send_sco(mp_hci_t *hci, uint16_t handle, uint8_t *input, int size) {
	uint8_t packet[4+MP_HCI_PAYLOAD_MAX];

	if(!(hci->state & MP_HCI_STATE_READY))
		return(FALSE);
	if(size < 0 || size > MP_HCI_PAYLOAD_MAX)
		return(FALSE);

	packet[0] = MP_HCI_MSG_SCO;
	packet[1] = handle & 0xff;
	packet[2] = (handle >> 8) & 0x0f;
	packet[3] = size;
	memcpy(packet+4, input, size);

	return(mp_hci_send_raw(hci, packet, size+4));
}

/* UART predefined interfacing */
static void mp_hci_UART_rxInt(mp_uart_t *uart) {
	unsigned char chr;

	mp_hci_t *hci = uart->user;

	/* read register */
	chr = mp_uart_rx(uart);

	/* run circular interrupt service */
	mp_circular_rxInterrupt(&hci->rxCir, chr);
	if(hci->rxHeld == NO && hci->rxCir.totalSize >= hci->rxCir.maxSize - MP_HCI_RX_SLACK) {
		hci->rxHeld = YES;
		mp_uart_disable_rx(uart);
	}
	if(hci->task)
		mp_task_wake(hci->task);
}

static void mp_hci_UART_txInt(mp_uart_t *uart) {
	mp_hci_t *hci = uart->user;
	unsigned char toSend;
	mp_bool_t done;

	/* run circular interrupt service */
	toSend = mp_circular_txInterrupt(&hci->txCir, &done);
	if(!done)
		mp_uart_tx(uart, toSend);
}


static void mp_hci_UART_rxIntDisable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_disable_rx_int(hci->uart);
}

static void mp_hci_UART_rxIntEnable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_enable_rx_int(hci->uart);
}


static void mp_hci_UART_txIntDisable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_disable_tx_int(hci->uart);
}

static void mp_hci_UART_txIntEnable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_enable_tx_int(hci->uart);
}

/**@}*/

#endif
