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

#include <mp.h>

#ifdef SUPPORT_DRV_TMP006

static void mp_drv_TMP006_write(mp_drv_TMP006_t *TMP006, unsigned char address, unsigned short writeByte);
static void _mp_drv_TMP006_onDRDY(void *user);

static void _mp_drv_TMP006_onManufacturerID(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_TMP006_onDeviceID(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_TMP006_onSettings(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_TMP006_writeControl(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_TMP006_onRawDieTemperature(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_TMP006_onRawVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate);

/**
@defgroup mpDriverTiTMP006 Ti TMP006

@ingroup mpDriver

@brief Ti TMP006 Object temperature

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015


Configuration for TMP006 example :
@li gate = USCI_B3
@li SDA = 10.1 / ext 1-17
@li SCL = 10.2 / ext 1-16
@li DRDY = 1.1 / ext 2-5

Initializing the driver :
@code
typedef struct olimex_msp430_s olimex_msp430_t;

struct olimex_msp430_s {
	mp_kernel_t kernel;

	mp_drv_TMP006_t tmp006;

};

// [...]
{
	mp_options_t options[] = {
		{ "gate", "USCI_B3" },
		{ "sda", "p10.1" },
		{ "clk", "p10.2" },
		{ "drdy", "p1.1" },
		{ NULL, NULL }
	};

	mp_drv_TMP006_init(&olimex->kernel, &olimex->tmp006, options, "Ti TMP006");
}
@endcode

@{
*/

mp_sensor_t *mp_drv_TMP006_init(mp_kernel_t *kernel, mp_drv_TMP006_t *TMP006, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(TMP006, 0, sizeof(*TMP006));
	TMP006->kernel = kernel;

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		TMP006->drdy = mp_gpio_text_handle(value, "TMP006 DRDY");
		if(!TMP006->drdy) {
			mp_printk("TMP006: need a valid DRDY port");
			mp_drv_TMP006_fini(TMP006);
			return(NULL);
		}

		/* set CS high */
		mp_gpio_direction(TMP006->drdy, MP_GPIO_INPUT);
	}
	else
		TMP006->drdy = NULL;

	/* open spi */
	ret = mp_i2c_open(kernel, &TMP006->i2c, options, "TMP006");
	if(ret == FALSE)
		return(NULL);

	mp_options_t setup[] = {
		{ "frequency", "400000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&TMP006->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&TMP006->i2c);
		return(NULL);
	}

	/* set slave address */
	mp_i2c_setSlaveAddress(&TMP006->i2c, 0x40);

	/* enable chip */
	if(TMP006->drdy) {
		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(TMP006->drdy, _mp_drv_TMP006_onDRDY, TMP006, who);
		if(ret == FALSE) {
			mp_gpio_release(TMP006->drdy);
			return(NULL);
		}
		mp_gpio_interrupt_hi2lo(TMP006->drdy);
	}
	else {
		mp_printk("TMP006 require DRDY interrupt for the moment");
		mp_i2c_close(&TMP006->i2c);
		return(NULL);
	}

	/* create regmaster control */
	ret = mp_regMaster_init_i2c(kernel, &TMP006->regMaster,
			&TMP006->i2c, TMP006, "TMP006 I2C");
	if(ret == FALSE) {
		mp_printk("TMP006 error while creating regMaster context");
		mp_gpio_release(TMP006->drdy);
		mp_i2c_close(&TMP006->i2c);
		return(NULL);
	}

	mp_printk("TMP006(%p): Initializing", TMP006);

	mp_regMaster_readExt(
		&TMP006->regMaster,
		mp_regMaster_register(TMP006_REG_WRITE_REG), 1,
		(unsigned char *)&TMP006->settings, 2,
		_mp_drv_TMP006_onSettings, TMP006,
		TRUE
	);

	/* read manufacturer */
	mp_regMaster_readExt(
		&TMP006->regMaster,
		mp_regMaster_register(TMP006_REG_MAN_ID), 1,
		(unsigned char *)&TMP006->manufacturerId, 2,
		_mp_drv_TMP006_onManufacturerID, TMP006,
		TRUE
	);

	/* check for device id */
	mp_regMaster_readExt(
		&TMP006->regMaster,
		mp_regMaster_register(TMP006_REG_DEVICE_ID), 1,
		(unsigned char *)&TMP006->deviceId, 2,
		_mp_drv_TMP006_onDeviceID, TMP006,
		TRUE
	);

	/* create sensor */
	TMP006->sensor = mp_sensor_register(kernel, MP_SENSOR_TEMPERATURE, who);

	return(TMP006->sensor);
}

void mp_drv_TMP006_fini(mp_drv_TMP006_t *TMP006) {
	mp_printk("Unloading TMP006 driver");

	mp_regMaster_fini(&TMP006->regMaster);
}

/**
 * @brief Power down TMP006
 *
 * @param[in] TMP006 context
 */
void mp_drv_TMP006_sleep(mp_drv_TMP006_t *TMP006) {
	/* Power-down TMP006 */
	TMP006->settings &= ~(TMP006_CFG_MODEON);

	mp_drv_TMP006_write(
		TMP006, TMP006_REG_WRITE_REG,
		TMP006->settings
	);
}

/**
 * @brief Power up TMP006
 *
 * @param[in] TMP006 context
 */
void mp_drv_TMP006_wakeUp(mp_drv_TMP006_t *TMP006) {
	/* Power-up TMP006 */
	TMP006->settings |= TMP006_CFG_MODEON | TMP006_CFG_DRDYEN;

	mp_drv_TMP006_write(
		TMP006, TMP006_REG_WRITE_REG,
		TMP006->settings
	);
}

/**
 * @brief Change sample rate for TMP006
 *
 * @param[in] TMP006 context
 * @param[in] sample Sample rate possible value are :
 * @li @ref TMP006_CFG_1SAMPLE : conversion rate 4 per second
 * @li @ref TMP006_CFG_2SAMPLE : conversion rate 2 per second
 * @li @ref TMP006_CFG_4SAMPLE : conversion rate 1 per second
 * @li @ref TMP006_CFG_8SAMPLE : conversion rate 0.5 per second
 * @li @ref TMP006_CFG_16SAMPLE : conversion rate 0.25 per second
 */
void mp_drv_TMP006_sample(mp_drv_TMP006_t *TMP006, mp_drv_TMP006_sample_t sample) {
	/* Change TMP006 sample rate */
	TMP006->settings |= sample;

	mp_drv_TMP006_write(
		TMP006, TMP006_REG_WRITE_REG,
		TMP006->settings
	);
}



/**@}*/


static void mp_drv_TMP006_write(mp_drv_TMP006_t *TMP006, unsigned char address, unsigned short writeByte) {
	unsigned char *ptr = mp_mem_alloc(TMP006->kernel, 3);
	unsigned char *src = ptr;

	*(ptr++) = address;
	*(ptr++) = (unsigned char)(writeByte>>8);
	*(ptr++) = (unsigned char)(writeByte&0x0F);

	mp_regMaster_write(
		&TMP006->regMaster,
		src, 3,
		_mp_drv_TMP006_writeControl, TMP006
	);
}



static void _mp_drv_TMP006_onDRDY(void *user) {
	mp_drv_TMP006_t *TMP006 = user;

	/* read die T */
	mp_regMaster_readExt(
		&TMP006->regMaster,
		mp_regMaster_register(TMP006_REG_TABT), 1,
		(unsigned char *)&TMP006->rawDieTemperature, 2,
		_mp_drv_TMP006_onRawDieTemperature, TMP006,
		TRUE
	);

	/* read voltage */
	mp_regMaster_readExt(
		&TMP006->regMaster,
		mp_regMaster_register(TMP006_REG_VOBJ), 1,
		(unsigned char *)&TMP006->rawVoltage, 2,
		_mp_drv_TMP006_onRawVoltage, TMP006,
		TRUE
	);
}

static void _mp_drv_TMP006_onManufacturerID(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_TMP006_t *TMP006 = operand->user;

	if(TMP006->manufacturerId != 0x5449) {
		mp_printk("TMP006(%p): Got manufacturer ID 0x%x (bad), terminating", operand->user, TMP006->manufacturerId);
		mp_regMaster_fini(&TMP006->regMaster);
	}
	else
		mp_printk("TMP006(%p): Got manufacturer ID 0x%x (good)", operand->user, TMP006->manufacturerId);
}

static void _mp_drv_TMP006_onDeviceID(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_TMP006_t *TMP006 = operand->user;

	if(TMP006->deviceId != 0x67) {
		mp_printk("TMP006(%p): Got device ID 0x%x (bad), terminating", operand->user, TMP006->deviceId);
		mp_regMaster_fini(&TMP006->regMaster);
	}
	else {
		mp_printk("TMP006(%p): Got device ID 0x%x (good)", operand->user, TMP006->deviceId);

		/* start interrupt  */
		mp_drv_TMP006_write(
			TMP006, TMP006_REG_WRITE_REG,
			TMP006_CFG_MODEON + TMP006_CFG_DRDYEN
		);

		/* change default sample rate */
		mp_drv_TMP006_sample(TMP006, TMP006_CFG_8SAMPLE);
	}
}

static void _mp_drv_TMP006_onSettings(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_TMP006_t *TMP006 = operand->user;
	mp_printk("TMP006(%p): Initial settings is 0x%x", operand->user, TMP006->settings);
}

static void _mp_drv_TMP006_writeControl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_TMP006_t *TMP006 = operand->user;
	mp_mem_free(TMP006->kernel, operand->reg);
}



static void _mp_drv_TMP006_onRawDieTemperature(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_TMP006_t *TMP006 = operand->user;
	TMP006->rawDieTemperature >>= 2;
	mp_printk("TMP006(%p): Got RawDieTemperature %x", operand->user, TMP006->rawDieTemperature);
}

static void _mp_drv_TMP006_onRawVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_TMP006_t *TMP006 = operand->user;
	mp_printk("TMP006(%p): Got RawVoltage %x", operand->user, TMP006->rawVoltage);


	float Tdie = TMP006->rawDieTemperature;
	float Vobj = TMP006->rawVoltage;

	Vobj *= 156.25;  // 156.25 nV per LSB
	Vobj /= 1000; // nV -> uV
	Vobj /= 1000; // uV -> mV
	Vobj /= 1000; // mV -> V
	Tdie *= 0.03125; // convert to celsius
	Tdie += 273.15; // convert to kelvin

	float tdie_tref = Tdie - TMP006_TREF;
	float S = (1 + TMP006_A1*tdie_tref +
		 TMP006_A2*tdie_tref*tdie_tref);
	S *= TMP006_S0;
	S /= 10000000;
	S /= 10000000;

	float Vos = TMP006_B0 + TMP006_B1*tdie_tref +
		TMP006_B2*tdie_tref*tdie_tref;

	float fVobj = (Vobj - Vos) + TMP006_C2*(Vobj-Vos)*(Vobj-Vos);

	float Tobj = sqrt(sqrt(Tdie * Tdie * Tdie * Tdie + fVobj/S));

	Tobj -= 273.15; // Kelvin -> *C

	mp_printk("object temperature %f", Tobj);
}


#endif

