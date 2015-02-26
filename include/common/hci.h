/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
 * Copyright (C) 2014  VERMAN                                              *
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

#ifndef _HAVE_MP_COMMON_HCI_H
	#define _HAVE_MP_COMMON_HCI_H

	#ifdef SUPPORT_COMMON_HCI

		#define MP_HCI_CMD_BUFFER_SIZE	260
		#define MP_HCI_CMD_CREATE_OPCODE(ocf, ogf, out_arr) (((ocf) << 10) | (ogf))

		typedef struct mp_hci_cmd_s mp_hci_cmd_t;
		typedef struct mp_hci_s mp_hci_t;

		enum {
			MP_HCI_STATE_OPENED = 0x01,
			MP_HCI_STATE_CONNECTED = 0x02,
		};

		struct mp_hci_cmd_s {
		    uint16_t    opcode;
		    const char *format;
		};

		struct mp_hci_s {
			mp_kernel_t *kernel;

			mp_uart_t *uart;

			mp_circular_t txCir;
			mp_circular_t rxCir;

			uint8_t addr[6];

			uint8_t state;
		};

		mp_ret_t mp_hci_initUART(mp_kernel_t *kernel, mp_hci_t *hci, mp_uart_t *uart, char *who);
		mp_ret_t mp_hci_fini(mp_hci_t *hci);

		void mp_hci_connect(mp_hci_t *hci, uint8_t *addr);
		void mp_hci_send_raw(mp_hci_t *hci, uint8_t *input, int size);

		uint16_t mp_hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const mp_hci_cmd_t *cmd, va_list argptr);
		uint16_t mp_hci_create_cmd(uint8_t *hci_cmd_buffer, mp_hci_cmd_t *cmd, ...);

		void mp_hci_send_cmd(mp_hci_t *hci, mp_hci_cmd_t *cmd, ...);

	#endif

#endif
