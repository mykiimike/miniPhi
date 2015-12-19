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
 * Inc., 51 Franklin STreet, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Some parts of this source has been ported from Kris Winer code's.
 * https://github.com/kriswiner/LSM9DS0
 */

#include <mp.h>

#ifdef SUPPORT_DRV_LSM9DS0

static void _mp_drv_LSM9DS0_onCSGWhoIAm(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onXMWhoIAm(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onWrite(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onGyroRead(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onGyroCalibrationRead(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onMagRead(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onTemperatureRead(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onAccelRead(mp_regMaster_op_t *operand, mp_bool_t terminate);
static void _mp_drv_LSM9DS0_onAccelCalibrationRead(mp_regMaster_op_t *operand, mp_bool_t terminate);

static void _mp_drv_LSM9DS0_onDRDY(void *user);
static void _mp_drv_LSM9DS0_onIntMag(void *user);
static void _mp_drv_LSM9DS0_onIntAcc(void *user);

MP_TASK(_mp_drv_LSM9DS0_ASR);

static void _mp_drv_LSM9DS0_calcgRes(mp_drv_LSM9DS0_t *LSM9DS0);
static void _mp_drv_LSM9DS0_calcaRes(mp_drv_LSM9DS0_t *LSM9DS0);
static void _mp_drv_LSM9DS0_calcmRes(mp_drv_LSM9DS0_t *LSM9DS0);

/**
@defgroup mpDriverSTLSM9DS0 ST LSM9DS0

@ingroup mpDriver

@brief ST LSM9DS0 3-axis Accelerometer / Gyroscope / Magneto with temperature bonus

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

@{
*/
/**
 * @brief Initiate LSM9DS0 context
 *
 * @param[in] kernel Kernel context
 * @param[in] LSM9DS0 Context
 * @param[in] options Special options :
 *  @li protocol : i2c or spi (case sensitive)
 *  @li csG : When using SPI the Gyro CS
 *  @li csXM : When using SPI the Acce/Mag CS
 *  @li gate : Required on some archs
 *  @li sda : SDA port when using I2C
 *  @li clk : Clock port source I2C/SPI
 *  @li simo : SPI Slave In Master Out
 *  @li somi : SPI Slave Out Master In
 *  @li int1 : INT1 port for accelerometer
 *  @li int2 : INT2 port for magneto
 *  @li drdy : DRDY port for gyro
 * @param[in] who Who own the driver session
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_LSM9DS0_init(mp_kernel_t *kernel, mp_drv_LSM9DS0_t *LSM9DS0, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(LSM9DS0, 0, sizeof(*LSM9DS0));
	LSM9DS0->kernel = kernel;

	/* protocol type */
	value = mp_options_get(options, "protocol");
	if(!value) {
		LSM9DS0->protocol = MP_DRV_LSM9DS0_MODE_SPI;
		mp_printk("LSM9DS0(%p) using SPI default protocol", LSM9DS0);
	}
	else {
		if(mp_options_cmp(value, "i2c")) {
			LSM9DS0->protocol = MP_DRV_LSM9DS0_MODE_I2C;
			mp_printk("LSM9DS0(%p) using i2c protocol", LSM9DS0);
		}
		else {
			LSM9DS0->protocol = MP_DRV_LSM9DS0_MODE_SPI;
			mp_printk("LSM9DS0(%p) using SPI protocol", LSM9DS0);
		}
	}

	/* using SPI protocol */
	if(LSM9DS0->protocol == MP_DRV_LSM9DS0_MODE_SPI) {

		/* csG */
		value = mp_options_get(options, "csG");
		if(!value) {
			mp_printk("LSM9DS0: need csG port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		LSM9DS0->csG = mp_gpio_text_handle(value, "LSM9DS0 csG");
		if(!LSM9DS0->csG) {
			mp_printk("LSM9DS0: need csG port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}

		/* csXM */
		value = mp_options_get(options, "csXM");
		if(!value) {
			mp_printk("LSM9DS0: need csXM port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		LSM9DS0->csXM = mp_gpio_text_handle(value, "LSM9DS0 csXM");
		if(!LSM9DS0->csXM) {
			mp_printk("LSM9DS0: need csXM port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}

		/* set CS high */
		mp_gpio_direction(LSM9DS0->csG, MP_GPIO_OUTPUT);
		mp_gpio_direction(LSM9DS0->csXM, MP_GPIO_OUTPUT);
		mp_gpio_set(LSM9DS0->csG);
		mp_gpio_set(LSM9DS0->csXM);

		/* open spi */
		ret = mp_spi_open(kernel, &LSM9DS0->spi, options, "LSM9DS0");
		if(ret == FALSE) {
			mp_printk("Error openning SPI interface");
			mp_drv_LSM9DS0_fini(LSM9DS0);
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
		ret = mp_spi_setup(&LSM9DS0->spi, setup);
		if(ret == FALSE) {
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}

		LSM9DS0->init = 1;

		/* create regmaster control */
		ret = mp_regMaster_init_spi(kernel, &LSM9DS0->regMaster,
				&LSM9DS0->spi, LSM9DS0, "LSM9DS0 SPI");
		if(ret == FALSE) {
			mp_printk("LSM9DS0 error while creating regMaster context");
			mp_drv_LSM9DS0_fini(LSM9DS0);

			return(FALSE);
		}

		LSM9DS0->init = 2;
	}
	/* using i2c */
	else {
		/* open spi */
		ret = mp_i2c_open(kernel, &LSM9DS0->i2c, options, "LSM9DS0");
		if(ret == FALSE) {
			mp_printk("Error openning i2c interface");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}

		mp_options_t setup[] = {
			{ "frequency", "400000" },
			{ "role", "master" },
			{ NULL, NULL }
		};
		ret = mp_i2c_setup(&LSM9DS0->i2c, setup);
		if(ret == FALSE) {
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}

		LSM9DS0->init = 1;

		/* set slave address */
		mp_i2c_setSlaveAddress(&LSM9DS0->i2c, LSM9DS0_ADDRESS_ACCELMAG);

		/* create regmaster control */
		ret = mp_regMaster_init_i2c(kernel, &LSM9DS0->regMaster,
				&LSM9DS0->i2c, LSM9DS0, "LSM9DS0 I2C");
		if(ret == FALSE) {
			mp_printk("LSM9DS0 error while creating regMaster context");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}

		LSM9DS0->init = 2;

	}
	/* int1 */
	value = mp_options_get(options, "int1");
	if(value) {
		LSM9DS0->int1 = mp_gpio_text_handle(value, "LSM9DS0 int1");
		if(!LSM9DS0->int1) {
			mp_printk("LSM9DS0: need int1 port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		mp_gpio_direction(LSM9DS0->int1, MP_GPIO_INPUT);

		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(LSM9DS0->int1, _mp_drv_LSM9DS0_onIntAcc, LSM9DS0, who);
		if(ret == FALSE) {
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		mp_gpio_interrupt_lo2hi(LSM9DS0->int1);
		mp_gpio_interrupt_enable(LSM9DS0->int1);
	}

	/* int2 */
	value = mp_options_get(options, "int2");
	if(value) {
		LSM9DS0->int2 = mp_gpio_text_handle(value, "LSM9DS0 int2");
		if(!LSM9DS0->int2) {
			mp_printk("LSM9DS0: need int2 port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		mp_gpio_direction(LSM9DS0->int2, MP_GPIO_INPUT);

		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(LSM9DS0->int2, _mp_drv_LSM9DS0_onIntMag, LSM9DS0, who);
		if(ret == FALSE) {
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		mp_gpio_interrupt_lo2hi(LSM9DS0->int2);
		mp_gpio_interrupt_enable(LSM9DS0->int2);

	}

	/* intG */
	value = mp_options_get(options, "intG");
	if(value) {
		LSM9DS0->intG = mp_gpio_text_handle(value, "LSM9DS0 intG");
		if(!LSM9DS0->intG) {
			mp_printk("LSM9DS0: need intG port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		mp_gpio_direction(LSM9DS0->intG, MP_GPIO_INPUT);
	}

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		LSM9DS0->drdy = mp_gpio_text_handle(value, "LSM9DS0 drdy");
		if(!LSM9DS0->drdy) {
			mp_printk("LSM9DS0: need drdy port");
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		mp_gpio_direction(LSM9DS0->drdy, MP_GPIO_INPUT);

		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(LSM9DS0->drdy, _mp_drv_LSM9DS0_onDRDY, LSM9DS0, who);
		if(ret == FALSE) {
			mp_drv_LSM9DS0_fini(LSM9DS0);
			return(FALSE);
		}
		mp_gpio_interrupt_lo2hi(LSM9DS0->drdy);
		mp_gpio_interrupt_enable(LSM9DS0->drdy);
	}


	/* create ASR task */
	LSM9DS0->task = mp_task_create(&kernel->tasks, who, _mp_drv_LSM9DS0_ASR, LSM9DS0, 100);
	if(!LSM9DS0->task) {
		mp_printk("LSM9DS0(%p) FATAL could not create ASR task", LSM9DS0);
		mp_drv_LSM9DS0_fini(LSM9DS0);
		return(FALSE);
	}

	mp_printk("LSM9DS0(%p) driver initialization in memory structure size of %d bytes", LSM9DS0, sizeof(*LSM9DS0));

	mp_clock_delay(100);

	/* check for device id */

	mp_drv_LSM9DS0_xmRead(
		LSM9DS0, WHO_AM_I_XM,
		(unsigned char *)&LSM9DS0->buffer, 1,
		_mp_drv_LSM9DS0_onXMWhoIAm
	);
	mp_drv_LSM9DS0_gRead(
		LSM9DS0, WHO_AM_I_G,
		(unsigned char *)&LSM9DS0->buffer, 1,
		_mp_drv_LSM9DS0_onCSGWhoIAm
	);
	/*
	// Gyro initialization stuff:
	mp_drv_LSM9DS0_initGyro(LSM9DS0); // This will "turn on" the gyro. Setting up interrupts, etc.
	mp_drv_LSM9DS0_setGyroODR(LSM9DS0, G_ODR_95_BW_125); // Set the gyro output data rate and bandwidth.
	mp_drv_LSM9DS0_setGyroScale(LSM9DS0, G_SCALE_500DPS); // Set the gyro range

	// Accelerometer initialization stuff:
	mp_drv_LSM9DS0_initAccel(LSM9DS0); // "Turn on" all axes of the accel. Set up interrupts, etc.
	mp_drv_LSM9DS0_setAccelODR(LSM9DS0, A_ODR_50); // Set the accel data rate.
	mp_drv_LSM9DS0_setAccelScale(LSM9DS0, A_SCALE_2G); // Set the accel range.

	// Magnetometer initialization stuff:
	mp_drv_LSM9DS0_initMag(LSM9DS0); // "Turn on" all axes of the mag. Set up interrupts, etc.
	mp_drv_LSM9DS0_setMagODR(LSM9DS0, M_ODR_125); // Set the magnetometer output data rate.
	mp_drv_LSM9DS0_setMagScale(LSM9DS0, M_SCALE_12GS); // Set the magnetometer's range.
	*/

	return(TRUE);
}

/**
 * @brief Terminate LSM9DS0 context
 *
 * @param[in] LSM9DS0 Context
 * @return TRUE or FALSE
 */
mp_ret_t mp_drv_LSM9DS0_fini(mp_drv_LSM9DS0_t *LSM9DS0) {

	if(LSM9DS0->csG)
		mp_gpio_release(LSM9DS0->csG);

	if(LSM9DS0->csXM)
		mp_gpio_release(LSM9DS0->csXM);

	if(LSM9DS0->int1)
		mp_gpio_release(LSM9DS0->int1);

	if(LSM9DS0->int2)
		mp_gpio_release(LSM9DS0->int2);

	if(LSM9DS0->intG)
		mp_gpio_release(LSM9DS0->intG);

	if(LSM9DS0->drdy)
		mp_gpio_release(LSM9DS0->drdy);

	if(LSM9DS0->task)
		mp_task_destroy(LSM9DS0->task);

	if(LSM9DS0->init >= 2)
		mp_regMaster_fini(&LSM9DS0->regMaster);

	if(LSM9DS0->init >= 1) {
		if(LSM9DS0->protocol == MP_DRV_LSM9DS0_MODE_SPI)
			mp_spi_close(&LSM9DS0->spi);
		else
			mp_i2c_close(&LSM9DS0->i2c);
	}

	mp_printk("Stopping LSM9DS0");
	return(TRUE);
}

/**
 * @brief Read on accelerometer / magneto
 *
 * This function prepare all the states to read bytes from
 * accelerometer and magneto.
 *
 * @param[in] LSM9DS0 Context
 * @param[in] reg LSM9DS0 register
 * @param[in] wait Buffer to fill
 * @param[in] waitSize Size of buffer to fill
 * @param[in] callback Executed regMaster callback
 */
void mp_drv_LSM9DS0_xmRead(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char *wait, int waitSize,
		mp_regMaster_cb_t callback
	) {
	unsigned char *subReg = NULL;

	if(LSM9DS0->protocol == MP_DRV_LSM9DS0_MODE_I2C) {
		mp_regMaster_setSlaveAddress(&LSM9DS0->regMaster, LSM9DS0_ADDRESS_ACCELMAG);
		subReg = mp_regMaster_register(reg);
	}
	else {
		mp_regMaster_setChipSelect(&LSM9DS0->regMaster, LSM9DS0->csXM);

		if(waitSize > 1)
			subReg = mp_regMaster_register(0xc0 | (reg & 0x3f)); /* one byte operation */
		else
			subReg = mp_regMaster_register(0x80 | (reg & 0x3f)); /* multi bytes operation */
	}

	mp_regMaster_read(
		&LSM9DS0->regMaster,
		subReg, 1,
		wait, waitSize,
		callback, LSM9DS0
	);
}

/**
 * @brief Write on accelerometer / magneto
 *
 * This function prepare all the states to write bytes from
 * the accelerometer / magneto
 *
 * @param[in] LSM9DS0 Context
 * @param[in] reg LSM9DS0 register
 * @param[in] reg Register and data to write
 * @param[in] regSize Size of the register payload
 * @param[in] callback Executed regMaster callback
 */
void mp_drv_LSM9DS0_xmWrite(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char value
	) {
	unsigned char *subReg = NULL;
	unsigned char *ptr = mp_mem_alloc(LSM9DS0->kernel, 2);
	unsigned char *src = ptr;

	if(LSM9DS0->protocol == MP_DRV_LSM9DS0_MODE_I2C) {
		mp_regMaster_setSlaveAddress(&LSM9DS0->regMaster, LSM9DS0_ADDRESS_ACCELMAG);
		subReg = mp_regMaster_register(reg);
	}
	else {
		mp_regMaster_setChipSelect(&LSM9DS0->regMaster, LSM9DS0->csXM);
		subReg = mp_regMaster_register(reg & 0x3f);
	}

	*(ptr++) = *subReg;
	*ptr = value;

	mp_regMaster_write(
		&LSM9DS0->regMaster,
		src, 2,
		_mp_drv_LSM9DS0_onWrite, LSM9DS0
	);
}

/**
 * @brief Read on gyroscope
 *
 * This function prepare all the states to read bytes from
 * the gyroscope
 *
 * @param[in] LSM9DS0 Context
 * @param[in] reg LSM9DS0 register
 * @param[in] wait Buffer to fill
 * @param[in] waitSize Size of buffer to fill
 * @param[in] callback Executed regMaster callback
 */
void mp_drv_LSM9DS0_gRead(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char *wait, int waitSize,
		mp_regMaster_cb_t callback
	) {
	unsigned char *subReg = NULL;

	if(LSM9DS0->protocol == MP_DRV_LSM9DS0_MODE_I2C) {
		mp_regMaster_setSlaveAddress(&LSM9DS0->regMaster, LSM9DS0_ADDRESS_GYRO);
		subReg = mp_regMaster_register(reg);
	}
	else {
		mp_regMaster_setChipSelect(&LSM9DS0->regMaster, LSM9DS0->csG);

		if(waitSize > 1)
			subReg = mp_regMaster_register(0xc0 | (reg & 0x3f)); /* one byte operation */
		else
			subReg = mp_regMaster_register(0x80 | (reg & 0x3f)); /* multi bytes operation */
	}

	mp_regMaster_read(
		&LSM9DS0->regMaster,
		subReg, 1,
		wait, waitSize,
		callback, LSM9DS0
	);
}

/**
 * @brief Write on gyroscope
 *
 * This function prepare all the states to write bytes from
 * the gyroscope
 *
 * @param[in] LSM9DS0 Context
 * @param[in] reg LSM9DS0 register
 * @param[in] reg Register and data to write
 * @param[in] regSize Size of the register payload
 * @param[in] callback Executed regMaster callback
 */
void mp_drv_LSM9DS0_gWrite(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char value
	) {
	unsigned char *subReg = NULL;
	unsigned char *ptr = mp_mem_alloc(LSM9DS0->kernel, 2);
	unsigned char *src = ptr;

	if(LSM9DS0->protocol == MP_DRV_LSM9DS0_MODE_I2C) {
		mp_regMaster_setSlaveAddress(&LSM9DS0->regMaster, LSM9DS0_ADDRESS_GYRO);
		subReg = mp_regMaster_register(reg);
	}
	else {
		mp_regMaster_setChipSelect(&LSM9DS0->regMaster, LSM9DS0->csG);
		subReg = mp_regMaster_register(reg & 0x3f);
	}

	*(ptr++) = *subReg;
	*ptr = value;

	mp_regMaster_write(
		&LSM9DS0->regMaster,
		src, 2,
		_mp_drv_LSM9DS0_onWrite, LSM9DS0
	);
}

void mp_drv_LSM9DS0_initGyro(mp_drv_LSM9DS0_t *LSM9DS0) {

	/* Register new gyro sensor */
	LSM9DS0->gyro = mp_sensor_register(LSM9DS0->kernel, MP_SENSOR_3AXIS, "LSM9DS0 GYRO");

	/* prepare gyro registers */
	LSM9DS0->gReg1 = 0x0f;
	LSM9DS0->gReg4 = 0x00;

	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG1_G, LSM9DS0->gReg1); // Normal mode, enable all axes
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG2_G, 0x30);
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG3_G, 0x88);
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG4_G, LSM9DS0->gReg4); // Set scale to 245 dps
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG5_G, 0x10);
}

void mp_drv_LSM9DS0_finiGyro(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG1_G, 0x00);
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG2_G, 0x00);
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG3_G, 0x00);
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG4_G, 0x00);
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG5_G, 0x00);

	if(LSM9DS0->gyro)
		mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->gyro);
}

void mp_drv_LSM9DS0_setGyroScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_gyro_scale_t gScl) {
	// Then mask out the gyro scale bits:
	LSM9DS0->gReg4 &= 0xFF^(0x3 << 4);

	// Then shift in our new scale bits:
	LSM9DS0->gReg4 |= gScl << 4;

	// And write the new register value back into CTRL_REG4_G:
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG4_G, LSM9DS0->gReg4);

	// We've updated the sensor, but we also need to update our class variables
	// First update gScale:
	LSM9DS0->gyro_scale = gScl;

	// Then calculate a new gRes, which relies on gScale being set correctly:
	_mp_drv_LSM9DS0_calcgRes(LSM9DS0);

	/* run calibration */
	LSM9DS0->gyroCal = 1;
}

void mp_drv_LSM9DS0_setGyroODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_gyro_odr_t gRate) {
	// Then mask out the gyro ODR bits:
	LSM9DS0->gReg1 &= 0xFF^(0xF << 4);

	// Then shift in our new ODR bits:
	LSM9DS0->gReg1 |= (gRate << 4);

	// And write the new register value back into CTRL_REG1_G:
	mp_drv_LSM9DS0_gWrite(LSM9DS0, CTRL_REG1_G, LSM9DS0->gReg1);
}

void mp_drv_LSM9DS0_initMag(mp_drv_LSM9DS0_t *LSM9DS0) {
	/* Register new magneto sensor */
	LSM9DS0->magneto = mp_sensor_register(LSM9DS0->kernel, MP_SENSOR_3AXIS, "LSM9DS0 MAGNETO");
	LSM9DS0->temperature = mp_sensor_register(LSM9DS0->kernel, MP_SENSOR_TEMPERATURE, "LSM9DS0 TEMP");

	/* prepare magneto registers */
	LSM9DS0->xmReg5 = 0x94;
	LSM9DS0->xmReg6 = 0x00;

	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG7_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG5_XM, LSM9DS0->xmReg5);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG6_XM, LSM9DS0->xmReg6);

	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG4_XM, 0x04);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, INT_CTRL_REG_M, 0x09);
}

void mp_drv_LSM9DS0_finiMag(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG5_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG6_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG7_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG4_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, INT_CTRL_REG_M, 0x00);

	if(LSM9DS0->magneto)
		mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->magneto);
	if(LSM9DS0->temperature)
		mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->temperature);
}

void mp_drv_LSM9DS0_setMagScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_mag_scale_t mScl) {
	// We need to preserve the other bytes in CTRL_REG6_XM. So, first read it:
	unsigned char temp = LSM9DS0->xmReg6;

	// Then mask out the mag scale bits:
	temp &= 0xFF^(0x3 << 5);

	// Then shift in our new scale bits:
	temp |= mScl << 5;

	LSM9DS0->xmReg6 = temp;

	// And write the new register value back into CTRL_REG6_XM:
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG6_XM, LSM9DS0->xmReg6);

	// We've updated the sensor, but we also need to update our class variables
	// First update mScale:
	LSM9DS0->mag_scale = mScl;

	// Then calculate a new mRes, which relies on mScale being set correctly:
	_mp_drv_LSM9DS0_calcmRes(LSM9DS0);
}

void mp_drv_LSM9DS0_setMagODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_mag_odr_t mRate) {
	// We need to preserve the other bytes in CTRL_REG5_XM. So, first read it:
	unsigned char temp = LSM9DS0->xmReg5;

	// Then mask out the mag ODR bits:
	temp &= 0xFF^(0x7 << 2);

	// Then shift in our new ODR bits:
	temp |= (mRate << 2);

	LSM9DS0->xmReg5 = temp;

	// And write the new register value back into CTRL_REG5_XM:
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG5_XM, LSM9DS0->xmReg5);
}

void mp_drv_LSM9DS0_initAccel(mp_drv_LSM9DS0_t *LSM9DS0) {
	/* Register new accelerometer sensor */
	LSM9DS0->accelero = mp_sensor_register(LSM9DS0->kernel, MP_SENSOR_3AXIS, "LSM9DS0 ACCEL");

	/* prepare accelerometer registers */
	LSM9DS0->xmReg1 = 0x57;
	LSM9DS0->xmReg2 = 0x00;

	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG0_XM, 0x02);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG1_XM, LSM9DS0->xmReg1); // 100Hz data rate, x/y/z all enabled
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG2_XM, LSM9DS0->xmReg2); // Set scale to 2g
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG3_XM, 0x04);
}

void mp_drv_LSM9DS0_finiAccel(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG0_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG1_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG2_XM, 0x00);
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG3_XM, 0x00);

	if(LSM9DS0->accelero)
		mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->accelero);
	LSM9DS0->accelero = NULL;
}

void mp_drv_LSM9DS0_setAccelScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_scale_t aScl, mp_bool_t calibrate) {
	// We need to preserve the other bytes in CTRL_REG2_XM. So, first read it:
	unsigned char temp = LSM9DS0->xmReg2;

	// Then mask out the accel scale bits:
	temp &= 0xFF^(0x3 << 3);

	// Then shift in our new scale bits:
	temp |= aScl << 3;
	LSM9DS0->xmReg2 = temp;

	// And write the new register value back into CTRL_REG2_XM:
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG2_XM, LSM9DS0->xmReg2);

	// We've updated the sensor, but we also need to update our class variables
	// First update aScale:
	LSM9DS0->accel_scale = aScl;

	// Then calculate a new aRes, which relies on aScale being set correctly:
	_mp_drv_LSM9DS0_calcaRes(LSM9DS0);

	if(calibrate == TRUE)
		LSM9DS0->accelCal = 1;
	else
		LSM9DS0->accelCal = 0;
}

void mp_drv_LSM9DS0_setAccelODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_odr_t aRate) {
	// We need to preserve the other bytes in CTRL_REG1_XM. So, first read it:
	unsigned char temp = LSM9DS0->xmReg1;

	// Then mask out the accel ODR bits:
	temp &= 0xFF^(0xF << 4);

	// Then shift in our new ODR bits:
	temp |= (aRate << 4);
	LSM9DS0->xmReg1 = temp;

	// And write the new register value back into CTRL_REG1_XM:
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG1_XM, LSM9DS0->xmReg1);
}

void mp_drv_LSM9DS0_setAccelABW(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_abw_t abwRate) {
	// We need to preserve the other bytes in CTRL_REG2_XM. So, first read it:
	unsigned char temp = LSM9DS0->xmReg2;

	// Then mask out the accel ABW bits:
	temp &= 0xFF^(0x3 << 7);

	// Then shift in our new ODR bits:
	temp |= (abwRate << 7);
	LSM9DS0->xmReg2 = temp;

	// And write the new register value back into CTRL_REG2_XM:
	mp_drv_LSM9DS0_xmWrite(LSM9DS0, CTRL_REG2_XM, LSM9DS0->xmReg2);

	LSM9DS0->accelCal = 1;
}

/**@}*/

static void _mp_drv_LSM9DS0_calcgRes(mp_drv_LSM9DS0_t *LSM9DS0) {
	// Possible gyro scales (and their register bit settings) are:
	// 245 DPS (00), 500 DPS (01), 2000 DPS (10). Here's a bit of an algorithm
	// to calculate DPS/(ADC tick) based on that 2-bit value:
	switch (LSM9DS0->gyro_scale) {
		case G_SCALE_245DPS:
			LSM9DS0->gRes = 245.0 / 32768.0;
			break;

		case G_SCALE_500DPS:
			LSM9DS0->gRes = 500.0 / 32768.0;
			break;

		case G_SCALE_2000DPS:
			LSM9DS0->gRes = 2000.0 / 32768.0;
			break;
	}
}

static void _mp_drv_LSM9DS0_calcaRes(mp_drv_LSM9DS0_t *LSM9DS0) {
	// Possible accelerometer scales (and their register bit settings) are:
	// 2 g (000), 4g (001), 6g (010) 8g (011), 16g (100). Here's a bit of an
	// algorithm to calculate g/(ADC tick) based on that 3-bit value:
	LSM9DS0->aRes = LSM9DS0->accel_scale == A_SCALE_16G ? 16.0 / 32768.0 :
		(((float) LSM9DS0->accel_scale + 1.0) * 2.0) / 32768.0;
}

static void _mp_drv_LSM9DS0_calcmRes(mp_drv_LSM9DS0_t *LSM9DS0) {
	// Possible magnetometer scales (and their register bit settings) are:
	// 2 Gs (00), 4 Gs (01), 8 Gs (10) 12 Gs (11). Here's a bit of an algorithm
	// to calculate Gs/(ADC tick) based on that 2-bit value:
	LSM9DS0->mRes = LSM9DS0->mag_scale == M_SCALE_2GS ? 2.0 / 32768.0 :
		(float) (LSM9DS0->mag_scale << 2) / 32768.0;
}

static void _mp_drv_LSM9DS0_onCSGWhoIAm(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;

	if(LSM9DS0->buffer[0] == 0xd4)
		mp_printk("LSM9DS0(%p) Got a valid Gyro iam %x", LSM9DS0, LSM9DS0->buffer[0]);
	else
		mp_printk("LSM9DS0(%p) WARNING Got a invalid Gyro iam %x", LSM9DS0, LSM9DS0->buffer[0]);
}

static void _mp_drv_LSM9DS0_onXMWhoIAm(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;
	if(LSM9DS0->buffer[0] == 0x49)
		mp_printk("LSM9DS0(%p) Got a valid XM iam %x", LSM9DS0, LSM9DS0->buffer[0]);
	else
		mp_printk("LSM9DS0(%p) WARNING Got a invalid XM iam %x", LSM9DS0, LSM9DS0->buffer[0]);

}

static void _mp_drv_LSM9DS0_onWrite(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;
	mp_printk("LSM9DS0(%p) register write 0x%x = 0x%x using address 0x%x", LSM9DS0, operand->reg[0], operand->reg[1], operand->slaveAddress);
	mp_mem_free(LSM9DS0->kernel, operand->reg);
}


//int init = 0;
static void _mp_drv_LSM9DS0_onGyroRead(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;

	LSM9DS0->gyro->axis3.x = (LSM9DS0->gRes*(double)((LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0]))-LSM9DS0->gbias[0];
	LSM9DS0->gyro->axis3.y = (LSM9DS0->gRes*(double)((LSM9DS0->buffer[3] << 8) | LSM9DS0->buffer[2]))-LSM9DS0->gbias[1];
	LSM9DS0->gyro->axis3.z = (LSM9DS0->gRes*(double)((LSM9DS0->buffer[5] << 8) | LSM9DS0->buffer[4]))-LSM9DS0->gbias[2];

	if(LSM9DS0->onGyroData)
		LSM9DS0->onGyroData(LSM9DS0);

	//if(init == 5) {
		//mp_printk("%d", ((LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0]));
	//mp_printk("LSM9DS0(%p) Gyro x=%f y=%f z=%f", LSM9DS0, LSM9DS0->gyro->axis3.x, LSM9DS0->gyro->axis3.y, LSM9DS0->gyro->axis3.z);
	//init=0;
	//}
	//init++;

}

static void _mp_drv_LSM9DS0_onGyroCalibrationRead(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	signed short ga[3];
	float tmp[3];
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;

	if(LSM9DS0->gyroCal == 1) {
		LSM9DS0->gbiasCal[3]++;

		if(LSM9DS0->gbiasCal[3] == LSM9DS0_GYRO_CALIBRATION_DROP) {
			LSM9DS0->gyroCal = 2;
			LSM9DS0->gbiasCal[0] = 0;
			LSM9DS0->gbiasCal[1] = 0;
			LSM9DS0->gbiasCal[2] = 0;
			LSM9DS0->gbiasCal[3] = 0;
		}
		else
			return;
	}

	LSM9DS0->gbiasCal[0] += (LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0];
	LSM9DS0->gbiasCal[1] += (LSM9DS0->buffer[3] << 8) | LSM9DS0->buffer[2];
	LSM9DS0->gbiasCal[2] += (LSM9DS0->buffer[5] << 8) | LSM9DS0->buffer[4];
	LSM9DS0->gbiasCal[3]++;

	if(LSM9DS0->gbiasCal[3] == LSM9DS0_GYRO_CALIBRATION_COUNT) {
		LSM9DS0->gbiasCal[0] /= LSM9DS0_GYRO_CALIBRATION_COUNT;
		LSM9DS0->gbiasCal[1] /= LSM9DS0_GYRO_CALIBRATION_COUNT;
		LSM9DS0->gbiasCal[2] /= LSM9DS0_GYRO_CALIBRATION_COUNT;

		ga[0] = LSM9DS0->gbiasCal[0];
		ga[1] = LSM9DS0->gbiasCal[1];
		ga[2] = LSM9DS0->gbiasCal[2];

		tmp[0] = (float)ga[0]*LSM9DS0->gRes;
		tmp[1] = (float)ga[1]*LSM9DS0->gRes;
		tmp[2] = (float)ga[2]*LSM9DS0->gRes;

		memcpy(&LSM9DS0->gbias, &tmp, sizeof(LSM9DS0->gbias));

		mp_printk("LSM9DS0 gyro calibration bias are x=%f y=%f z=%f res=%f using %d samples",
				LSM9DS0->gbias[0], LSM9DS0->gbias[1], LSM9DS0->gbias[2], LSM9DS0->gRes, LSM9DS0_GYRO_CALIBRATION_COUNT);

		LSM9DS0->gyroCal = 0;
	}
}

//int init = 0;
static void _mp_drv_LSM9DS0_onMagRead(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;

	LSM9DS0->magneto->axis3.x = LSM9DS0->mRes*(double)((LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0]);
	LSM9DS0->magneto->axis3.y = LSM9DS0->mRes*(double)((LSM9DS0->buffer[3] << 8) | LSM9DS0->buffer[2]);
	LSM9DS0->magneto->axis3.z = LSM9DS0->mRes*(double)((LSM9DS0->buffer[5] << 8) | LSM9DS0->buffer[4]);

	if(LSM9DS0->onMagData)
		LSM9DS0->onMagData(LSM9DS0);

	//if(init == 10) {
		//mp_printk("%d", ((LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0]));
	//mp_printk("LSM9DS0(%p) Magneto x=%f y=%f z=%f res=%f", LSM9DS0, LSM9DS0->magneto->axis3.x, LSM9DS0->magneto->axis3.y, LSM9DS0->magneto->axis3.z, LSM9DS0->mRes);
	//init=0;
	//}
	//init++;
}

static void _mp_drv_LSM9DS0_onTemperatureRead(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;
	LSM9DS0->temperature->temperature.result = ((LSM9DS0->buffer[1] << 12) | LSM9DS0->buffer[0] << 4 ) >> 4;

	if(LSM9DS0->onTempData)
		LSM9DS0->onTempData(LSM9DS0);

	//mp_printk("LSM9DS0(%p) Temperature %f C", LSM9DS0, LSM9DS0->temperature->temperature.result);
}

//int initAccel = 0;
static void _mp_drv_LSM9DS0_onAccelRead(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;

	LSM9DS0->accelero->axis3.x = (LSM9DS0->aRes*(double)((LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0]))-LSM9DS0->abias[0];
	LSM9DS0->accelero->axis3.y = (LSM9DS0->aRes*(double)((LSM9DS0->buffer[3] << 8) | LSM9DS0->buffer[2]))-LSM9DS0->abias[1];
	LSM9DS0->accelero->axis3.z = (LSM9DS0->aRes*(double)((LSM9DS0->buffer[5] << 8) | LSM9DS0->buffer[4]))-LSM9DS0->abias[2];

	if(LSM9DS0->onAccelData)
		LSM9DS0->onAccelData(LSM9DS0);

	//mp_printk("Operand %p", operand);

	//if(initAccel == 10) {
		//mp_printk("%d", ((LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0]));
	//mp_printk("LSM9DS0(%p) Accel x=%f y=%f z=%f", LSM9DS0, LSM9DS0->accelero->axis3.x, LSM9DS0->accelero->axis3.y, LSM9DS0->accelero->axis3.z);
	//initAccel=0;
	//}
	//initAccel++;
}

static void _mp_drv_LSM9DS0_onAccelCalibrationRead(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	signed short ga[3];
	float tmp[3];
	mp_drv_LSM9DS0_t *LSM9DS0 = operand->user;

	if(LSM9DS0->accelCal == 1) {
		LSM9DS0->abiasCal[3]++;

		if(LSM9DS0->abiasCal[3] == LSM9DS0_ACCELERO_CALIBRATION_DROP) {
			LSM9DS0->accelCal = 2;
			LSM9DS0->abiasCal[0] = 0;
			LSM9DS0->abiasCal[1] = 0;
			LSM9DS0->abiasCal[2] = 0;
			LSM9DS0->abiasCal[3] = 0;
		}
		else
			return;
	}

	LSM9DS0->abiasCal[0] += (LSM9DS0->buffer[1] << 8) | LSM9DS0->buffer[0];
	LSM9DS0->abiasCal[1] += (LSM9DS0->buffer[3] << 8) | LSM9DS0->buffer[2];
	LSM9DS0->abiasCal[2] += (LSM9DS0->buffer[5] << 8) | LSM9DS0->buffer[4];
	LSM9DS0->abiasCal[3]++;

	if(LSM9DS0->abiasCal[3] == LSM9DS0_ACCELERO_CALIBRATION_COUNT) {
		LSM9DS0->abiasCal[0] /= LSM9DS0_ACCELERO_CALIBRATION_COUNT;
		LSM9DS0->abiasCal[1] /= LSM9DS0_ACCELERO_CALIBRATION_COUNT;
		LSM9DS0->abiasCal[2] /= LSM9DS0_ACCELERO_CALIBRATION_COUNT;

		ga[0] = LSM9DS0->abiasCal[0];
		ga[1] = LSM9DS0->abiasCal[1];
		ga[2] = LSM9DS0->abiasCal[2];

		tmp[0] = (float)ga[0]*LSM9DS0->aRes;
		tmp[1] = (float)ga[1]*LSM9DS0->aRes;
		tmp[2] = (float)ga[2]*LSM9DS0->aRes;

		memcpy(&LSM9DS0->abias, &tmp, sizeof(LSM9DS0->abias));

		mp_printk("LSM9DS0 accelero calibration bias are x=%f y=%f z=%f res=%f using %d samples",
				LSM9DS0->abias[0], LSM9DS0->abias[1], LSM9DS0->abias[2], LSM9DS0->aRes, LSM9DS0_ACCELERO_CALIBRATION_COUNT);

		LSM9DS0->accelCal = 0;
	}
}

static void _mp_drv_LSM9DS0_onDRDY(void *user) {
	mp_drv_LSM9DS0_t *LSM9DS0 = user;
	LSM9DS0->intSrc |= 0x1;
	LSM9DS0->task->signal = MP_TASK_SIG_PENDING;
}

static void _mp_drv_LSM9DS0_onIntMag(void *user) {
	mp_drv_LSM9DS0_t *LSM9DS0 = user;
	LSM9DS0->intSrc |= 0x2;
	LSM9DS0->task->signal = MP_TASK_SIG_PENDING;
}

static void _mp_drv_LSM9DS0_onIntAcc(void *user) {
	mp_drv_LSM9DS0_t *LSM9DS0 = user;
	LSM9DS0->intSrc |= 0x4;
	LSM9DS0->task->signal = MP_TASK_SIG_PENDING;
}

MP_TASK(_mp_drv_LSM9DS0_ASR) {
	mp_drv_LSM9DS0_t *LSM9DS0 = task->user;

	/* receive regMaster shutdown */
	if(task->signal == MP_TASK_SIG_STOP) {
		if(LSM9DS0->gyro)
			mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->gyro);

		if(LSM9DS0->magneto)
			mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->magneto);

		if(LSM9DS0->temperature)
			mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->temperature);

		if(LSM9DS0->accelero)
			mp_sensor_unregister(LSM9DS0->kernel, LSM9DS0->accelero);

		/* acknowledging */
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	/* Gyro atomic read */
	if(LSM9DS0->intSrc & 0x1) {
		if(LSM9DS0->gyroCal == 0) {
			mp_drv_LSM9DS0_gRead(
				LSM9DS0, OUT_X_L_G | 0x80,
				(unsigned char *)&LSM9DS0->buffer, 6,
				_mp_drv_LSM9DS0_onGyroRead
			);
		}
		else {
			mp_drv_LSM9DS0_gRead(
				LSM9DS0, OUT_X_L_G | 0x80,
				(unsigned char *)&LSM9DS0->buffer, 6,
				_mp_drv_LSM9DS0_onGyroCalibrationRead
			);
		}
		LSM9DS0->intSrc &= ~0x1;
	}

	/* Magneto atomic read */
	if(LSM9DS0->intSrc & 0x2) {
		mp_drv_LSM9DS0_xmRead(
			LSM9DS0, OUT_X_L_M | 0x80,
			(unsigned char *)&LSM9DS0->buffer, 6,
			_mp_drv_LSM9DS0_onMagRead
		);

		/* check if temperature sensor is on */
		if(LSM9DS0->xmReg5 & 0x80)
			mp_drv_LSM9DS0_xmRead(
				LSM9DS0, OUT_TEMP_L_XM | 0x80,
				(unsigned char *)&LSM9DS0->buffer, 2,
				_mp_drv_LSM9DS0_onTemperatureRead
			);
		LSM9DS0->intSrc &= ~0x2;
	}

	/* Accelero atomic read */
	if(LSM9DS0->intSrc & 0x4) {

		if(LSM9DS0->accelCal == 0) {
			mp_drv_LSM9DS0_xmRead(
				LSM9DS0, OUT_X_L_A | 0x80,
				(unsigned char *)&LSM9DS0->buffer, 6,
				_mp_drv_LSM9DS0_onAccelRead
			);
		}
		else {
			mp_drv_LSM9DS0_xmRead(
				LSM9DS0, OUT_X_L_A | 0x80,
				(unsigned char *)&LSM9DS0->buffer, 6,
				_mp_drv_LSM9DS0_onAccelCalibrationRead
			);
		}
		LSM9DS0->intSrc &= ~0x4;
	}

	task->signal = MP_TASK_SIG_SLEEP;
}


#endif

