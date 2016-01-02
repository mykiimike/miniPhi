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

#ifdef SUPPORT_DRV_INA219

static void mp_drv_INA219_write(mp_drv_INA219_t *INA219, unsigned char address, unsigned short writeByte);

static void _mp_drv_INA219_onConfiguration(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_writeControl(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_busVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_shuntVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_INA219_current(mp_regMaster_op_t *operand, mp_bool_t terminate);

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
	//char *value;
	mp_ret_t ret;

	memset(INA219, 0, sizeof(*INA219));
	INA219->kernel = kernel;

	/* open spi */
	ret = mp_i2c_open(kernel, &INA219->i2c, options, "INA219");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
		{ "frequency", "1000000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&INA219->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&INA219->i2c);
		return(FALSE);
	}

	/* set slave address: make this changable */
	mp_i2c_setSlaveAddress(&INA219->i2c, 0x40);

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

	/* create sensor */
	INA219->busVoltage = mp_sensor_register(kernel, MP_SENSOR_VOLTAGE, "INA219: Bus");
	INA219->shuntVoltage = mp_sensor_register(kernel, MP_SENSOR_VOLTAGE, "INA219: Shunt");
	INA219->current = mp_sensor_register(kernel, MP_SENSOR_CURRENT, "INA219: Current");

	mp_printk("INA219(%p): Initializing", INA219);

	mp_regMaster_readExt(
		&INA219->regMaster,
		mp_regMaster_register(0), 1,
		(unsigned char *)&INA219->configuration, 2,
		_mp_drv_INA219_onConfiguration, INA219,
		TRUE
	);

	return(TRUE);
}

void mp_drv_INA219_fini(mp_drv_INA219_t *INA219) {
	mp_printk("Unloading INA219 driver");

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
	INA219->currentDivider = 10;  // Current LSB = 100uA per bit (1000/100 = 10)
	INA219->powerDivider = 2;     // Power LSB = 1mW per bit (2/1)

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
	INA219->currentDivider = 25;
	INA219->powerDivider = 1;

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
	INA219->currentDivider = 20;
	INA219->powerDivider = 1;

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

/**@}*/

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

	INA219->rawBusVoltage = ((INA219->rawBusVoltage >> 3) * 4);
	INA219->busVoltage->voltage.result = INA219->rawBusVoltage;
	mp_printk("_mp_drv_INA219_busVoltage %d", INA219->rawBusVoltage);

}

static void _mp_drv_INA219_shuntVoltage(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_INA219_t *INA219 = operand->user;
	INA219->shuntVoltage->voltage.result = INA219->rawShuntVoltage*0.01;
	mp_printk("_mp_drv_INA219_shuntVoltage: %f", INA219->shuntVoltage->voltage.result);
}

static void _mp_drv_INA219_current(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_INA219_t *INA219 = operand->user;
	INA219->current->current.result = INA219->rawCurrent*0.001;
	mp_printk("_mp_drv_INA219_current %f", INA219->current->current.result);
}




MP_TASK(_mp_drv_INA219_ASR) {
	mp_drv_INA219_t *INA219 = task->user;

	/* receive shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {

		mp_sensor_unregister(INA219->kernel, INA219->busVoltage);
		mp_sensor_unregister(INA219->kernel, INA219->shuntVoltage);
		mp_sensor_unregister(INA219->kernel, INA219->current);

		mp_regMaster_fini(&INA219->regMaster);

		/* acknowledging */
		mp_task_signal(task, MP_TASK_SIG_DEAD);
		return;
	}

/*
	mp_regMaster_readExt(
		&INA219->regMaster,
		mp_regMaster_register(0), 1,
		(unsigned char *)&INA219->configuration, 2,
		_mp_drv_INA219_onConfiguration, INA219,
		TRUE
	);

	mp_drv_INA219_update_busVoltage(INA219);
	mp_drv_INA219_update_shuntVoltage(INA219);
	mp_drv_INA219_update_current(INA219);
	mp_printk("pouf");
*/
}


#endif

