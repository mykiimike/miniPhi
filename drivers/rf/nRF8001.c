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

static void _mp_drv_nRF8001_onIntRdyn(void *user);

static void _mp_drv_nRF8001_spi_interrupt(mp_spi_t *spi, mp_spi_iv_t iv);

MP_TASK(_mp_drv_nRF8001_ASR);

/**
@defgroup mpDriverNRF8001 N NRF8001

@ingroup mpDriver

@brief nRF8001 - Single-chip Bluetooth® low energy solution

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 11 Nov 2015

@{
*/

/**
 * @brief Initiate nRF8001 context
 *
 * @param[in] kernel Kernel context
 * @param[in] nRF8001 Context
 * @param[in] options Special options :
 *  @li reqn : Request wire
 *  @li rdyn : Ready wire (int)
 *  @li reset : Reset wire
 *  @li gate : SPI Gate (if required)
 *  @li mosi : Master Out Slave In
 *  @li miso : Master In Slave Out
 *  @li clk : Clock port source
 *  @li uRx : DTM UART RX (optionnal)
 *  @li uTx : DTM UART TX (optionnal)
 *  @li uGate : DTM UART Gate (optionnal)
 *  @li uBaud : DTM UART Baud (optionnal - default 19200)
 * @param[in] who Who own the driver session
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_nRF8001_init(mp_kernel_t *kernel, mp_drv_nRF8001_t *nRF8001, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(nRF8001, 0, sizeof(*nRF8001));
	nRF8001->kernel = kernel;

	/* Reset IO */
	value = mp_options_get(options, "reset");
	if(!value) {
		mp_printk("nRF8001: need reset port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	nRF8001->reset = mp_gpio_text_handle(value, "nRF8001 reset");
	if(!nRF8001->reset) {
		mp_printk("nRF8001: need reset port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	mp_gpio_direction(nRF8001->reset, MP_GPIO_OUTPUT);
	mp_gpio_unset(nRF8001->reset);

	/* reqn IO */
	value = mp_options_get(options, "reqn");
	if(!value) {
		mp_printk("nRF8001: need reqn port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	nRF8001->reqn = mp_gpio_text_handle(value, "nRF8001 reqn");
	if(!nRF8001->reqn) {
		mp_printk("nRF8001: need reqn port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	mp_gpio_direction(nRF8001->reqn, MP_GPIO_OUTPUT);
	mp_gpio_unset(nRF8001->reqn);

	/* Rdyn IO */
	value = mp_options_get(options, "rdyn");
	if(!value) {
		mp_printk("nRF8001: rdyn reqn port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	nRF8001->rdyn = mp_gpio_text_handle(value, "nRF8001 rdyn");
	if(!nRF8001->rdyn) {
		mp_printk("nRF8001: need rdyn port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	mp_gpio_direction(nRF8001->rdyn, MP_GPIO_INPUT);

	/* install Rdyn interrupt high > low */
	mp_gpio_interrupt_disable(nRF8001->rdyn);
	ret = mp_gpio_interrupt_set(nRF8001->rdyn, _mp_drv_nRF8001_onIntRdyn, nRF8001, who);
	if(ret == FALSE) {
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	mp_gpio_interrupt_hi2lo(nRF8001->rdyn);


	/* open spi */
	ret = mp_spi_open(kernel, &nRF8001->spi, options, "nRF8001");
	if(ret == FALSE) {
		mp_printk("Error openning SPI interface");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}

	mp_options_t setup[] = {
		{ "frequency", "2000000" },
		//{ "phase", "change" },
		{ "polarity", "low" },
		{ "first", "lsb" },
		{ "role", "master" },
		{ "bit", "8" },
		{ NULL, NULL }
	};
	ret = mp_spi_setup(&nRF8001->spi, setup);
	if(ret == FALSE) {
		mp_printk("nRF8001 error while creating SPI interface");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	nRF8001->spi.user = nRF8001;
	mp_spi_disable_both(&nRF8001->spi);
	mp_spi_setInterruption(&nRF8001->spi, _mp_drv_nRF8001_spi_interrupt);

	nRF8001->init = 1;

	/* create ASR task */
	nRF8001->task = mp_task_create(&kernel->tasks, who, _mp_drv_nRF8001_ASR, nRF8001, 100);
	if(!nRF8001->task) {
		mp_printk("nRF8001(%p) FATAL could not create ASR task", nRF8001);
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}

	nRF8001->init = 2;

	/* initialize pkts */
	mp_list_init(&nRF8001->tx_pkts);
	mp_list_init(&nRF8001->rx_pkts);
	mp_list_init(&nRF8001->pending_tx_pkts);
	mp_list_init(&nRF8001->pending_rx_pkts);

	int a;

	mp_drv_nRF8001_aci_queue_t *queue;
	for(a=0; a<(MP_NRF8001_ACI_QUEUE_SIZE/2)-1; a++) {
		queue = &nRF8001->inline_pkts[a];
		mp_list_add_last(&nRF8001->tx_pkts, &queue->item, queue);
		nRF8001->inline_pkts_alloc++;
	}

	for(a=0; a<(MP_NRF8001_ACI_QUEUE_SIZE/2)-1; a++) {
		queue = &nRF8001->inline_pkts[a+nRF8001->inline_pkts_alloc];
		mp_list_add_last(&nRF8001->rx_pkts, &queue->item, queue);
		nRF8001->inline_pkts_alloc++;
	}

	/* intialize events */
	mp_drv_nRF8001_evt_init(nRF8001);

	/* set task sleeping */
	nRF8001->task->signal = MP_TASK_SIG_SLEEP;

	mp_printk("nRF8001(%p) driver initialization in memory structure size of %d bytes", nRF8001, sizeof(*nRF8001));


	return(TRUE);
}

/**
 * @brief Terminate nRF8001 context
 *
 * @param[in] nRF8001 Context
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_nRF8001_fini(mp_drv_nRF8001_t *nRF8001) {


	if(nRF8001->reset)
		mp_gpio_release(nRF8001->reset);

	if(nRF8001->reqn)
		mp_gpio_release(nRF8001->reqn);

	if(nRF8001->rdyn)
		mp_gpio_release(nRF8001->rdyn);

	if(nRF8001->task)
		mp_task_destroy(nRF8001->task);

	if(nRF8001->init >= 1)
		mp_spi_close(&nRF8001->spi);

	mp_printk("Stopping nRF8001");
	return(TRUE);
}




mp_drv_nRF8001_aci_queue_t *mp_drv_nRF8001_send_alloc(mp_drv_nRF8001_t *nRF8001) {
	mp_drv_nRF8001_aci_queue_t *queue = nRF8001->tx_pkts.first->user;
	if(!queue)
		mp_printk("FATAL NO MORE QUEUE");

	mp_list_remove(&nRF8001->tx_pkts, &queue->item);

	return(queue);
}

mp_bool_t mp_drv_nRF8001_send_queue(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	/* add queue at last pending */

	mp_list_add_last(&nRF8001->pending_tx_pkts, &queue->item, queue);
	nRF8001->task->signal = MP_TASK_SIG_PENDING;

	return(TRUE);
}

mp_ret_t mp_drv_nRF8001_start(mp_drv_nRF8001_t *nRF8001) {

	/* reset the nrf */
	mp_gpio_unset(nRF8001->reset);
	mp_clock_delay(1);
	mp_gpio_set(nRF8001->reset);
	mp_clock_delay(62);

	mp_gpio_interrupt_enable(nRF8001->rdyn);

	return(TRUE);
}

mp_ret_t mp_drv_nRF8001_onReady(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_onReady_t onReady, void *user) {
	nRF8001->onReady = onReady;
	nRF8001->user = user;

	return(TRUE);
}

mp_ret_t mp_drv_nRF8001_go(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_setup_t *setup, int messages, aci_services_pipe_type_mapping_t *pipe, int pipe_size) {
	mp_printk("nRF8001(%p) Receiving %d nRF Go packets", nRF8001, messages);

	nRF8001->setup = setup;
	nRF8001->setup_size = messages;
	nRF8001->setup_idx = 0;

	nRF8001->pipe_map = pipe;
	nRF8001->pipe_map_size = pipe_size;

	return(TRUE);
}

mp_ret_t mp_drv_nRF8001_stop(mp_drv_nRF8001_t *nRF8001) {
	mp_gpio_interrupt_disable(nRF8001->rdyn);

	mp_gpio_unset(nRF8001->reset);
	mp_printk("nRF8001 a lot to do there");
	return(TRUE);
}


/**@}*/

static void _mp_drv_nRF8001_onIntRdyn(void *user) {
	mp_drv_nRF8001_t *nRF8001 = user;

	if(nRF8001->intSrc & MP_NRF8001_INTSRC_RDYN) {

		mp_spi_disable_both(&nRF8001->spi);
		mp_gpio_interrupt_hi2lo(nRF8001->rdyn);
		nRF8001->intSrc &= ~MP_NRF8001_INTSRC_RDYN;
	}
	else {
		mp_gpio_interrupt_lo2hi(nRF8001->rdyn);

		nRF8001->intSrc |= MP_NRF8001_INTSRC_RDYN;

		if(!(nRF8001->intSrc & MP_NRF8001_INTSRC_REQN)) {
			mp_gpio_unset(nRF8001->reqn);
			nRF8001->intSrc |= MP_NRF8001_INTSRC_REQN;
		}

		mp_spi_enable_both(&nRF8001->spi);
	}
}

MP_TASK(_mp_drv_nRF8001_ASR) {
	mp_drv_nRF8001_t *nRF8001 = task->user;
	mp_drv_nRF8001_aci_queue_t *queue;
	char canRequestOff = 0;

	/* receive regMaster shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {
		/* acknowledging */
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* */
	if(nRF8001->duplexStatus & MP_NRF8001_DUPLEX_RX_PENDING) {
		if(nRF8001->pending_rx_pkts.first) {
			queue = nRF8001->pending_rx_pkts.first->user;
			unsigned char opcode = queue->packet.payload[0];

			//mp_printk("pkt size=%d opcode=%x", queue->packet.length, opcode);

			/* execute user callback */
			nRF8001->evts[opcode](nRF8001, queue);

			/* remove from pending */
			mp_list_remove(&nRF8001->pending_rx_pkts, &queue->item);

			/* switch to free rx pkts */
			mp_list_add_first(&nRF8001->rx_pkts, &queue->item, queue);

			task->signal = MP_TASK_SIG_PENDING;
		}
		else {
			nRF8001->duplexStatus &= ~MP_NRF8001_DUPLEX_RX_PENDING;
			canRequestOff++;
		}
	}
	else
		canRequestOff++;


	if(!nRF8001->current_tx_pkts && nRF8001->pending_tx_pkts.first) {

		nRF8001->current_tx_pkts = nRF8001->pending_tx_pkts.first->user;
		mp_list_remove(&nRF8001->pending_tx_pkts, &nRF8001->current_tx_pkts->item);

		if(!(nRF8001->intSrc & MP_NRF8001_INTSRC_REQN)) {
			nRF8001->intSrc |= MP_NRF8001_INTSRC_REQN;
			mp_gpio_unset(nRF8001->reqn);
		}
	}
	else
		canRequestOff++;



	/* disable all ? */
	if(canRequestOff == 2) {
		task->signal = MP_TASK_SIG_SLEEP;
	}
}

static void _mp_drv_nRF8001_spi_interrupt(mp_spi_t *spi, mp_spi_iv_t iv) {
	mp_drv_nRF8001_t *nRF8001 = spi->user;
	mp_drv_nRF8001_aci_queue_t *queueRx = nRF8001->current_rx_pkts;
	mp_drv_nRF8001_aci_queue_t *queueTx = nRF8001->current_tx_pkts;

	mp_spi_flag_t flags = mp_spi_flags_get(spi);

	if(flags & MP_SPI_FL_RX) {


		if(nRF8001->statusByte == 0) {
			mp_spi_rx(spi);
			nRF8001->statusByte = 1;
		}
		else {
			if(nRF8001->duplexStatus & MP_NRF8001_DUPLEX_RX) {
				queueRx->packet.payload[queueRx->rest] = mp_spi_rx(spi);
				queueRx->rest++;

				if(queueRx->packet.length == queueRx->rest) {

					/* add the queue into pending rx */
					mp_list_add_last(&nRF8001->pending_rx_pkts, &queueRx->item, queueRx);

					/* nullize */
					nRF8001->current_rx_pkts = NULL;

					nRF8001->duplexStatus &= ~MP_NRF8001_DUPLEX_RX;
					nRF8001->duplexStatus |= MP_NRF8001_DUPLEX_RX_PENDING;

					nRF8001->statusByte = 0;

					nRF8001->task->signal = MP_TASK_SIG_PENDING;

					if(!nRF8001->current_tx_pkts) {
						mp_spi_disable_both(&nRF8001->spi);
						nRF8001->intSrc &= ~MP_NRF8001_INTSRC_REQN;
						mp_gpio_set(nRF8001->reqn);
						return;
					}
				}
			}
			else {
				unsigned char length = mp_spi_rx(spi);
				if(length > 0) {
					if(!nRF8001->current_rx_pkts) {
						nRF8001->current_rx_pkts = nRF8001->rx_pkts.first->user;
						mp_list_remove(&nRF8001->rx_pkts, &nRF8001->current_rx_pkts->item);
					}

					queueRx = nRF8001->current_rx_pkts;
					queueRx->rest = 0;
					queueRx->packet.length = length;
					nRF8001->duplexStatus |= MP_NRF8001_DUPLEX_RX;
				}
				else {

					nRF8001->statusByte = 0;

					if(!nRF8001->current_tx_pkts) {
						mp_spi_disable_both(&nRF8001->spi);
						nRF8001->intSrc &= ~MP_NRF8001_INTSRC_REQN;
						mp_gpio_set(nRF8001->reqn);
						return;
					}
				}
			}
		}
	}

	if(flags & MP_SPI_FL_TX) {


		queueTx = nRF8001->current_tx_pkts;

		if(queueTx) {
			/* send the rest */
			if(nRF8001->duplexStatus & MP_NRF8001_DUPLEX_TX) {
				mp_spi_tx(spi, queueTx->packet.payload[queueTx->rest]);
				queueTx->rest++;

				if(queueTx->packet.length == queueTx->rest) {

					/* add the queue into pending rx */
					mp_list_add_last(&nRF8001->tx_pkts, &queueTx->item, queueTx);

					/* nullize */
					nRF8001->current_tx_pkts = NULL;

					nRF8001->duplexStatus &= ~MP_NRF8001_DUPLEX_TX;

					nRF8001->task->signal = MP_TASK_SIG_PENDING;
/*
					if(!(nRF8001->duplexStatus & MP_NRF8001_DUPLEX_RX) && nRF8001->statusByte == 0) {
						mp_spi_rx(spi);
						mp_spi_disable_both(&nRF8001->spi);
						nRF8001->intSrc &= ~MP_NRF8001_INTSRC_REQN;
						mp_gpio_set(nRF8001->reqn);

						return;
					}
*/

				}
			}
			/* send length */
			else {
				mp_spi_tx(spi, queueTx->packet.length);
				queueTx->rest = 0;
				nRF8001->duplexStatus |= MP_NRF8001_DUPLEX_TX;
			}
		}
		else
			mp_spi_tx(spi, 0x0);
	}


	return;
}



#endif

