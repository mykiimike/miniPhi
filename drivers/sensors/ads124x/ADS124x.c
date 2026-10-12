/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2016  Michael VERGOZ                                      *
 * Copyright (C) 2016  VERMAN                                              *
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

#ifdef SUPPORT_DRV_ADS124X

static void _mp_drv_ADS124X_onDRDY(void *user);

static void _mp_drv_ADS124X_onRegdump(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_ADS124X_onRegWrite(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_ADS124X_onReset(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_ADS124X_onWakeup(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_ADS124X_onDummy(mp_regMaster_op_t *operand, mp_bool_t terminate);

MP_TASK(_mp_drv_ADS124X_ASR);

/**
@defgroup mpDriverTiADS124X Ti ADS1246/7/8

@ingroup mpDriver

@brief Ti ADS1246/7/8 24-Bit Analog-to-Digital Converters for Temperature Sensors

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2016
Michael Vergoz <mv@verman.fr>

@date 26 Mar 2016

The ADS1246, ADS1247, and ADS1248 are highly-integrated, precision, 24-bit analog-to-digital
converters (ADCs). The ADS1246/7/8 feature an onboard, low-noise, programmable gain amplifier
(PGA), a precision delta-sigma (ΔΣ) ADC with a single-cycle settling digital filter, and an internal
oscillator. The ADS1247 and ADS1248 also provide a built-in, very low drift voltage reference with 10mA
output capacity, and two matched programmable current digital-to-analog converters (DACs). The
ADS1246/7/8 provide a complete front-end solution for temperature sensor applications including thermal
couples, thermistors, and RTDs.

Links :
@li http://www.ti.com/lit/ds/symlink/ads1247.pdf

@{
*/


/**
 * @brief Initiate ADS124X context
 *
 * @param[in] kernel Kernel context
 * @param[in] ADS124X Context
 * @param[in] options Special options :
 *  @li version : ADS1246, ADS1247, ADS1248
 *  @li clk : Clock port source SPI
 *  @li cs : Chip select
 *  @li simo : SPI Slave In Master Out
 *  @li somi : SPI Slave Out Master In
 *  @li drdy : DRDY port data read/data ready
 *  @li reset : Reset port
 *  @li Start : Start port
 * @param[in] who Who own the driver session
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_ADS124X_init(mp_kernel_t *kernel, mp_drv_ADS124X_t *ADS124X, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(ADS124X, 0, sizeof(*ADS124X));
	ADS124X->kernel = kernel;

	/* version */
	value = mp_options_get(options, "version");
	if(!value) {
		mp_printk("ADS124X(%p): You need to define an ADS version", ADS124X);
		return(FALSE);
	}
	if(strcmp(value, "ADS1246") == 0)
		ADS124X->version = _TI_ADS1246;
	else if(strcmp(value, "ADS1247") == 0)
			ADS124X->version = _TI_ADS1247;
	else if(strcmp(value, "ADS1248") == 0)
			ADS124X->version = _TI_ADS1248;
	else {
		mp_printk("ADS124X(%p): Unrecognised ADS version", ADS124X);
		return(FALSE);
	}

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		ADS124X->drdy = mp_gpio_text_handle(value, "ADS124X DRDY");
		if(!ADS124X->drdy) {
			mp_printk("ADS124X: need a valid DRDY port");
			mp_drv_ADS124X_fini(ADS124X);
			return(FALSE);
		}

		/* set CS high */
		mp_gpio_direction(ADS124X->drdy, MP_GPIO_INPUT);

		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(ADS124X->drdy, _mp_drv_ADS124X_onDRDY, ADS124X, who);
		if(ret == FALSE) {
			mp_drv_ADS124X_fini(ADS124X);
			return(FALSE);
		}
		mp_gpio_interrupt_hi2lo(ADS124X->drdy);
		mp_gpio_interrupt_disable(ADS124X->drdy);
	}
	else {
		mp_printk("ADS124X: need DRDY port");
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}

	/* configure chip select */
	value = mp_options_get(options, "cs");
	if(!value) {
		mp_printk("ADS124X: need cs port");
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}
	ADS124X->cs = mp_gpio_text_handle(value, "ADS124X cs");
	if(!ADS124X->cs) {
		mp_printk("ADS124X: need CS port");
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}
	mp_gpio_direction(ADS124X->cs, MP_GPIO_OUTPUT);
	mp_gpio_set(ADS124X->cs);

	/* configure reset port */
	value = mp_options_get(options, "reset");
	if(!value) {
		mp_printk("ADS124X: need RESET port");
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}
	ADS124X->reset = mp_gpio_text_handle(value, "ADS124X reset");
	if(!ADS124X->reset) {
		mp_printk("ADS124X: RESET port already taken");
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}
	mp_gpio_direction(ADS124X->reset, MP_GPIO_OUTPUT);
	mp_gpio_set(ADS124X->reset);

	/* configure start port */
	value = mp_options_get(options, "start");
	if(!value) {
		mp_printk("ADS124X: need START port");
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}
	ADS124X->start = mp_gpio_text_handle(value, "ADS124X start");
	if(!ADS124X->start) {
		mp_printk("ADS124X: START port already taken");
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}
	mp_gpio_direction(ADS124X->start, MP_GPIO_OUTPUT);
	mp_gpio_unset(ADS124X->start);

	/* open spi */
	ret = mp_spi_open(kernel, &ADS124X->spi, options, "ADS124X");
	if(ret == FALSE) {
		mp_printk("ADS124X(%p): Error openning SPI interface", ADS124X);
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}

	mp_options_t setup[] = {
		{ "frequency", "1000000" },

		//{ "phase", "change" },
		//{ "polarity", "low" },
		//{ "first", "lsb" },

		{ "role", "master" },

		{ "bit", "8" },
		{ NULL, NULL }
	};
	ret = mp_spi_setup(&ADS124X->spi, setup);
	if(ret == FALSE) {
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}



	/* create ASR task */
	ADS124X->task = mp_task_create(&kernel->tasks, who, _mp_drv_ADS124X_ASR, ADS124X, 1000);
	if(!ADS124X->task) {
		mp_printk("ADS124X(%p) FATAL could not create ASR task", ADS124X);
		mp_drv_ADS124X_fini(ADS124X);
		return(FALSE);
	}

	/* create regmaster control */
	ret = mp_regMaster_init_spi(kernel, &ADS124X->regMaster,
			&ADS124X->spi, ADS124X, "ADS124X SPI");
	if(ret == FALSE) {
		mp_printk("ADS124X(%p) error while creating regMaster context", ADS124X);
		mp_spi_close(&ADS124X->spi);
		return(FALSE);
	}

	/* change nop operation */
	mp_regMaster_setNOP(&ADS124X->regMaster, ADS124X_SPI_NOP);

	/* turn on the device */
	mp_gpio_unset(ADS124X->reset);
	mp_gpio_set(ADS124X->reset);
	mp_gpio_set(ADS124X->start);


	mp_regMaster_write(
		&ADS124X->regMaster,
		mp_regMaster_register(ADS124X_SPI_RESET), 1,
		_mp_drv_ADS124X_onReset, ADS124X
	);

	mp_printk("ADS124X(%p): Initializing", ADS124X);

	/*
	mp_regMaster_readExt(
		&ADS124X->regMaster,
		mp_regMaster_register(ADS1015_REG_POINTER_CONFIG), 1,
		(unsigned char *)&ADS124X->config, 2,
		_mp_drv_ADS124X_checkConfig, ADS124X,
		TRUE // on the fly swap
	);
*/

	return(TRUE);
}

void mp_drv_ADS124X_fini(mp_drv_ADS124X_t *ADS124X) {
	mp_printk("Unloading ADS124X driver");

	mp_spi_close(&ADS124X->spi);

	mp_regMaster_fini(&ADS124X->regMaster);

	mp_gpio_release(ADS124X->drdy);
	mp_gpio_release(ADS124X->cs);
	mp_gpio_release(ADS124X->reset);
	mp_gpio_release(ADS124X->start);
}

/**
 * @brief Start ADS conversion
 *
 * @param[in] ADS124X Context
 */
void mp_drv_ADS124X_start(mp_drv_ADS124X_t *ADS124X) {
	mp_gpio_set(ADS124X->start);
}

/**
 * @brief Stop ADS conversion
 *
 * @param[in] ADS124X Context
 */
void mp_drv_ADS124X_stop(mp_drv_ADS124X_t *ADS124X) {
	mp_gpio_unset(ADS124X->start);
}

/**
 * @brief Start ADS byte conversion
 *
 * @param[in] ADS124X Context
 */
void mp_drv_ADS124X_startRead(mp_drv_ADS124X_t *ADS124X) {
	mp_gpio_interrupt_enable(ADS124X->drdy);
}

/**
 * @brief Stop ADS byte conversion
 *
 * @param[in] ADS124X Context
 */
void mp_drv_ADS124X_stopRead(mp_drv_ADS124X_t *ADS124X) {
	mp_gpio_interrupt_disable(ADS124X->drdy);
}

/**
 * @brief Send wakeup ADS command
 *
 * @param[in] ADS124X Context
 * @param[out] ADS124X Context
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_ADS124X_wakeup(mp_drv_ADS124X_t *ADS124X, mp_regMaster_cb_t callback) {
	mp_regMaster_cb_t used = _mp_drv_ADS124X_onWakeup;
	if(callback)
		used = callback;

	mp_printk("ADS124X(%p) Sending wakeup", ADS124X);

	mp_drv_ADS124X_stopRead(ADS124X);

	mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);
	mp_regMaster_write(
		&ADS124X->regMaster,
		mp_regMaster_register(ADS124X_SPI_WAKEUP), 1,
		used, ADS124X
	);

	return(TRUE);
}

/**
 * @brief Send sleep ADS command
 *
 * @param[in] ADS124X Context
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_ADS124X_sleep(mp_drv_ADS124X_t *ADS124X, mp_regMaster_cb_t callback) {
	mp_regMaster_cb_t used = _mp_drv_ADS124X_onDummy;
	if(callback)
		used = callback;

	mp_printk("ADS124X(%p) Sending sleep", ADS124X);

	mp_regMaster_write(
		&ADS124X->regMaster,
		mp_regMaster_register(ADS124X_SPI_SLEEP), 1,
		used, ADS124X
	);

	return(TRUE);
}

/**
 * @brief Send write register ADS command
 *
 * @param[in] ADS124X Context
 * @param[in] from Register offset
 * @param[in] regs Registers to write
 * @param[in] size Size of registers
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_ADS124X_writeRegister(mp_drv_ADS124X_t *ADS124X, mp_regMaster_cb_t callback, unsigned char from, unsigned char *regs, int size) {

	mp_regMaster_cb_t used = _mp_drv_ADS124X_onRegWrite;
	if(callback)
		used = callback;

	unsigned char *ptr = mp_mem_alloc(ADS124X->kernel, size+2);
	unsigned char *src = ptr;
	int a;

	*(ptr++) = ADS124X_SPI_WREG | from;
	*(ptr++) = size-1;
	for(a=0; a<size; a++)
		*(ptr++) = regs[0];

	mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);
	mp_regMaster_write(
		&ADS124X->regMaster,
		src, size+2,
		used, ADS124X
	);

	return(TRUE);
}

/**
 * @brief Send read register ADS command
 *
 * @param[in] ADS124X Context
 * @param[in] from Register offset
 * @param[in] size Size of registers
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_ADS124X_readRegister(mp_drv_ADS124X_t *ADS124X, mp_regMaster_cb_t callback, unsigned char from, int size) {
	mp_regMaster_cb_t used = _mp_drv_ADS124X_onRegdump;
	if(callback)
		used = callback;

	unsigned char *ptr = mp_mem_alloc(ADS124X->kernel, size+2);
	unsigned char *src = ptr;

	*(ptr++) = ADS124X_SPI_RREG | from;
	*ptr = size-1;

	mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);
/*
	mp_regMaster_readExt(
		&ADS124X->regMaster,
		src, 2,
		(unsigned char *)&ADS124X->registerMap, size,
		used, ADS124X,
		FALSE
	);
*/
	return(TRUE);
}

static void _mp_drv_ADS124X_onRegdump(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_mem_free(ADS124X->kernel, operand->reg);

	if(terminate == YES)
		return;

	mp_printk("ADS124X(%p) Configuration registers dumped", ADS124X);

	int a;
/*
	for(a=0; a<_ADS124X_REGCOUNT; a++)
		mp_printk("ADS124X(%p) RREG #%d = %x", ADS124X, a, ADS124X->registerMap[a]);
		*/

}

static void _mp_drv_ADS124X_onRegWrite(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_mem_free(ADS124X->kernel, operand->reg);

	if(terminate == YES)
		return;

	//mp_printk("ADS124X(%p) Configuration registers writed", ADS124X);
}


static void _mp_drv_ADS124X_onReset(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_printk("_mp_drv_ADS124X_onReset %p", ADS124X);

	if(terminate == YES)
		return;
}

static void _mp_drv_ADS124X_onWakeup(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_printk("_mp_drv_ADS124X_onWakeup %p", ADS124X);

	if(terminate == YES)
		return;

}

static void _mp_drv_ADS124X_onData(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	long value = 0;

	value = operand->wait[2];
	value = value << 16;
	value |= operand->wait[1]<<8;
	value |= operand->wait[0];

	//mp_printk("Data %ld - %x %x %x", value, operand->wait[2], operand->wait[1], operand->wait[0]);

	mp_mem_free(ADS124X->kernel, operand->wait);
	operand->wait = NULL;

	if(ADS124X->onData)
		ADS124X->onData(ADS124X, value);

	if(terminate == YES)
		return;
}


static void _mp_drv_ADS124X_onDummy(mp_regMaster_op_t *operand, mp_bool_t terminate) { }

/*
#define ADS124X_SPI_SYNC     (0x04)
#define ADS124X_SPI_RESET    (0x06)
#define ADS124X_SPI_NOP      (0xFF)
#define ADS124X_SPI_RDATA    (0x12)
#define ADS124X_SPI_RDATAC   (0x14)
#define ADS124X_SPI_SDATAC   (0x16)
#define ADS124X_SPI_RREG     (0x20)
#define ADS124X_SPI_WREG     (0x40)
#define ADS124X_SPI_SYSOCAL  (0x60)
#define ADS124X_SPI_SYSGCAL  (0x61)
#define ADS124X_SPI_SELFOCAL (0x62)
*/


static void _mp_drv_ADS124X_onDRDY(void *user) {
	mp_drv_ADS124X_t *ADS124X = user;


	ADS124X->onDrdy = 1;
	mp_task_signal(ADS124X->task, MP_TASK_SIG_PENDING);

	//memset(src, 0, 3);
/*

*/

}

MP_TASK(_mp_drv_ADS124X_ASR) {
	mp_drv_ADS124X_t *ADS124X = task->user;

	/* receive regMaster shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {

		/* acknowledging */
		mp_task_signal(ADS124X->task, MP_TASK_SIG_DEAD);
		return;
	}

	mp_interrupt_disable();
	/* data available */
	if(ADS124X->onDrdy == 1) {
		unsigned char *src = mp_mem_alloc(ADS124X->kernel, 10);

		mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);

		mp_regMaster_readExt(
			&ADS124X->regMaster,
			mp_regMaster_register(ADS124X_SPI_RDATA), 1,
			src, 3,
			_mp_drv_ADS124X_onData, ADS124X,
			TRUE
		);

		//mp_printk("DRDY!!!!!!!!!!!");
		ADS124X->onDrdy = 0;
	}
	mp_interrupt_enable();

	mp_task_signal(ADS124X->task, MP_TASK_SIG_SLEEP);


}

#endif

