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

static void _mp_drv_nRF8001_onIntActive(void *user);
static void _mp_drv_nRF8001_onIntRdyn(void *user);

static void _mp_drv_nRF8001_spi_interrupt(mp_spi_t *spi, mp_spi_flag_t flag);

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
 *  @li active : Active wire (int)
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

	/* Active IO */
	value = mp_options_get(options, "active");
	if(!value) {
		mp_printk("nRF8001: need active port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	nRF8001->active = mp_gpio_text_handle(value, "nRF8001 active");
	if(!nRF8001->active) {
		mp_printk("nRF8001: need active port");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}

	mp_gpio_direction(nRF8001->active, MP_GPIO_INPUT);

	/* install Active interrupt high > low */
	ret = mp_gpio_interrupt_set(nRF8001->active, _mp_drv_nRF8001_onIntActive, nRF8001, who);
	if(ret == FALSE) {
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	mp_gpio_interrupt_lo2hi(nRF8001->active);
	mp_gpio_interrupt_disable(nRF8001->active);

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
	mp_gpio_set(nRF8001->reqn);

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
	ret = mp_gpio_interrupt_set(nRF8001->rdyn, _mp_drv_nRF8001_onIntRdyn, nRF8001, who);
	if(ret == FALSE) {
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	mp_gpio_interrupt_lo2hi(nRF8001->rdyn);
	mp_gpio_interrupt_disable(nRF8001->rdyn);

	/* open spi */
	ret = mp_spi_open(kernel, &nRF8001->spi, options, "nRF8001");
	if(ret == FALSE) {
		mp_printk("Error openning SPI interface");
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}

	mp_options_t setup[] = {
		{ "frequency", "1000000" },
		{ "phase", "change" },
		{ "polarity", "high" },
		{ "first", "MSB" },
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

	mp_list_init(&nRF8001->pendingRX);
	mp_list_init(&nRF8001->pendingTX);

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

	if(nRF8001->active)
		mp_gpio_release(nRF8001->active);

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



mp_ret_t mp_drv_nRF8001_start(mp_drv_nRF8001_t *nRF8001) {
	mp_gpio_set(nRF8001->reset);
	mp_clock_delay(62);
	//mp_gpio_interrupt_disable(nRF8001->active);
	mp_gpio_interrupt_enable(nRF8001->rdyn);
	return(TRUE);
}

mp_ret_t mp_drv_nRF8001_stop(mp_drv_nRF8001_t *nRF8001) {
	//mp_gpio_interrupt_disable(nRF8001->active);
	mp_gpio_interrupt_disable(nRF8001->rdyn);

	mp_gpio_unset(nRF8001->reset);
	mp_printk("nRF8001 a lot to do there");
	return(TRUE);
}

/**@}*/

static void _mp_drv_nRF8001_read(mp_drv_nRF8001_t *nRF8001) {
	mp_drv_nRF8001_aci_queue_t *queue;

	/* allocate new queue */
	queue = mp_mem_alloc(nRF8001->kernel, sizeof(*queue));

	queue->state = MP_NRF8001_STATE_RX_DEBUG;
	queue->packet.length = 0;
	queue->packet.opcode = 0;
	queue->rest = 0;

	/* add queue at last pending */
	mp_list_add_last(&nRF8001->pendingRX, &queue->item, queue);

	/* tell to the scheduler task pending */
	if(nRF8001->task->signal == MP_TASK_SIG_SLEEP)
		nRF8001->task->signal = MP_TASK_SIG_PENDING;

}

static void _mp_drv_nRF8001_write(mp_drv_nRF8001_t *nRF8001, mp_drv_nRF8001_aci_queue_t *queue) {
	queue->state = MP_NRF8001_STATE_TX;
	queue->packet.length = 0;
	queue->packet.opcode = 0;
	queue->rest = 0;

	/* add queue at last pending */
	mp_list_add_last(&nRF8001->pendingTX, &queue->item, queue);

	/* tell to the scheduler task pending */
	if(nRF8001->task->signal == MP_TASK_SIG_SLEEP)
		nRF8001->task->signal = MP_TASK_SIG_PENDING;

}


static void _mp_drv_nRF8001_onIntActive(void *user) {
	mp_drv_nRF8001_t *nRF8001 = user;
	nRF8001->intSrc |= 0x2;
	nRF8001->task->signal = MP_TASK_SIG_PENDING;
	mp_printk("nRF8001(%p) Radio is active", user);
}

static void _mp_drv_nRF8001_onIntRdyn(void *user) {
	mp_drv_nRF8001_t *nRF8001 = user;

	/* at this time reqn is high */
	nRF8001->intSrc |= 0x4;

	/* disable rdyn for the moment, asr will activate reqn */
	mp_gpio_interrupt_disable(nRF8001->rdyn);

	nRF8001->task->signal = MP_TASK_SIG_PENDING;

}

MP_TASK(_mp_drv_nRF8001_ASR) {
	mp_drv_nRF8001_t *nRF8001 = task->user;

	/* receive regMaster shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {
		/* acknowledging */
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* handle RDY */
	if(nRF8001->intSrc & 0x4) {

		mp_printk("nRF8001(%p) RDYn", nRF8001);


		_mp_drv_nRF8001_read(nRF8001);


		mp_gpio_unset(nRF8001->reqn);
		nRF8001->intSrc &= ~0x4;
	}


	/* check for read packet */
	if(nRF8001->pendingRX.first) {
		nRF8001->current = nRF8001->pendingRX.first->user;

		mp_list_remove(&nRF8001->pendingRX, &nRF8001->current->item);
		mp_gpio_unset(nRF8001->reqn);
		mp_spi_enable_rx(&nRF8001->spi);
		mp_spi_enable_tx(&nRF8001->spi);
	}

	task->signal = MP_TASK_SIG_SLEEP;
}

static void _mp_drv_nRF8001_spi_interrupt(mp_spi_t *spi, mp_spi_flag_t flag) {
	mp_drv_nRF8001_t *nRF8001 = spi->user;
	mp_drv_nRF8001_aci_queue_t *queue;
	int rest;

	/* get first queue */
	queue = nRF8001->current;

	P10OUT ^= 0x40;

	/* send registers */
	if(flag == MP_SPI_FL_TX) {
		mp_spi_tx(spi, 0x0);
	}

	if(queue->state == MP_NRF8001_STATE_RX_DEBUG && flag == MP_SPI_FL_TX) {
		mp_spi_rx(spi);
	}
	if(queue->state == MP_NRF8001_STATE_RX_DEBUG && flag == MP_SPI_FL_RX) {
		mp_spi_rx(spi);
		queue->state = MP_NRF8001_STATE_RX_LENGTH;
		queue->rest++;
	}
	else if(queue->state == MP_NRF8001_STATE_RX_LENGTH && flag == MP_SPI_FL_RX) {
		queue->packet.length = mp_spi_rx(spi);
		queue->state = MP_NRF8001_STATE_RX;
		queue->rest++;
	}
	else if(queue->state == MP_NRF8001_STATE_RX && flag == MP_SPI_FL_RX) {
		queue->packet.payload[queue->rest-2] = mp_spi_rx(spi);

		queue->rest++;


		if(queue->packet.length == queue->rest) {
			mp_gpio_set(nRF8001->reqn);
			mp_spi_disable_rx(&nRF8001->spi);
			mp_spi_disable_tx(&nRF8001->spi);
			nRF8001->task->signal = MP_TASK_SIG_PENDING;
		}

	}

	/*
	else if(queue->state == MP_NRF8001_STATE_NULLRX && flag == MP_SPI_FL_RX) {
		queue->state = MP_NRF8001_STATE_RX;
		mp_spi_tx(spi, 0);
		mp_spi_rx(spi);
	}
	else if(queue->state == MP_NRF8001_STATE_NULLTX && flag == MP_SPI_FL_RX) {
		queue->state = MP_NRF8001_STATE_TX;
		mp_spi_rx(spi);

		nRF8001->enableTX(nRF8001);
		nRF8001->disableRX(nRF8001);
	}
	*/

	return;
}

#endif
