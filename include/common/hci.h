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

		typedef struct mp_hci_s mp_hci_t;

		struct mp_hci_s {
			mp_kernel_t *kernel;

			mp_uart_t *uart;

			mp_circular_t txCir;
			mp_circular_t rxCir;

			char opened;
		};

		mp_ret_t mp_hci_initUART(mp_kernel_t *kernel, mp_hci_t *hci, mp_uart_t *uart, char *who);
		mp_ret_t mp_hci_fini(mp_hci_t *hci);

	#endif

#endif
