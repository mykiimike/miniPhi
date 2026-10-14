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

/* Link Control */
const mp_hci_cmd_t mp_hci_cmd_disconnect = { MP_HCI_CMD_CREATE_OPCODE(0x01, 0x0006), "H1" };

/* Controller and Baseband */
const mp_hci_cmd_t mp_hci_cmd_set_event_mask = { MP_HCI_CMD_CREATE_OPCODE(0x03, 0x0001), "D" };
const mp_hci_cmd_t mp_hci_cmd_reset = { MP_HCI_CMD_CREATE_OPCODE(0x03, 0x0003), "" };
const mp_hci_cmd_t mp_hci_cmd_host_buffer_size = { MP_HCI_CMD_CREATE_OPCODE(0x03, 0x0033), "2122" };

/* Informational */
const mp_hci_cmd_t mp_hci_cmd_read_local_version = { MP_HCI_CMD_CREATE_OPCODE(0x04, 0x0001), "" };
const mp_hci_cmd_t mp_hci_cmd_read_buffer_size = { MP_HCI_CMD_CREATE_OPCODE(0x04, 0x0005), "" };
const mp_hci_cmd_t mp_hci_cmd_read_bd_addr = { MP_HCI_CMD_CREATE_OPCODE(0x04, 0x0009), "" };

/* LE Controller */
const mp_hci_cmd_t mp_hci_cmd_le_set_event_mask = { MP_HCI_CMD_CREATE_OPCODE(0x08, 0x0001), "D" };
const mp_hci_cmd_t mp_hci_cmd_le_read_buffer_size = { MP_HCI_CMD_CREATE_OPCODE(0x08, 0x0002), "" };

/* size of each format letter in the command parameters, 0 when unknown */
static unsigned int _mp_hci_field_size(char letter) {
	switch(letter) {
		case '1': return(1);
		case '2': return(2);
		case 'H': return(2);
		case '3': return(3);
		case '4': return(4);
		case 'B': return(6);
		case 'D': return(8);
		case 'P': return(16);
		case 'A': return(31);
		case 'E': return(240);
		case 'N': return(248);
		default: return(0);
	}
}

/**
 * construct HCI Command based on template
 *
 * Format:
 *   1, 2: one or two byte value, passed as int
 *   3, 4: three or four byte value, passed as uint32_t (cast literals)
 *   H: HCI connection handle, passed as int
 *   B: Bluetooth Baseband Address (BD_ADDR), 6 bytes most significant first
 *   D: 8 byte data block
 *   P: 16 byte Pairing code or link key
 *   A: 31 byte advertising data
 *   E: Extended Inquiry Result, 240 bytes
 *   N: Name up to 248 chars, \0 terminated
 *
 * hci_cmd_buffer holds MP_HCI_CMD_BUFFER_SIZE bytes.
 *
 * @return Bytes written, 0 when the format is unknown or the parameters exceed MP_HCI_PAYLOAD_MAX
 */
uint16_t mp_hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const mp_hci_cmd_t *cmd, va_list argptr) {
	const char *format;
	unsigned int pos;
	unsigned int field;
	uint16_t word;
	uint32_t longword;
	uint8_t *ptr;
	uint16_t len;

	/* check the whole format before reading any argument */
	pos = 3;
	for(format = cmd->format; *format; format++) {
		field = _mp_hci_field_size(*format);
		if(field == 0 || pos + field > MP_HCI_CMD_BUFFER_SIZE)
			return(0);
		pos += field;
	}

	hci_cmd_buffer[0] = cmd->opcode & 0xff;
	hci_cmd_buffer[1] = cmd->opcode >> 8;
	pos = 3;

	for(format = cmd->format; *format; format++) {
		switch(*format) {
			case '1':
			case '2':
			case 'H':
				/* minimal va_arg is int: 2 bytes on 8+16 bit CPUs */
				word = va_arg(argptr, int);
				hci_cmd_buffer[pos++] = word & 0xff;
				if(*format != '1')
					hci_cmd_buffer[pos++] = word >> 8;
				break;
			case '3':
			case '4':
				longword = va_arg(argptr, uint32_t);
				hci_cmd_buffer[pos++] = longword;
				hci_cmd_buffer[pos++] = longword >> 8;
				hci_cmd_buffer[pos++] = longword >> 16;
				if(*format == '4')
					hci_cmd_buffer[pos++] = longword >> 24;
				break;
			case 'B':
				ptr = va_arg(argptr, uint8_t *);
				hci_cmd_buffer[pos++] = ptr[5];
				hci_cmd_buffer[pos++] = ptr[4];
				hci_cmd_buffer[pos++] = ptr[3];
				hci_cmd_buffer[pos++] = ptr[2];
				hci_cmd_buffer[pos++] = ptr[1];
				hci_cmd_buffer[pos++] = ptr[0];
				break;
			case 'N':
				ptr = va_arg(argptr, uint8_t *);
				len = strlen((const char *)ptr);
				if(len > 248)
					len = 248;
				memcpy(&hci_cmd_buffer[pos], ptr, len);
				memset(&hci_cmd_buffer[pos+len], 0, 248-len);
				pos += 248;
				break;
			default:
				/* D, P, A, E: fixed size blocks */
				field = _mp_hci_field_size(*format);
				ptr = va_arg(argptr, uint8_t *);
				memcpy(&hci_cmd_buffer[pos], ptr, field);
				pos += field;
				break;
		}
	}

	hci_cmd_buffer[2] = pos - 3;
	return(pos);
}

/**
 * construct HCI Command based on template
 *
 * mainly calls hci_create_cmd_internal
 */
uint16_t mp_hci_create_cmd(uint8_t *hci_cmd_buffer, const mp_hci_cmd_t *cmd, ...) {
	va_list argptr;
	uint16_t len;

	va_start(argptr, cmd);
	len = mp_hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
	va_end(argptr);
	return(len);
}

/**
 * @brief Build a command and queue it whole, without looking at the reset sequence
 *
 * One command waits for its answer at a time, and only when the
 * controller gave a credit.
 */
mp_ret_t _mp_hci_issue(mp_hci_t *hci, const mp_hci_cmd_t *cmd, mp_hci_on_complete_t onComplete, void *user, va_list argptr) {
	uint8_t packet[1+MP_HCI_CMD_BUFFER_SIZE];
	uint16_t len;

	if(!(hci->state & MP_HCI_STATE_OPENED))
		return(FALSE);
	if(hci->cmdOpcode != 0 || hci->cmdCredits <= 0)
		return(FALSE);

	len = mp_hci_create_cmd_internal(packet+1, cmd, argptr);
	if(len == 0)
		return(FALSE);

	packet[0] = MP_HCI_MSG_COMMAND;
	if(mp_hci_send_raw(hci, packet, len+1) == FALSE)
		return(FALSE);

	hci->cmdCredits--;
	hci->cmdOpcode = cmd->opcode;
	hci->cmdSent = mp_clock_ticks();
	hci->cmdOnComplete = onComplete;
	hci->cmdUser = user;

	/* the task watches the timeout */
	mp_task_wake(hci->task);

	return(TRUE);
}

/**
 * @brief Send a command once the controller is ready
 *
 * @param[in] onComplete Called from the HCI task with the answer, may be NULL
 * @return FALSE while the reset sequence runs, while another command waits
 * for its answer, without credit, or when the command can not be built or queued
 */
mp_ret_t mp_hci_send_cmd(mp_hci_t *hci, const mp_hci_cmd_t *cmd, mp_hci_on_complete_t onComplete, void *user, ...) {
	va_list argptr;
	mp_ret_t ret;

	if(!(hci->state & MP_HCI_STATE_READY))
		return(FALSE);

	va_start(argptr, user);
	ret = _mp_hci_issue(hci, cmd, onComplete, user, argptr);
	va_end(argptr);

	return(ret);
}

#endif
