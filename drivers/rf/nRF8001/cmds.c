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

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_setup(mp_drv_nRF8001_t *nRF8001) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);
	mp_drv_nRF8001_setup_t *ptr = &nRF8001->setup[nRF8001->setup_idx];

	queue->packet.length = ptr->payload[0];

	memcpy(&queue->packet.payload[0], &ptr->payload[1], queue->packet.length);

	nRF8001->setup_idx++;

	return(queue);
}


mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_get_device_version(mp_drv_nRF8001_t *nRF8001) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	queue->packet.length = 1;
	queue->packet.payload[0] = ACI_CMD_GET_DEVICE_VERSION;

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_get_device_address(mp_drv_nRF8001_t *nRF8001) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	queue->packet.length = 1;
	queue->packet.payload[0] = ACI_CMD_GET_DEVICE_ADDRESS;

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_test(mp_drv_nRF8001_t *nRF8001, aci_test_mode_change_t mode) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	queue->packet.length = sizeof(queue->cmd.params.test)+1;
	queue->packet.payload[0] = ACI_CMD_TEST;

	queue->cmd.params.test.test_mode_change = mode;

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_wakeup(mp_drv_nRF8001_t *nRF8001) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	queue->packet.length = 1;
	queue->packet.payload[0] = ACI_CMD_WAKEUP;

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_echo(mp_drv_nRF8001_t *nRF8001, char *data, int size) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);
	int rsize = 0;

	rsize = size+1 > ACI_ECHO_DATA_MAX_LEN ? ACI_ECHO_DATA_MAX_LEN : size;

	queue->packet.length = rsize+1;
	queue->packet.payload[0] = ACI_CMD_ECHO;

	memcpy(&queue->packet.payload[1], data, rsize);

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_bond(mp_drv_nRF8001_t *nRF8001, uint16_t timeout, uint16_t adv_interval) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	queue->packet.length = sizeof(queue->cmd.params.bond)+1;
	queue->packet.payload[0] = ACI_CMD_BOND;

	queue->cmd.params.bond.timeout.sb.msb = (uint8_t)(timeout >> 8);
	queue->cmd.params.bond.timeout.sb.lsb = (uint8_t)(timeout);

	queue->cmd.params.bond.adv_interval.sb.msb = (uint8_t)(adv_interval >> 8);
	queue->cmd.params.bond.adv_interval.sb.lsb = (uint8_t)(adv_interval);

	mp_printk("queue->cmd.params.bond.adv_interval: %d / %d %d", sizeof(queue->cmd.params.bond.adv_interval), queue->cmd.params.bond.timeout.tt, queue->cmd.params.bond.adv_interval.tt);

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_broadcast(mp_drv_nRF8001_t *nRF8001, uint16_t timeout, uint16_t adv_interval) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	queue->packet.length = sizeof(queue->cmd.params.broadcast)+1;
	queue->packet.payload[0] = ACI_CMD_BROADCAST;

	queue->cmd.params.broadcast.timeout.sb.msb = (uint8_t)(timeout >> 8);
	queue->cmd.params.broadcast.timeout.sb.lsb = (uint8_t)(timeout);

	queue->cmd.params.broadcast.adv_interval.sb.msb = (uint8_t)(adv_interval >> 8);
	queue->cmd.params.broadcast.adv_interval.sb.lsb = (uint8_t)(adv_interval);

	mp_printk("queue->cmd.params.broadcast.adv_interval: %d / %d %d", sizeof(queue->cmd.params.broadcast.adv_interval), queue->cmd.params.broadcast.timeout.tt, queue->cmd.params.broadcast.adv_interval.tt);

	return(queue);
}


mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_connect(mp_drv_nRF8001_t *nRF8001, uint16_t timeout, uint16_t adv_interval) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	queue->packet.length = sizeof(queue->cmd.params.connect)+1;
	queue->packet.payload[0] = ACI_CMD_CONNECT;

	queue->cmd.params.connect.timeout.sb.msb = (uint8_t)(timeout >> 8);
	queue->cmd.params.connect.timeout.sb.lsb = (uint8_t)(timeout);

	queue->cmd.params.connect.adv_interval.sb.msb = (uint8_t)(adv_interval >> 8);
	queue->cmd.params.connect.adv_interval.sb.lsb = (uint8_t)(adv_interval);

	mp_printk("queue->cmd.params.connect.adv_interval: %d / %d %d", sizeof(queue->cmd.params.connect.adv_interval), queue->cmd.params.connect.timeout.tt, queue->cmd.params.connect.adv_interval.tt);

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_set_local_data(mp_drv_nRF8001_t *nRF8001, uint8_t pipe, char *name, int size) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);
	int rsize = size > ACI_PIPE_TX_DATA_MAX_LEN ? ACI_PIPE_TX_DATA_MAX_LEN : size;

	queue->packet.length = rsize+1;
	queue->packet.payload[0] = ACI_CMD_SET_LOCAL_DATA;

	queue->cmd.params.set_local_data.tx_data.pipe_number = pipe;

	memcpy(&queue->cmd.params.set_local_data.tx_data.aci_data[0], name, rsize);

	return(queue);
}



mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_open_adv_pipes(mp_drv_nRF8001_t *nRF8001) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);
	aci_services_pipe_type_mapping_t *pipe = nRF8001->pipe_map;

	int a;

	queue->packet.length = 9;
	queue->packet.payload[0] = ACI_CMD_OPEN_ADV_PIPE;

	memset(&queue->cmd.params.open_adv_pipe.pipes, 0, sizeof(queue->cmd.params.open_adv_pipe.pipes));

	for(a=0; a<nRF8001->pipe_map_size; a++, pipe++) {
		if(pipe->pipe_type == ACI_SET)
			queue->cmd.params.open_adv_pipe.pipes[8-(a/8)] |= 1<<(a%8);
	}

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_open_remote_pipe(mp_drv_nRF8001_t *nRF8001, uint8_t pipe) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	if(pipe>62)
		mp_printk("nRF8001(%p) Invalid pipe number in open remote pipe");

	queue->packet.length = sizeof(queue->cmd.params.open_remote_pipe)+1;
	queue->packet.payload[0] = ACI_CMD_OPEN_REMOTE_PIPE;

	queue->cmd.params.open_remote_pipe.pipe_number = pipe;

	return(queue);
}

mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_cmd_close_remote_pipe(mp_drv_nRF8001_t *nRF8001, uint8_t pipe) {
	mp_drv_nRF8001_aci_queue_t *queue = mp_drv_nRF8001_send_alloc(nRF8001);

	if(pipe>62)
		mp_printk("nRF8001(%p) Invalid pipe number in close remote pipe");

	queue->packet.length = sizeof(queue->cmd.params.open_remote_pipe)+1;
	queue->packet.payload[0] = ACI_CMD_CLOSE_REMOTE_PIPE;

	queue->cmd.params.close_remote_pipe.pipe_number = pipe;

	return(queue);
}


#endif
