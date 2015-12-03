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

static void _mp_drv_nRF8001_onIntActive(void *user);
static void _mp_drv_nRF8001_onIntRdyn(void *user);

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

	/* install Active interrupt high > low */
	ret = mp_gpio_interrupt_set(nRF8001->active, _mp_drv_nRF8001_onIntActive, nRF8001, who);
	if(ret == FALSE) {
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}
	mp_gpio_interrupt_hi2lo(nRF8001->active);

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

	/* install Rdyn interrupt high > low */
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

	nRF8001->init = 1;

	/* create regmaster control */
	ret = mp_regMaster_init_spi(kernel, &nRF8001->regMaster,
			&nRF8001->spi, nRF8001, "nRF8001 SPI");
	if(ret == FALSE) {
		mp_printk("nRF8001 error while creating regMaster context");
		mp_drv_nRF8001_fini(nRF8001);

		return(FALSE);
	}

	nRF8001->init = 2;

	/* create ASR task */
	nRF8001->task = mp_task_create(&kernel->tasks, who, _mp_drv_nRF8001_ASR, nRF8001, 100);
	if(!nRF8001->task) {
		mp_printk("nRF8001(%p) FATAL could not create ASR task", nRF8001);
		mp_drv_nRF8001_fini(nRF8001);
		return(FALSE);
	}

	nRF8001->init = 3;

	mp_printk("nRF8001(%p) driver initialization in memory structure size of %d bytes", nRF8001, sizeof(*nRF8001));
	mp_clock_delay(50);

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

	if(nRF8001->init >= 2)
		mp_regMaster_fini(&nRF8001->regMaster);

	if(nRF8001->init >= 1)
		mp_spi_close(&nRF8001->spi);

	mp_printk("Stopping nRF8001");
	return(TRUE);
}


/**@}*/

static void _mp_drv_nRF8001_onIntActive(void *user) {
	//mp_drv_nRF8001_t *nRF8001 = user;
	//nRF8001->intSrc |= 0x2;
	//nRF8001->task->signal = MP_TASK_SIG_PENDING;
}

static void _mp_drv_nRF8001_onIntRdyn(void *user) {
	//mp_drv_nRF8001_t *nRF8001 = user;
	//nRF8001->intSrc |= 0x4;
	//nRF8001->task->signal = MP_TASK_SIG_PENDING;
}

MP_TASK(_mp_drv_nRF8001_ASR) {
	mp_drv_nRF8001_t *nRF8001 = task->user;

	/* receive regMaster shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {
		/* acknowledging */
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

/*
	if(nRF8001->intSrc & 0x4) {

		if(nRF8001->accelCal == 0) {
			mp_drv_nRF8001_xmRead(
				nRF8001, OUT_X_L_A | 0x80,
				(unsigned char *)&nRF8001->buffer, 6,
				_mp_drv_nRF8001_onAccelRead
			);
		}
		else {
			mp_drv_nRF8001_xmRead(
				nRF8001, OUT_X_L_A | 0x80,
				(unsigned char *)&nRF8001->buffer, 6,
				_mp_drv_nRF8001_onAccelCalibrationRead
			);
		}
		nRF8001->intSrc &= ~0x4;
	}
*/

	task->signal = MP_TASK_SIG_SLEEP;
}

