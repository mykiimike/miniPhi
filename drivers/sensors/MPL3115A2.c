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

#ifdef SUPPORT_DRV_MPL3115A2

static void _mp_drv_MPL3115A2_onDRDY(void *user);

void mp_drv_MPL3115A2_setSeaLevel(mp_drv_MPL3115A2_t *MPL3115A2, int Pa);

void _mp_drv_MPL3115A2_onWhoIAm(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;
	if(MPL3115A2->whoIam != 0xc4) {
		mp_printk("MPL3115A2(%p): Got who iam %x (bad), terminating", operand->user, MPL3115A2->whoIam);
		mp_regMaster_fini(&MPL3115A2->regMaster);
	}
	else
		mp_printk("MPL3115A2(%p): Got who iam %x (good)", operand->user, MPL3115A2->whoIam);
}

void _mp_drv_MPL3115A2_onSettings(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;
	mp_printk("MPL3115A2(%p): Initial settings is %x", operand->user, MPL3115A2->settings);
}

void _mp_drv_MPL3115A2_writeControl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;
	mp_mem_free(MPL3115A2->kernel, operand->reg);
	//mp_printk("MPL3115A2(%p): Got write control", operand->user);
}

void _mp_drv_MPL3115A2_readPressureControl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;
	unsigned char msb, csb, lsb;

	msb = operand->wait[0];
	csb = operand->wait[1];
	lsb = operand->wait[2];

	/* Pressure comes back as a left shifted 20 bit number */
	unsigned long pressure_whole = (long)msb<<16 | (long)csb<<8 | (long)lsb;
	pressure_whole >>= 6; //Pressure is an 18 bit number with 2 bits of decimal. Get rid of decimal portion.

	lsb &= 0x30; /* Bits 5/4 represent the fractional component */
	lsb >>= 4; /* Get it right aligned */
	float pressure_decimal = (float)lsb/4.0; /* Turn it into fraction */

	MPL3115A2->sensor->barometer.result = (float)pressure_whole + pressure_decimal;

	mp_printk("Got pressure information: %f", MPL3115A2->sensor->barometer.result);

	mp_mem_free(MPL3115A2->kernel, operand->wait);
}

void _mp_drv_MPL3115A2_readAltimeterControl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;
	float tempcsb = (operand->wait[2]>>4)/16.0;
	float altitude = (float)( (operand->wait[0] << 8) | operand->wait[1]) + tempcsb;

	if(MPL3115A2->sensor->altimeter.conversion == MP_SENSOR_ALTIMETER_FEET)
		MPL3115A2->sensor->altimeter.result = altitude;
	else
		MPL3115A2->sensor->altimeter.result = altitude/MP_SENSOR_ALTIMETER_FMC;

	//mp_printk("Got altimeter information: %f", MPL3115A2->sensor->altimeter.result);
	mp_mem_free(MPL3115A2->kernel, operand->wait);
}

void _mp_drv_MPL3115A2_readIntSource(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;


	/* OUT PRESSURE interrupt */
	if(operand->wait[0] & 0x80) {
		unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 6);

		mp_regMaster_read(
			&MPL3115A2->regMaster,
			mp_regMaster_register(MPL3115A2_OUT_P_MSB), 1,
			ptr, 3,
			MPL3115A2->readerControl, MPL3115A2
		);
	}

	mp_mem_free(MPL3115A2->kernel, operand->wait);
}


/**
@defgroup mpDriverFreescaleMPL3115A2 Freescale MPL3115A2

@ingroup mpDriver

@brief Freescale MPL3115A2 Altimeter / Barometer / Temperature

@version 1.0.0

@author @htmlonly &copy; @endhtmlonly 2015
Michael Vergoz <mv@verman.fr>

@date 03 Feb 2015

Informations :
@li <strong>INT1</strong> is configured by design for DRDY interrupt.
@li <strong>DRDY</strong> must be connected to drive the read of datas.

Breakouts available on :
@li https://www.sparkfun.com/products/11084
@li http://www.adafruit.com/product/1893
@li http://www.artekit.eu/products/breakout-boards/ak-mpl3115a2-altimeter-pressure-sensor-breakout/


Configuration for MPL3115A2 example :
@li gate = USCI_B3 // msp430 based
@li SDA = 10.1 / ext 1-17
@li SCL = 10.2 / ext 1-16
@li DRDY = 1.1 / ext 2-5 (to INT1)

Initializing the driver :
@code
typedef struct olimex_msp430_s olimex_msp430_t;

struct olimex_msp430_s {
	mp_kernel_t kernel;

	mp_drv_MPL3115A2_t bat;
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

	mp_drv_MPL3115A2_init(&olimex->kernel, &olimex->bat, options, "Freescale MPL3115A2");
}
@endcode

@{
*/


mp_ret_t mp_drv_MPL3115A2_init(mp_kernel_t *kernel, mp_drv_MPL3115A2_t *MPL3115A2, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(MPL3115A2, 0, sizeof(*MPL3115A2));
	MPL3115A2->kernel = kernel;

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		MPL3115A2->drdy = mp_gpio_text_handle(value, "MPL3115A2 DRDY");
		if(!MPL3115A2->drdy) {
			mp_printk("MPL3115A2: need a valid DRDY port");
			mp_drv_MPL3115A2_fini(MPL3115A2);
			return(FALSE);
		}

		/* set CS high */
		mp_gpio_direction(MPL3115A2->drdy, MP_GPIO_INPUT);
	}
	else
		MPL3115A2->drdy = NULL;

	/* open spi */
	ret = mp_i2c_open(kernel, &MPL3115A2->i2c, options, "MPL3115A2");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
		{ "frequency", "400000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&MPL3115A2->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&MPL3115A2->i2c);
		return(FALSE);
	}

	/* set slave address */
	mp_i2c_setSlaveAddress(&MPL3115A2->i2c, MPL3115A2_ADDRESS);

	/* enable chip */
	if(MPL3115A2->drdy) {
		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(MPL3115A2->drdy, _mp_drv_MPL3115A2_onDRDY, MPL3115A2, who);
		if(ret == FALSE) {
			mp_gpio_release(MPL3115A2->drdy);
			return(FALSE);
		}
		mp_gpio_interrupt_hi2lo(MPL3115A2->drdy);
	}
	else {
		mp_printk("MPL3115A2(%p): require DRDY interrupt for the moment", MPL3115A2);
		mp_i2c_close(&MPL3115A2->i2c);
		return(FALSE);
	}

	/* create regmaster control */
	ret = mp_regMaster_init_i2c(kernel, &MPL3115A2->regMaster,
			&MPL3115A2->i2c, MPL3115A2, "MPL3115A2 I2C");
	if(ret == FALSE) {
		mp_printk("MPL3115A2 error while creating regMaster context");
		mp_i2c_close(&MPL3115A2->i2c);
		return(FALSE);
	}

	mp_printk("MPL3115A2(%p): Initializing", MPL3115A2);

	/* check for device id */
	mp_regMaster_read(
		&MPL3115A2->regMaster,
		mp_regMaster_register(MPL3115A2_WHO_AM_I), 1,
		(unsigned char *)&MPL3115A2->whoIam, 1,
		_mp_drv_MPL3115A2_onWhoIAm, MPL3115A2
	);

	/* reset the device */
	//mp_drv_MPL3115A2_reset(MPL3115A2);

	/* set altimeter mode */
	mp_drv_MPL3115A2_setModeAltimeter(MPL3115A2);

	/* Enable Data Flags in PT_DATA_CFG */
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	*(ptr++) = MPL3115A2_PT_DATA_CFG;
	*ptr = 0x07;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

	/* Set INT to Active Low Open Drain */
	/*
	ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG3;
	*ptr = 0x11;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);
*/



	/* Route DRDY INT to INT1 */
	ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG5;
	*ptr = 0x80;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

	/* Enable DRDY Interrupt */
	ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG4;
	*ptr = 0x80;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

	/* change OS timer */
	mp_drv_MPL3115A2_OSTimer(MPL3115A2, MPL3115A_512MS);

	/* active mode */
	mp_drv_MPL3115A2_wakeUp(MPL3115A2);

	/* get back reg1 */
	mp_regMaster_read(
		&MPL3115A2->regMaster,
		mp_regMaster_register(MPL3115A2_CTRL_REG1), 1,
		(unsigned char *)&MPL3115A2->settings, 1,
		_mp_drv_MPL3115A2_onSettings, MPL3115A2
	);

	return(TRUE);
}

void mp_drv_MPL3115A2_fini(mp_drv_MPL3115A2_t *MPL3115A2) {
	mp_printk("Unloading MPL3115A2 driver");
}


void mp_drv_MPL3115A2_sleep(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings &= ~(1<<0);
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);


}

void mp_drv_MPL3115A2_wakeUp(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings |= (1<<0); //Set SBYB bit for Active mode
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);
}

void mp_drv_MPL3115A2_reset(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings |= (1<<2);
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);
}

mp_ret_t mp_drv_MPL3115A2_acquisitionTimeStep(mp_drv_MPL3115A2_t *MPL3115A2, unsigned char st) {
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;
	unsigned char control = st;

	if(st > 15)
		return(FALSE);

	*(ptr++) = MPL3115A2_CTRL_REG2;
	*ptr = control;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

	return(TRUE);
}


void mp_drv_MPL3115A2_setModeBarometer(mp_drv_MPL3115A2_t *MPL3115A2) {
	/* enable sensor */
	if(MPL3115A2->sensor)
		mp_sensor_unregister(MPL3115A2->kernel, MPL3115A2->sensor);
	MPL3115A2->sensor = mp_sensor_register(MPL3115A2->kernel, MP_SENSOR_BAROMETER, "Barometer");

	MPL3115A2->settings &= ~(1<<7); //Clear ALT bit
	MPL3115A2->readerControl = _mp_drv_MPL3115A2_readPressureControl;

	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

}


void mp_drv_MPL3115A2_setModeAltimeter(mp_drv_MPL3115A2_t *MPL3115A2) {
	/* enable sensor */
	if(MPL3115A2->sensor)
		mp_sensor_unregister(MPL3115A2->kernel, MPL3115A2->sensor);
	MPL3115A2->sensor = mp_sensor_register(MPL3115A2->kernel, MP_SENSOR_ALTIMETER, "Altimeter");
	MPL3115A2->sensor->altimeter.conversion = MP_SENSOR_ALTIMETER_FEET;

	/* request command */
	MPL3115A2->settings |= (1<<7); //Set ALT bit
	MPL3115A2->readerControl = _mp_drv_MPL3115A2_readAltimeterControl;
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

}

void mp_drv_MPL3115A2_setSeaLevel(mp_drv_MPL3115A2_t *MPL3115A2, int Pa) {
	unsigned char msb, lsb;

	//mp_clock_delay(500);
	//msb = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_BAR_IN_MSB);
	//mp_clock_delay(500);
	//lsb = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_BAR_IN_LSB);


	//mp_printk("%x %x", msb, lsb);
}



void mp_drv_MPL3115A2_OST(mp_drv_MPL3115A2_t *MPL3115A2) {
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	/* Clear OST bit */
	MPL3115A2->settings &= ~(1<<1);

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

	/* Set OST bit */
	MPL3115A2->settings |= (1<<1);

	ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);


}

void mp_drv_MPL3115A2_OSTimer(mp_drv_MPL3115A2_t *MPL3115A2, mp_drv_MPL3115A_OS_t timer) {
	MPL3115A2->settings |= (timer<<3);
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 2);
	unsigned char *src = ptr;

	*(ptr++) = MPL3115A2_CTRL_REG1;
	*ptr = MPL3115A2->settings;

	mp_regMaster_write(
		&MPL3115A2->regMaster,
		src, 2,
		_mp_drv_MPL3115A2_writeControl, MPL3115A2
	);

}

/**@}*/


static void _mp_drv_MPL3115A2_onDRDY(void *user) {
	mp_drv_MPL3115A2_t *MPL3115A2 = user;

	P10OUT ^= 0xc0;

	/* run read */
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 1);

	mp_regMaster_read(
		&MPL3115A2->regMaster,
		mp_regMaster_register(MPL3115A2_INT_SOURCE), 1,
		ptr, 1,
		_mp_drv_MPL3115A2_readIntSource, MPL3115A2
	);
}


#endif


