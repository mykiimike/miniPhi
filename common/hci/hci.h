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

		/* while a command waits for its answer, the task looks at the clock every MP_HCI_POLL ms */
		#ifndef MP_HCI_POLL
			#define MP_HCI_POLL 100
		#endif

		/* a command without answer after this resets the controller */
		#ifndef MP_HCI_CMD_TIMEOUT
			#define MP_HCI_CMD_TIMEOUT 2000
		#endif

		/* connections tracked for ACL credits and reassembly */
		#ifndef MP_HCI_CONNECTIONS
			#define MP_HCI_CONNECTIONS 2
		#endif

		/* longest L2CAP frame sent or reassembled, header included */
		#ifndef MP_HCI_ACL_MAX
			#define MP_HCI_ACL_MAX 255
		#endif

		/* command parameters and event parameters have a one byte length */
		#define MP_HCI_PAYLOAD_MAX 255

		/* opcode, length, parameters */
		#define MP_HCI_CMD_BUFFER_SIZE (3+MP_HCI_PAYLOAD_MAX)

		/* type byte, the longest header (ACL, ISO), payload */
		#define MP_HCI_PACKET_MAX (1+4+MP_HCI_PAYLOAD_MAX)

		/* the smallest ACL buffer a controller may report (LE) */
		#define MP_HCI_ACL_MTU_MIN 27

		/* an L2CAP frame cut at the smallest buffer, every fragment with its 5 header bytes */
		#define MP_HCI_ACL_TX_MAX \
			(MP_HCI_ACL_MAX + 5*((MP_HCI_ACL_MAX + MP_HCI_ACL_MTU_MIN - 1) / MP_HCI_ACL_MTU_MIN))

		#define MP_HCI_CMD_CREATE_OPCODE(ogf, ocf) ((uint16_t)(((ogf) << 10) | ((ocf) & 0x03ff)))

		/* status given to a command callback when no answer came */
		#define MP_HCI_STATUS_TIMEOUT (-1)

		/* status given to a command callback when the controller was reset meanwhile */
		#define MP_HCI_STATUS_RESET (-2)

		typedef enum mp_hci_msg_type_s mp_hci_msg_type_t;
		typedef struct mp_hci_read_state_s mp_hci_read_state_t;
		typedef struct mp_hci_cmd_s mp_hci_cmd_t;
		typedef struct mp_hci_conn_s mp_hci_conn_t;
		typedef struct mp_hci_s mp_hci_t;

		enum {
			MP_HCI_STATE_OPENED = 0x01,
			/* the reset sequence is done, commands and ACL are accepted */
			MP_HCI_STATE_READY = 0x02,
		};

		/* UART transport (H4) packet indicators */
		enum mp_hci_msg_type_s {
			MP_HCI_MSG_UNKNOWN = 0x00,
			MP_HCI_MSG_COMMAND = 0x01,
			MP_HCI_MSG_ACL = 0x02,
			MP_HCI_MSG_SCO = 0x03,
			MP_HCI_MSG_EVENT = 0x04,
			MP_HCI_MSG_ISO = 0x05,
		};

		enum {
			MP_HCI_EVT_DISCONNECTION_COMPLETE = 0x05,
			MP_HCI_EVT_ENCRYPTION_CHANGE = 0x08,
			MP_HCI_EVT_COMMAND_COMPLETE = 0x0e,
			MP_HCI_EVT_COMMAND_STATUS = 0x0f,
			MP_HCI_EVT_HARDWARE_ERROR = 0x10,
			MP_HCI_EVT_NUMBER_OF_COMPLETED_PACKETS = 0x13,
			MP_HCI_EVT_DATA_BUFFER_OVERFLOW = 0x1a,
			MP_HCI_EVT_LE_META = 0x3e,
		};

		/**
		 * A packet from the controller, without its type byte: the header
		 * then the payload. Events arrive here after the module read them.
		 * ACL data goes to onAcl instead.
		 */
		typedef void (*mp_hci_on_packet_t)(mp_hci_t *hci, mp_hci_msg_type_t type, uint8_t *packet, int size);

		/** A whole L2CAP frame, basic header included */
		typedef void (*mp_hci_on_acl_t)(mp_hci_t *hci, uint16_t handle, uint8_t *frame, int size);

		/** The reset sequence is done, after the open and after every recovery */
		typedef void (*mp_hci_on_ready_t)(mp_hci_t *hci);

		/**
		 * The answer to a command
		 *
		 * status is the controller status (0 is success),
		 * MP_HCI_STATUS_TIMEOUT or MP_HCI_STATUS_RESET. ret holds the
		 * return parameters of Command Complete after the status byte,
		 * none for Command Status.
		 */
		typedef void (*mp_hci_on_complete_t)(mp_hci_t *hci, uint16_t opcode, int status, uint8_t *ret, int size, void *user);

		struct mp_hci_cmd_s {
			uint16_t opcode;
			const char *format;
		};

		struct mp_hci_read_state_s {
			mp_hci_msg_type_t msgType;

			mp_bool_t haveSize;

			/* bytes received and expected after the type byte */
			int have;
			int need;

			uint8_t packet[MP_HCI_PACKET_MAX-1];
		};

		struct mp_hci_conn_s {
			mp_bool_t used;
			uint16_t handle;

			/* ACL packets sent and not completed yet */
			int inFlight;

			/* frame being reassembled, need is 0 when none */
			int have;
			int need;
			uint8_t frame[MP_HCI_ACL_MAX];
		};

		struct mp_hci_s {
			mp_kernel_t *kernel;

			mp_uart_t *uart;

			mp_circular_t txCir;
			mp_circular_t rxCir;

			mp_task_t *task;

			mp_hci_read_state_t readState;

			/* set by the owner after mp_hci_initUART() */
			mp_hci_on_packet_t onPacket;
			mp_hci_on_acl_t onAcl;
			mp_hci_on_ready_t onReady;
			void *user;

			/* commands the controller accepts now */
			int cmdCredits;

			/* the command waiting for its answer, opcode 0 when none */
			uint16_t cmdOpcode;
			unsigned long cmdSent;
			mp_hci_on_complete_t cmdOnComplete;
			void *cmdUser;

			/* reset sequence step */
			int step;
			mp_bool_t resetPending;

			/* ACL buffers of the controller */
			int aclMtu;
			int aclTotal;
			int aclCredits;

			mp_hci_conn_t conns[MP_HCI_CONNECTIONS];

			/* packets or frames too long, skipped */
			int dropped;

			/* controller resets after a desync, a hardware error or a timeout */
			int recoveries;

			uint8_t state;
		};

		extern const mp_hci_cmd_t mp_hci_cmd_reset;
		extern const mp_hci_cmd_t mp_hci_cmd_set_event_mask;
		extern const mp_hci_cmd_t mp_hci_cmd_host_buffer_size;
		extern const mp_hci_cmd_t mp_hci_cmd_read_local_version;
		extern const mp_hci_cmd_t mp_hci_cmd_read_buffer_size;
		extern const mp_hci_cmd_t mp_hci_cmd_read_bd_addr;
		extern const mp_hci_cmd_t mp_hci_cmd_disconnect;
		extern const mp_hci_cmd_t mp_hci_cmd_le_set_event_mask;
		extern const mp_hci_cmd_t mp_hci_cmd_le_read_buffer_size;

		mp_ret_t mp_hci_initUART(mp_kernel_t *kernel, mp_hci_t *hci, mp_uart_t *uart, char *who);
		mp_ret_t mp_hci_fini(mp_hci_t *hci);
		mp_ret_t mp_hci_reset(mp_hci_t *hci);
		mp_ret_t mp_hci_uart_setup(mp_hci_t *hci, mp_options_t *options);

		mp_ret_t mp_hci_send_raw(mp_hci_t *hci, uint8_t *input, int size);
		mp_ret_t mp_hci_send_acl(mp_hci_t *hci, uint16_t handle, uint8_t *frame, int size);
		mp_ret_t mp_hci_send_sco(mp_hci_t *hci, uint16_t handle, uint8_t *input, int size);

		uint16_t mp_hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const mp_hci_cmd_t *cmd, va_list argptr);
		uint16_t mp_hci_create_cmd(uint8_t *hci_cmd_buffer, const mp_hci_cmd_t *cmd, ...);

		mp_ret_t mp_hci_send_cmd(mp_hci_t *hci, const mp_hci_cmd_t *cmd, mp_hci_on_complete_t onComplete, void *user, ...);

		/* for hci_cmds.c */
		mp_ret_t _mp_hci_issue(mp_hci_t *hci, const mp_hci_cmd_t *cmd, mp_hci_on_complete_t onComplete, void *user, va_list argptr);

	#endif

#endif
