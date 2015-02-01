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

/*
 * Freescale MPL3115A2 Altimeter / Barometer / Temperature
 *
 * INT1 is configured by design for DRDY interrupt.
 * DRDY must be connected to drive the read of datas.
 *
 * Breakouts available on :
 * https://www.sparkfun.com/products/11084
 * http://www.adafruit.com/product/1893
 * http://www.artekit.eu/products/breakout-boards/ak-mpl3115a2-altimeter-pressure-sensor-breakout/
 */

#ifdef SUPPORT_DRV_MPL3115A2

static void _mp_drv_MPL3115A2_onDRDY(void *user);

void mp_drv_MPL3115A2_setSeaLevel(mp_drv_MPL3115A2_t *MPL3115A2, int Pa);

static unsigned char *_registers = NULL;
static int _register_references = 0;


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

	/* Pressure comes back as a left shifted 20 bit number */
	//unsigned long pressure_whole = (long)msb<<16 | (long)csb<<8 | (long)lsb;
	//pressure_whole >>= 6; //Pressure is an 18 bit number with 2 bits of decimal. Get rid of decimal portion.

	//lsb &= 0x30; /* Bits 5/4 represent the fractional component */
	//lsb >>= 4; /* Get it right aligned */
	//float pressure_decimal = (float)lsb/4.0; /* Turn it into fraction */

	//float pressure = (float)pressure_whole + pressure_decimal;


}

void _mp_drv_MPL3115A2_readAltimeterControl(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;
	float tempcsb = (operand->wait[2]>>4)/16.0;
	float altitude = (float)( (operand->wait[0] << 8) | operand->wait[1]) + tempcsb;
	mp_printk("Got altimeter information: %f ft", altitude);
	mp_mem_free(MPL3115A2->kernel, operand->wait);
	//mp_gpio_interrupt_enable(MPL3115A2->drdy);
}

void _mp_drv_MPL3115A2_readIntSource(mp_regMaster_op_t *operand, mp_bool_t terminate) {
	mp_drv_MPL3115A2_t *MPL3115A2 = operand->user;

	mp_printk("got read int source %x", operand->wait[0]);

	if(operand->wait[0] & 0x80) {
		unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 6);

		mp_regMaster_read(
			&MPL3115A2->regMaster,
			&_registers[MPL3115A2_OUT_P_MSB], 1,
			ptr, 6,
			MPL3115A2->readerControl, MPL3115A2
		);
	}

	mp_mem_free(MPL3115A2->kernel, operand->wait);
}



static void _mp_drv_MPL3115A2_ginit(mp_kernel_t *kernel) {
	int a;

	if(_registers)
		return;

	_registers = malloc(MPL3115A2_OFF_H+1);
	for(a=0; a<MPL3115A2_OFF_H+1; a++)
		_registers[a] = a;

	_register_references++;
}

static void _mp_drv_MPL3115A2_gfini(mp_kernel_t *kernel) {
	_register_references--;
	if(_register_references == 0)
		free(_registers);

}

mp_sensor_t *mp_drv_MPL3115A2_init(mp_kernel_t *kernel, mp_drv_MPL3115A2_t *MPL3115A2, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	_mp_drv_MPL3115A2_ginit(kernel);

	memset(MPL3115A2, 0, sizeof(*MPL3115A2));
	MPL3115A2->kernel = kernel;

	/* drdy */
	value = mp_options_get(options, "drdy");
	if(value) {
		MPL3115A2->drdy = mp_gpio_text_handle(value, "MPL3115A2 DRDY");
		if(!MPL3115A2->drdy) {
			mp_printk("MPL3115A2: need a valid DRDY port");
			mp_drv_MPL3115A2_fini(MPL3115A2);
			return(NULL);
		}

		/* set CS high */
		mp_gpio_direction(MPL3115A2->drdy, MP_GPIO_INPUT);
	}
	else
		MPL3115A2->drdy = NULL;

	/* open spi */
	ret = mp_i2c_open(kernel, &MPL3115A2->i2c, options, "MPL3115A2");
	if(ret == FALSE)
		return(NULL);

	mp_options_t setup[] = {
		{ "frequency", "100000" },
		{ "role", "master" },
		{ NULL, NULL }
	};
	ret = mp_i2c_setup(&MPL3115A2->i2c, setup);
	if(ret == FALSE) {
		mp_i2c_close(&MPL3115A2->i2c);
		return(NULL);
	}

	/* set slave address */
	mp_i2c_setSlaveAddress(&MPL3115A2->i2c, MPL3115A2_ADDRESS);

	/* enable chip */
	if(MPL3115A2->drdy) {
		/* install drdy interrupt high > low */
		ret = mp_gpio_interrupt_set(MPL3115A2->drdy, _mp_drv_MPL3115A2_onDRDY, MPL3115A2, who);
		if(ret == FALSE) {
			mp_gpio_release(MPL3115A2->drdy);
			return(NULL);
		}
		mp_gpio_interrupt_hi2lo(MPL3115A2->drdy);
	}
	else {
		mp_printk("MPL3115A2(%p): require DRDY interrupt for the moment", MPL3115A2);
		mp_i2c_close(&MPL3115A2->i2c);
		return(NULL);
	}

	/* create regmaster control */
	ret = mp_regMaster_init_i2c(kernel, &MPL3115A2->regMaster,
			&MPL3115A2->i2c, MPL3115A2, "MPL3115A2 I2C");
	if(ret == FALSE) {
		mp_printk("MPL3115A2 error while creating regMaster context");
		mp_i2c_close(&MPL3115A2->i2c);
		return(NULL);
	}

	mp_printk("MPL3115A2(%p): Initializing", MPL3115A2);

	/* check for device id */
	mp_regMaster_read(
		&MPL3115A2->regMaster,
		&_registers[MPL3115A2_WHO_AM_I], 1,
		(unsigned char *)&MPL3115A2->whoIam, 1,
		_mp_drv_MPL3115A2_onWhoIAm, MPL3115A2
	);

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
		&_registers[MPL3115A2_CTRL_REG1], 1,
		(unsigned char *)&MPL3115A2->settings, 1,
		_mp_drv_MPL3115A2_onSettings, MPL3115A2
	);



	/*
	deviceId = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_WHO_AM_I);
	if(deviceId != 0xc4) {
		mp_printk("Error loading MPL3115A2 driver: invalid device ID id=%x", deviceId);
		mp_i2c_close(&MPL3115A2->i2c);
		return(NULL);
	}
*/
	/* read settings */
	//mp_drv_MPL3115A2_wakeUp(MPL3115A2);

	//mp_drv_MPL3115A2_setSeaLevel(MPL3115A2, 100343);

	//MPL3115A2->settings = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_CTRL_REG1);

	//mp_printk("Loading MPL3115A2 driver with device id=%x settings=%x", deviceId, MPL3115A2->settings);

	/*
	mp_drv_MPL3115A2_OST(MPL3115A2);

	float test = mp_drv_MPL3115A2_readPressure(MPL3115A2);
	mp_printk("test: %f", test);
	*/
	return(MPL3115A2->sensor);
}

void mp_drv_MPL3115A2_fini(mp_drv_MPL3115A2_t *MPL3115A2) {
	mp_printk("Unloading MPL3115A2 driver");

	_mp_drv_MPL3115A2_gfini(MPL3115A2->kernel);
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




static void _mp_drv_MPL3115A2_onDRDY(void *user) {
	mp_drv_MPL3115A2_t *MPL3115A2 = user;

	P10OUT ^= 0xc0;

	/* run read */
	unsigned char *ptr = mp_mem_alloc(MPL3115A2->kernel, 1);

	mp_regMaster_read(
		&MPL3115A2->regMaster,
		&_registers[MPL3115A2_INT_SOURCE], 1,
		ptr, 1,
		_mp_drv_MPL3115A2_readIntSource, MPL3115A2
	);

	//mp_gpio_interrupt_disable(MPL3115A2->drdy);
}


#endif

