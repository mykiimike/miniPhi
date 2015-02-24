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

static void mp_hci_UART_rxInt(mp_uart_t *uart);
static void mp_hci_UART_txInt(mp_uart_t *uart);

static void mp_hci_UART_rxIntDisable(mp_circular_t *cir);
static void mp_hci_UART_rxIntEnable(mp_circular_t *cir);
static void mp_hci_UART_txIntDisable(mp_circular_t *cir);
static void mp_hci_UART_txIntEnable(mp_circular_t *cir);

mp_ret_t mp_hci_initUART(mp_kernel_t *kernel, mp_hci_t *hci, mp_uart_t *uart, char *who) {
	mp_ret_t ret;

	memset(hci, 0, sizeof(*hci));

	hci->kernel = kernel;
	hci->uart = uart;

	/* Circular context for TX */
	ret = mp_circular_init(
		kernel, &hci->txCir,
		mp_hci_UART_txIntEnable, mp_hci_UART_txIntDisable
	);
	if(!ret) {
		mp_printk("HCI UART: Can not create TX circular");
		return(FALSE);
	}

	/* Circular context for RX */
	ret = mp_circular_init(
		kernel, &hci->rxCir,
		mp_hci_UART_rxIntEnable, mp_hci_UART_rxIntDisable
	);
	if(!ret) {
		mp_printk("HCI UART: Can not create RX circular");
		return(FALSE);
	}

	hci->txCir.user = hci;
	hci->rxCir.user = hci;

	/* setup interruption vectors */
	hci->uart->user = hci;
	hci->uart->onRead = mp_hci_UART_rxInt;
	hci->uart->onWrite = mp_hci_UART_txInt;

	mp_printk("Initialize HCI over UART: %s", who);

	hci->opened = TRUE;
	return(TRUE);
}

mp_ret_t mp_hci_fini(mp_hci_t *hci) {
	hci->uart->onRead = NULL;
	hci->uart->onWrite = NULL;
	hci->uart->user = NULL;

	mp_circular_fini(&hci->rxCir);
	mp_circular_fini(&hci->txCir);

	return(TRUE);
}


/* UART predefined interfacing */
static void mp_hci_UART_rxInt(mp_uart_t *uart) {
	unsigned char chr;

	mp_hci_t *hci = uart->user;

	/* read register */
	chr = mp_uart_rx(uart);

	/* run circular interrupt service */
	mp_circular_rxInterrupt(&hci->rxCir, chr);
}

static void mp_hci_UART_txInt(mp_uart_t *uart) {
	mp_hci_t *hci = uart->user;
	unsigned char toSend;
	mp_bool_t done;

	/* run circular interrupt service */
	toSend = mp_circular_txInterrupt(&hci->txCir, &done);
	if(!done)
		mp_uart_tx(uart, toSend);

	/* done could be used to manage CTS */

}


static void mp_hci_UART_rxIntDisable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_disable_rx_int(hci->uart);
	return;
}

static void mp_hci_UART_rxIntEnable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_enable_rx_int(hci->uart);
	return;
}


static void mp_hci_UART_txIntDisable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_disable_tx_int(hci->uart);
	return;
}

static void mp_hci_UART_txIntEnable(mp_circular_t *cir) {
	mp_hci_t *hci = cir->user;
	mp_uart_enable_tx_int(hci->uart);

}
