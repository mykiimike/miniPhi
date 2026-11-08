/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2016  Michael VERGOZ                                      *
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

#ifdef SUPPORT_DRV_INA219

static void mp_drv_INA219_write(mp_drv_INA219_t *INA219, unsigned char address, unsigned short writeByte);
static unsigned char _mp_drv_INA219_address(char *text);

static void _mp_drv_INA219_onConfiguration(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_writeControl(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_busVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_shuntVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_current(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_power(mp_regMaster_op_t *operand, mp_bool_t terminate);

MP_TASK(_mp_drv_INA219_ASR);

/**
@defgroup mpDriverTiINA219 Ti INA219

@ingroup mpDriver

@brief Ti INA219 Current sensor

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2016
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015


Configuration for INA219 example :
@li gate = USCI_B3
@li SDA = 10.1 / ext 1-17
@li SCL = 10.2 / ext 1-16

Initializing the driver :
@code
typedef struct olimex_msp430_s olimex_msp430_t;

struct olimex_msp430_s {
	mp_kernel_t kernel;

	mp_drv_INA219_t ina219;

};

// [...]
{
	mp_options_t options[] = {
		{ "gate", "USCI_B3" },
		{ "sda", "p10.1" },
		{ "clk", "p10.2" },
		{ NULL, NULL }
	};

	mp_drv_INA219_init(&olimex->kernel, &olimex->ina219, options, "Ti INA219");
}
@endcode

@{
*/

mp_ret_t mp_drv_INA219_init(mp_kernel_t *kernel, mp_drv_INA219_t *INA219, mp_options_t *options, char *who) {
	mp_ret_t ret;

	memset(INA219, 0, sizeof(*INA219));
	INA219->kernel = kernel;

	/* open i2c */
	ret = mp_i2c_open(kernel, &INA219->i2c, options, "INA219");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
		{ "frequency", "400000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&INA219->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&INA219->i2c);
		return(FALSE);
	}

	mp_i2c_setSlaveAddress(&INA219->i2c, _mp_drv_INA219_address(mp_options_get(options, "address")));

	/* create regmaster control */
	ret = mp_regMaster_init_i2c(kernel, &INA219->regMaster,
			&INA219->i2c, INA219, "INA219 I2C");
	if(ret == FALSE) {
		mp_printk("INA219 error while creating regMaster context");
		mp_i2c_close(&INA219->i2c);
		return(FALSE);
	}

	/* create ASR task */
	INA219->task = mp_task_create(&kernel->tasks, who, _mp_drv_INA219_ASR, INA219, 1000);
	if(!INA219->task) {
		mp_printk("INA219(%p) FATAL could not create ASR task", INA219);
		mp_drv_INA219_fini(INA219);
		return(FALSE);
	}

	{
		static const mp_device_driver_ops_t ops = {
			"sensor/ina219", "TI INA219", "1.0.0",
			(unsigned int)((1u << MP_DEVICE_VOLTAGE) | (1u << MP_DEVICE_CURRENT) | (1u << MP_DEVICE_POWER)),
			NULL, NULL, NULL, NULL, NULL
		};

		mp_device_driver_register(kernel, &INA219->binding, &ops, INA219, NULL);
		mp_device_registry_register(&INA219->binding, &INA219->busDev, MP_DEVICE_VOLTAGE, 0, &INA219->busVoltage, "ina219", "bus", NULL);
		mp_device_registry_register(&INA219->binding, &INA219->shuntDev, MP_DEVICE_VOLTAGE, 1, &INA219->shuntVoltage, "ina219", "shunt", NULL);
		mp_device_registry_register(&INA219->binding, &INA219->currDev, MP_DEVICE_CURRENT, 0, &INA219->current, "ina219", "i", NULL);
		mp_device_registry_register(&INA219->binding, &INA219->powerDev, MP_DEVICE_POWER, 0, &INA219->power, "ina219", "p", NULL);
		INA219->busDev.label = "TI INA219 bus";
		INA219->shuntDev.label = "TI INA219 shunt";
		INA219->currDev.label = "TI INA219";
		INA219->powerDev.label = "TI INA219";
	}

	mp_printk("INA219(%p): Initializing", INA219);

	mp_regMaster_readExt(
		&INA219->regMaster,
		mp_regMaster_register(0), 1,
		(unsigned char *)&INA219->configuration, 2,
		_mp_drv_INA219_onConfiguration, INA219,
		TRUE
	);

	/* current and power stay at 0 until the calibration register is written */
	mp_drv_INA219_setCalibration_32V_2A(INA219);
	return(TRUE);
}

void mp_drv_INA219_fini(mp_drv_INA219_t *INA219) {
	mp_printk("Unloading INA219 driver");

	mp_device_registry_unregister(&INA219->busDev);
	mp_device_registry_unregister(&INA219->shuntDev);
	mp_device_registry_unregister(&INA219->currDev);
	mp_device_registry_unregister(&INA219->powerDev);
	mp_device_driver_unregister(INA219->kernel, &INA219->binding);

	if(INA219->task)
		mp_task_destroy(INA219->task);
}

/**
 * @brief Set calibration to 32V and 2A
 *
 * @param[in] INA219 context
 */
void mp_drv_INA219_setCalibration_32V_2A(mp_drv_INA219_t *INA219) {

	INA219->calibrationVal = 4096;
	INA219->currentLsb = 0.0001f; /* 100 uA, shunt 0.1 ohm */
	INA219->powerLsb = 0.002f;    /* 20 * current LSB */

	/* Write calibration */
	mp_drv_INA219_write(
		INA219, INA219_REG_CALIBRATION,
		INA219->calibrationVal
	);

	unsigned short config = INA219_CONFIG_BVOLTAGERANGE_32V |
		INA219_CONFIG_GAIN_8_320MV |
		INA219_CONFIG_BADCRES_12BIT |
		INA219_CONFIG_SADCRES_12BIT_1S_532US |
		INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

	/* Write configuration */
	mp_drv_INA219_write(
		INA219, INA219_REG_CONFIG,
		config
	);

}

/**
 * @brief Set calibration to 32V and 1A
 *
 * @param[in] INA219 context
 */
void mp_drv_INA219_setCalibration_32V_1A(mp_drv_INA219_t *INA219) {

	INA219->calibrationVal = 10240;
	INA219->currentLsb = 0.00004f; /* 40 uA, shunt 0.1 ohm */
	INA219->powerLsb = 0.0008f;    /* 20 * current LSB */

	/* Write calibration */
	mp_drv_INA219_write(
		INA219, INA219_REG_CALIBRATION,
		INA219->calibrationVal
	);

	unsigned short config = INA219_CONFIG_BVOLTAGERANGE_32V |
		INA219_CONFIG_GAIN_8_320MV |
		INA219_CONFIG_BADCRES_12BIT |
		INA219_CONFIG_SADCRES_12BIT_1S_532US |
		INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

	/* Write configuration */
	mp_drv_INA219_write(
		INA219, INA219_REG_CONFIG,
		config
	);

}


/**
 * @brief Set calibration to 16V and 400mA
 *
 * @param[in] INA219 context
 */
void mp_drv_INA219_setCalibration_16V_400mA(mp_drv_INA219_t *INA219) {

	INA219->calibrationVal = 8192;
	INA219->currentLsb = 0.00005f; /* 50 uA, shunt 0.1 ohm */
	INA219->powerLsb = 0.001f;     /* 20 * current LSB */

	/* Write calibration */
	mp_drv_INA219_write(
		INA219, INA219_REG_CALIBRATION,
		INA219->calibrationVal
	);

	unsigned short config = INA219_CONFIG_BVOLTAGERANGE_16V |
		INA219_CONFIG_GAIN_1_40MV |
		INA219_CONFIG_BADCRES_12BIT |
		INA219_CONFIG_SADCRES_12BIT_1S_532US |
		INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

	/* Write configuration */
	mp_drv_INA219_write(
		INA219, INA219_REG_CONFIG,
		config
	);

}


/**
 * @brief Update bus voltage sensor
 *
 * @param[in] INA219 context
 */
void mp_drv_INA219_update_busVoltage(mp_drv_INA219_t *INA219) {
	mp_regMaster_readExt(
		&INA219->regMaster,
		mp_regMaster_register(INA219_REG_BUSVOLTAGE), 1,
		(unsigned char *)&INA219->rawBusVoltage, 2,
		_mp_drv_INA219_busVoltage, INA219,
		TRUE
	);
}

/**
 * @brief Update shunt voltage
 *
 * @param[in] INA219 context
 */
void mp_drv_INA219_update_shuntVoltage(mp_drv_INA219_t *INA219) {
	mp_regMaster_readExt(
		&INA219->regMaster,
		mp_regMaster_register(INA219_REG_SHUNTVOLTAGE), 1,
		(unsigned char *)&INA219->rawShuntVoltage, 2,
		_mp_drv_INA219_shuntVoltage, INA219,
		TRUE
	);
}

/**
 * @brief Update current
 *
 * @param[in] INA219 context
 */
void mp_drv_INA219_update_current(mp_drv_INA219_t *INA219) {
	mp_regMaster_readExt(
		&INA219->regMaster,
		mp_regMaster_register(INA219_REG_CURRENT), 1,
		(unsigned char *)&INA219->rawCurrent, 2,
		_mp_drv_INA219_current, INA219,
		TRUE
	);
}

/**
 * @brief Update power
 *
 * powerLsb is watts per bit, written by the calibration.
 *
 * @param[in] INA219 context
 */
void mp_drv_INA219_update_power(mp_drv_INA219_t *INA219) {
	mp_regMaster_readExt(
		&INA219->regMaster,
		mp_regMaster_register(INA219_REG_POWER), 1,
		(unsigned char *)&INA219->rawPower, 2,
		_mp_drv_INA219_power, INA219,
		TRUE
	);
}

/**@}*/

/* A1/A0 select 0x40..0x4F. Missing or out of range stays at 0x40. */
static unsigned char _mp_drv_INA219_address(char *text) {
	unsigned int value = 0;

	if(!text || !*text)
		return(INA219_ADDRESS);
	if(text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		text += 2;
	while(*text) {
		char c = *text++;

		value <<= 4;
		if(c >= '0' && c <= '9')
			value += (unsigned int)(c - '0');
		else if(c >= 'a' && c <= 'f')
			value += (unsigned int)(c - 'a' + 10);
		else if(c >= 'A' && c <= 'F')
			value += (unsigned int)(c - 'A' + 10);
		else
			return(INA219_ADDRESS);
	}
	if(value < 0x40 || value > 0x4F)
		return(INA219_ADDRESS);
	return((unsigned char)value);
}

static void mp_drv_INA219_write(mp_drv_INA219_t *INA219, unsigned char address, unsigned short writeByte) {
	unsigned char *ptr = mp_mem_alloc(INA219->kernel, 3);
	unsigned char *src = ptr;

	*(ptr++) = address;
	*(ptr++) = (unsigned char)(writeByte >> 8);
	*(ptr++) = (unsigned char)(writeByte & 0xFF);

	mp_regMaster_write(
		&INA219->regMaster,
		src, 3,
		_mp_drv_INA219_writeControl, INA219
	);

}

static void _mp_drv_INA219_onConfiguration(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_INA219_t *INA219 = operand->user;
	mp_printk("INA219(%p) Initial configuration set to 0x%x", INA219, INA219->configuration);
}


static void _mp_drv_INA219_writeControl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_printk("Data written");

}

static void _mp_drv_INA219_busVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_INA219_t *INA219 = operand->user;
	unsigned short raw = INA219->rawBusVoltage;

	/* bit 0 is the math overflow; the voltage sits above the three low bits, 4 mV per step */
	INA219->overflow = (raw & 0x0001) ? YES : NO;
	INA219->rawBusVoltage = (raw >> 3) * 4;
	mp_device_measure_publish(&INA219->busVoltage, (float)INA219->rawBusVoltage / 1000.0f);

}

static void _mp_drv_INA219_shuntVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_INA219_t *INA219 = operand->user;
	/* signed, 10 uV per step */
	mp_device_measure_publish(&INA219->shuntVoltage, (float)(int16_t)INA219->rawShuntVoltage * 0.00001f);
}

static void _mp_drv_INA219_current(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_INA219_t *INA219 = operand->user;

	/* signed; the overflow flag comes from the bus read queued just before */
	if(INA219->overflow || INA219->currentLsb == 0.0f)
		return;
	mp_device_measure_publish(&INA219->current, (float)(int16_t)INA219->rawCurrent * INA219->currentLsb);
}

static void _mp_drv_INA219_power(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_INA219_t *INA219 = operand->user;

	/* unsigned; same overflow flag as the current */
	if(INA219->overflow || INA219->powerLsb == 0.0f)
		return;
	mp_device_measure_publish(&INA219->power, (float)INA219->rawPower * INA219->powerLsb);
}




MP_TASK(_mp_drv_INA219_ASR) {
	mp_drv_INA219_t *INA219 = task->user;

	/* receive shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {

		mp_device_registry_unregister(&INA219->busDev);
		mp_device_registry_unregister(&INA219->shuntDev);
		mp_device_registry_unregister(&INA219->currDev);
		mp_device_registry_unregister(&INA219->powerDev);
		mp_device_driver_unregister(INA219->kernel, &INA219->binding);

		mp_regMaster_fini(&INA219->regMaster);

		/* acknowledging */
		mp_task_signal(task, MP_TASK_SIG_DEAD);
		return;
	}


	mp_drv_INA219_update_busVoltage(INA219);
	mp_drv_INA219_update_shuntVoltage(INA219);
	mp_drv_INA219_update_current(INA219);
	mp_drv_INA219_update_power(INA219);
}


#endif

