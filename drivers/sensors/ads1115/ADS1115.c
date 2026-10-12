/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2015  Michael VERGOZ                                      *
 * Copyright (C) 2015  VERMAN                                              *
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

#ifdef SUPPORT_DRV_ADS1115

static void _mp_drv_ADS1115_checkConfig(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_ADS1115_onConfigUpdated(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_ADS1115_onResult(mp_regMaster_op_t *operand, mp_bool_t terminate);

static void _mp_drv_ADS1115_onDRDY(void *user);

/**
@defgroup mpDriverTiADS1115 Ti ADS1115

@ingroup mpDriver

@brief Ti ADS1115 16-bits ADC 4 channels

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Mar 2015

ADS1115 are precision analog-to-digital converters (ADCs)
with 16 bits of resolution offered in an ultra-small, leadless QFN-10
package or an MSOP-10 package. The ADS1113/4/5 are designed with precision,
power, and ease of implementation in mind. The ADS1113/4/5 feature an
onboard reference and oscillator. Data are transferred via an I2C-compatible
serial interface; four I2C slave addresses can be selected. The ADS1113/4/5
operate from a single power supply ranging from 2.0V to 5.5V

Breakouts available on :
@li http://www.adafruit.com/product/1085

@{
*/

mp_ret_t mp_drv_ADS1115_init(mp_kernel_t *kernel, mp_drv_ADS1115_t *ADS1115, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(ADS1115, 0, sizeof(*ADS1115));
	ADS1115->kernel = kernel;

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		ADS1115->drdy = mp_gpio_text_handle(value, "ADS1115 DRDY");
		if(!ADS1115->drdy) {
			mp_printk("ADS1115: need a valid DRDY port");
			mp_drv_ADS1115_fini(ADS1115);
			return(FALSE);
		}

		/* set CS high */
		mp_gpio_direction(ADS1115->drdy, MP_GPIO_INPUT);
	}
	else
		ADS1115->drdy = NULL;

	/* open spi */
	ret = mp_i2c_open(kernel, &ADS1115->i2c, options, "ADS1115");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
		{ "frequency", "400000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&ADS1115->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&ADS1115->i2c);
		return(FALSE);
	}

	/* set slave address */
	mp_i2c_setSlaveAddress(&ADS1115->i2c, ADS1115_ADDRESS);

	/* enable chip */
	if(!ADS1115->drdy) {
		mp_printk("ADS1115(%p): require DRDY interrupt for the moment", ADS1115);
		mp_i2c_close(&ADS1115->i2c);
		return(FALSE);
	}

	mp_printk("ADS1115(%p): Initializing", ADS1115);

	/* create regmaster control */
	ret = mp_regMaster_init_i2c(kernel, &ADS1115->regMaster,
			&ADS1115->i2c, ADS1115, "ADS1115 I2C");
	if(ret == FALSE) {
		mp_printk("ADS1115 error while creating regMaster context");
		mp_i2c_close(&ADS1115->i2c);
		return(FALSE);
	}

	/* default config register */
	ADS1115->config = 0x8583;

	/* base initialization */
	mp_drv_ADS1115_assertAfter(ADS1115, 4);
	mp_drv_ADS1115_start(ADS1115);
	mp_drv_ADS1115_updateConfig(ADS1115);

	mp_regMaster_readExt(
		&ADS1115->regMaster,
		mp_regMaster_register(ADS1015_REG_POINTER_CONFIG), 1,
		(unsigned char *)&ADS1115->config, 2,
		_mp_drv_ADS1115_checkConfig, ADS1115,
		TRUE // on the fly swap
	);


	return(TRUE);
}

void mp_drv_ADS1115_fini(mp_drv_ADS1115_t *ADS1115) {
	mp_printk("Unloading ADS1115 driver");
}

/**
 * @brief Update ADS1115 set comparator delay
 *
 * This update the comparator delay. You must execute
 * @ref mp_drv_ADS1115_updateConfig function after changing comparator delay.
 *
 * @param[in] ADS1115 ADS1115 context
 * @param[in] num Number of conversion before releasing alert, could be set to 0, 1, 2, 4
 * @return Normally this function returns TRUE all the time
 */
mp_ret_t mp_drv_ADS1115_assertAfter(mp_drv_ADS1115_t *ADS1115, char num) {

	ADS1115->config &= ~0x3;

	/*
	switch(num) {
		case 0:
			ADS1115->config += ADS1015_REG_CONFIG_CQUE_NONE;
			break;

		case 1:
			ADS1115->config += ADS1015_REG_CONFIG_CQUE_1CONV;
			break;

		case 2:
			ADS1115->config += ADS1015_REG_CONFIG_CQUE_2CONV;
			break;

		case 4:
			ADS1115->config += ADS1015_REG_CONFIG_CQUE_4CONV;
			break;
		default:
			mp_printk("ADS1115(%p): Invalid comparator queue", ADS1115);
			return(FALSE);
	}
*/

	return(TRUE);

}

/**
 * @brief Start ADS1115 conversion
 *
 * This starts the ADC conversion. You must execute
 * @ref mp_drv_ADS1115_updateConfig function after changing comparator delay.
 *
 * @param[in] ADS1115 ADS1115 context
 * @return returns @ref TRUE everytime
 */
mp_ret_t mp_drv_ADS1115_start(mp_drv_ADS1115_t *ADS1115) {
	ADS1115->config |= ADS1015_REG_CONFIG_PGA_6_144V;
	ADS1115->config &= ~ADS1015_REG_CONFIG_MODE_SINGLE;
	ADS1115->config &= ~ADS1015_REG_CONFIG_OS_MASK;
	return(TRUE);
}

/**
 * @brief Stop ADS1115 conversion
 *
 * This stops the ADC conversion. You must execute
 * @ref mp_drv_ADS1115_updateConfig function after changing comparator delay.
 *
 *
 * @param[in] ADS1115 ADS1115 context
 * @return returns @ref TRUE everytime
 */
mp_ret_t mp_drv_ADS1115_stop(mp_drv_ADS1115_t *ADS1115) {
	ADS1115->config |= ADS1015_REG_CONFIG_MODE_SINGLE;
	return(TRUE);
}


/**
 * @brief Update ADS1115 configuration register
 *
 * This update configuration register of the ADS1115. You must wait the end
 * of the update process before reloading one.
 *
 * @param[in] ADS1115 ADS1115 context
 * @return TRUE if operation pending and FALSE if an operation is already pending
 */
mp_ret_t mp_drv_ADS1115_updateConfig(mp_drv_ADS1115_t *ADS1115) {
	if(ADS1115->flags & ADS1115_FLAG_CONFIG) {
		mp_printk("ADS1115(%p): Configuration update in process", ADS1115);
		return(FALSE);
	}

	/* load config buffer */
	ADS1115->configBuffer[0] = ADS1015_REG_POINTER_CONFIG;
	ADS1115->configBuffer[1] = (unsigned char)(ADS1115->config >> 8);
	ADS1115->configBuffer[2] = (unsigned char)(ADS1115->config & 0xFF);

	/* lock update */
	ADS1115->flags |= ADS1115_FLAG_CONFIG;

	mp_regMaster_write(
		&ADS1115->regMaster,
		ADS1115->configBuffer, 3, // chelou ce cast en fait
		_mp_drv_ADS1115_onConfigUpdated, ADS1115
	);

	return(TRUE);
}



/**@}*/

static void _mp_drv_ADS1115_checkConfig(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS1115_t *ADS1115 = operand->user;

	/*
	if(ADS1115->config != 0x8583) {
		mp_printk("ADS1115(%p): Got who iam %x (bad), terminating", operand->user, ADS1115->config);
		mp_regMaster_fini(&ADS1115->regMaster);
		return;
	}
	else
		mp_printk("ADS1115(%p): Got who iam %x (good)", operand->user, ADS1115->config);
	*/
	/* install drdy interrupt high > low */
	mp_gpio_interrupt_set(ADS1115->drdy, _mp_drv_ADS1115_onDRDY, ADS1115, "ADS1115");
	mp_gpio_interrupt_lo2hi(ADS1115->drdy);
	//mp_gpio_interrupt_hi2lo(ADS1115->drdy);

	mp_printk("ADS1115(%p): Configuration register checked set to %x", operand->user, ADS1115->config);

}

static void _mp_drv_ADS1115_onConfigUpdated(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS1115_t *ADS1115 = operand->user;

	mp_printk("ADS1115(%p): Configuration register updated %x", operand->user, ADS1115->config);
}

static void _mp_drv_ADS1115_onResult(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_ADS1115_t *ADS1115 = operand->user;

	mp_printk("ADS1115(%p): Got result %d", operand->user, ADS1115->value);

	/* unlock update */
	ADS1115->flags &= ~ADS1115_FLAG_CONFIG;

	mp_regMaster_readExt(
		&ADS1115->regMaster,
		mp_regMaster_register(ADS1015_REG_POINTER_CONVERT), 1,
		(unsigned char *)&ADS1115->value, 2,
		_mp_drv_ADS1115_onResult, ADS1115,
		TRUE // on the fly swap
	);
}


static void _mp_drv_ADS1115_onDRDY(void *user) {
	mp_drv_ADS1115_t *ADS1115 = user;

	mp_regMaster_readExt(
		&ADS1115->regMaster,
		mp_regMaster_register(ADS1015_REG_POINTER_CONVERT), 1,
		(unsigned char *)&ADS1115->value, 2,
		_mp_drv_ADS1115_onResult, ADS1115,
		TRUE // on the fly swap
	);


}

#endif

