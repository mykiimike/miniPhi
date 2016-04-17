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
static void _mp_drv_ADS124X_onSleep(mp_regMaster_op_t *operand, mp_bool_t terminate);

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

	mp_printk("ADS124X(%p): Initializing", ADS124X);

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
	mp_gpio_interrupt_enable(ADS124X->drdy);
	mp_gpio_set(ADS124X->reset);
	mp_gpio_set(ADS124X->start);
	mp_clock_delay(500);

	mp_regMaster_write(
		&ADS124X->regMaster,
		mp_regMaster_register(ADS124X_SPI_RESET), 1,
		_mp_drv_ADS124X_onReset, ADS124X
	);

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
}


mp_ret_t mp_drv_ADS124X_wakeup(mp_drv_ADS124X_t *ADS124X) {
	mp_printk("ADS124X(%p) Ask to wakeup", ADS124X);

	mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);
	mp_regMaster_write(
		&ADS124X->regMaster,
		mp_regMaster_register(ADS124X_SPI_WAKEUP), 1,
		_mp_drv_ADS124X_onWakeup, ADS124X
	);

	return(TRUE);
}

mp_ret_t mp_drv_ADS124X_WREG(mp_drv_ADS124X_t *ADS124X, unsigned char from, unsigned char *regs, int size) {
	mp_printk("ADS124X(%p) Ask to wakeup", ADS124X);

	unsigned char *ptr = mp_mem_alloc(ADS124X->kernel, size+2);
	unsigned char *src = ptr;
	int a;

	*(ptr++) = ADS124X_SPI_WREG | from;
	*(ptr++) = size-1;
	for(a=0; a<size; a++)
		*(ptr++) = regs[0];

	for(a=0; a<size+2; a++)
		mp_printk(">> %x", src[a]);

	mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);
	mp_regMaster_write(
		&ADS124X->regMaster,
		src, size+2,
		_mp_drv_ADS124X_onRegWrite, ADS124X
	);

	return(TRUE);
}


mp_ret_t mp_drv_ADS124X_sleep(mp_drv_ADS124X_t *ADS124X) {
	mp_printk("ADS124X(%p) Entering in sleep", ADS124X);
	mp_regMaster_write(
		&ADS124X->regMaster,
		mp_regMaster_register(ADS124X_SPI_SLEEP), 1,
		_mp_drv_ADS124X_onRegWrite, ADS124X
	);

	return(TRUE);
}


static void _mp_drv_ADS124X_onRegdump(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_mem_free(ADS124X->kernel, operand->reg);

	if(terminate == YES)
		return;

	mp_printk("ADS124X(%p) Configuration registers dumped", ADS124X);

	int a;

	for(a=0; a<_ADS124X_REGCOUNT; a++)
		mp_printk("ADS124X(%p) RREG #%d = %x", ADS124X, a, ADS124X->registerMap[a]);

}

static void _mp_drv_ADS124X_onRegWrite(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_mem_free(ADS124X->kernel, operand->reg);

	if(terminate == YES)
		return;

	mp_printk("ADS124X(%p) Configuration registers writed", ADS124X);


	{
		mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);
		unsigned char *ptr = mp_mem_alloc(ADS124X->kernel, 3);
		unsigned char *src = ptr;

		*(ptr++) = ADS124X_SPI_RREG;

		if(ADS124X->version == _TI_ADS1246) {
			*ptr = 0x0b;
			mp_regMaster_readExt(
				&ADS124X->regMaster,
				src, 2,
				(unsigned char *)&ADS124X->registerMap, *ptr,
				_mp_drv_ADS124X_onRegdump, ADS124X,
				FALSE
			);
		}
		else {
			*ptr = 15-1;
			mp_printk(">>>>>>>>>>> %x %x", *src, *(src+1));
			mp_regMaster_readExt(
				&ADS124X->regMaster,
				src, 2,
				(unsigned char *)&ADS124X->registerMap, *ptr,
				_mp_drv_ADS124X_onRegdump, ADS124X,
				FALSE
			);

		}
	}


}


static void _mp_drv_ADS124X_onReset(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_printk("_mp_drv_ADS124X_onReset %p", ADS124X);

	if(terminate == YES)
		return;

	mp_drv_ADS124X_wakeup(ADS124X);
}

static void _mp_drv_ADS124X_onWakeup(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;
	mp_printk("_mp_drv_ADS124X_onWakeup %p", ADS124X);

	if(terminate == YES)
		return;





	{
		mp_regMaster_setChipSelect(&ADS124X->regMaster, ADS124X->cs);
		unsigned char *ptr = mp_mem_alloc(ADS124X->kernel, 3);
		unsigned char *src = ptr;

		*(ptr++) = ADS124X_SPI_RREG;

		if(ADS124X->version == _TI_ADS1246) {
			*ptr = 0x0b;
			mp_regMaster_readExt(
				&ADS124X->regMaster,
				src, 2,
				(unsigned char *)&ADS124X->registerMap, *ptr,
				_mp_drv_ADS124X_onRegdump, ADS124X,
				FALSE
			);
		}
		else {
			*ptr = 15-1;
			mp_printk(">>>>>>>>>>> %x %x", *src, *(src+1));
			mp_regMaster_readExt(
				&ADS124X->regMaster,
				src, 2,
				(unsigned char *)&ADS124X->registerMap, *ptr,
				_mp_drv_ADS124X_onRegdump, ADS124X,
				FALSE
			);

		}
	}
	unsigned char cmd[] = { ADS12478_REG_SYS0_PGA_4 | ADS12478_REG_SYS0_DOR_10SPS };
	mp_drv_ADS124X_WREG(ADS124X, ADS1246_REG_SYS0, cmd, 1);

}

static void _mp_drv_ADS124X_onSleep(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS124X_t *ADS124X = operand->user;

	if(terminate == YES)
		return;

	mp_printk("_mp_drv_ADS124X_onSleep %p", ADS124X);
}


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

	//mp_task_signal(LSM9DS0->task, MP_TASK_SIG_PENDING);
	//mp_printk("DRDY!!!!!!!!!!!");




}


#endif

