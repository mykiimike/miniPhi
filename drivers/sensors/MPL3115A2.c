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

static unsigned short mp_drv_MPL3115A2_read(mp_drv_MPL3115A2_t *MPL3115A2, unsigned char regAddr);
static void mp_drv_MPL3115A2_write(mp_drv_MPL3115A2_t *MPL3115A2, unsigned char address, unsigned char writeByte);
static void _mp_drv_MPL3115A2_onDRDY(void *user);


void mp_drv_MPL3115A2_setSeaLevel(mp_drv_MPL3115A2_t *MPL3115A2, int Pa);

void mp_drv_MPL3115A2_OST(mp_drv_MPL3115A2_t *MPL3115A2);
float mp_drv_MPL3115A2_readPressure(mp_drv_MPL3115A2_t *MPL3115A2);
float mp_drv_MPL3115A2_readAltitude(mp_drv_MPL3115A2_t *MPL3115A2);

MP_TASK(_mp_drv_MPL3115A2_ASR);

mp_sensor_t *mp_drv_MPL3115A2_init(mp_kernel_t *kernel, mp_drv_MPL3115A2_t *MPL3115A2, mp_options_t *options, char *who) {
	unsigned char deviceId;
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
		mp_printk("MPL3115A2 require DRDY interrupt for the moment");
		mp_i2c_close(&MPL3115A2->i2c);
		return(NULL);
	}

	/* check for device id */
	deviceId = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_WHO_AM_I);
	if(deviceId != 0xc4) {
		mp_printk("Error loading MPL3115A2 driver: invalid device ID id=%x", deviceId);
		mp_i2c_close(&MPL3115A2->i2c);
		return(NULL);
	}

	/* read settings */
	mp_drv_MPL3115A2_wakeUp(MPL3115A2);

	//mp_drv_MPL3115A2_setSeaLevel(MPL3115A2, 100343);

	MPL3115A2->settings = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_CTRL_REG1);

	mp_printk("Loading MPL3115A2 driver with device id=%x settings=%x", deviceId, MPL3115A2->settings);

	/*
	mp_drv_MPL3115A2_OST(MPL3115A2);

	float test = mp_drv_MPL3115A2_readPressure(MPL3115A2);
	mp_printk("test: %f", test);
	*/
	return(MPL3115A2->sensor);
}

void mp_drv_MPL3115A2_fini(mp_drv_MPL3115A2_t *MPL3115A2) {
	mp_printk("Unloading MPL3115A2 driver");

	mp_task_destroy(MPL3115A2->task);
}


void mp_drv_MPL3115A2_sleep(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings &= ~(1<<0); //Set SBYB bit for Active mode
	mp_drv_MPL3115A2_write(MPL3115A2, MPL3115A2_CTRL_REG1, MPL3115A2->settings);
}

void mp_drv_MPL3115A2_wakeUp(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings |= (1<<0); //Set SBYB bit for Active mode
	mp_drv_MPL3115A2_write(MPL3115A2, MPL3115A2_CTRL_REG1, MPL3115A2->settings);
}

void mp_drv_MPL3115A2_reset(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings |= (1<<2); //Set RST bit
	mp_drv_MPL3115A2_write(MPL3115A2, MPL3115A2_CTRL_REG1, MPL3115A2->settings);
}


void mp_drv_MPL3115A2_setModeBarometer(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings &= ~(1<<7); //Clear ALT bit
	mp_drv_MPL3115A2_write(MPL3115A2, MPL3115A2_CTRL_REG1, MPL3115A2->settings);
}


void mp_drv_MPL3115A2_setModeAltimeter(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings |= (1<<7); //Set ALT bit
	mp_drv_MPL3115A2_write(MPL3115A2, MPL3115A2_CTRL_REG1, MPL3115A2->settings);
}

void mp_drv_MPL3115A2_setSeaLevel(mp_drv_MPL3115A2_t *MPL3115A2, int Pa) {
	unsigned char msb, lsb;

	//mp_clock_delay(500);
	//msb = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_BAR_IN_MSB);
	//mp_clock_delay(500);
	//lsb = mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_BAR_IN_LSB);


	//mp_printk("%x %x", msb, lsb);
}

float mp_drv_MPL3115A2_readPressure(mp_drv_MPL3115A2_t *MPL3115A2) {
	unsigned char msb, csb, lsb;

	/** \todo this function must be improved */

	/* Check PDR bit, if it's not set then toggle OST */
	if(mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_STATUS) & (1<<2) == 0) mp_drv_MPL3115A2_OST(MPL3115A2);

	/* Wait for PDR bit, indicates we have new pressure data */
	int counter = 0;
	while(mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_STATUS) & (1<<2) == 0) {
		if(++counter > 600) return(-999); //Error out after max of 512ms for a read
		mp_clock_delay(1);
	}

	mp_i2c_mode(&MPL3115A2->i2c, 1);
	mp_i2c_txStart(&MPL3115A2->i2c);

	/* write register */
	mp_i2c_tx(&MPL3115A2->i2c, MPL3115A2_OUT_P_MSB);
	mp_i2c_waitTX(&MPL3115A2->i2c);

	/* no stop let do a restart */
	mp_i2c_mode(&MPL3115A2->i2c, 0); /* receiver */
	mp_i2c_txStart(&MPL3115A2->i2c); /* start */

	mp_i2c_waitRX(&MPL3115A2->i2c);
	msb = mp_i2c_rx(&MPL3115A2->i2c);

	mp_i2c_waitRX(&MPL3115A2->i2c);
	csb = mp_i2c_rx(&MPL3115A2->i2c);

	mp_i2c_waitRX(&MPL3115A2->i2c);
	lsb = mp_i2c_rx(&MPL3115A2->i2c);

	/* prepare stop */
	mp_i2c_txStop(&MPL3115A2->i2c);

	mp_drv_MPL3115A2_OST(MPL3115A2);

	/* Pressure comes back as a left shifted 20 bit number */
	unsigned long pressure_whole = (long)msb<<16 | (long)csb<<8 | (long)lsb;
	pressure_whole >>= 6; //Pressure is an 18 bit number with 2 bits of decimal. Get rid of decimal portion.

	lsb &= 0x30; /* Bits 5/4 represent the fractional component */
	lsb >>= 4; /* Get it right aligned */
	float pressure_decimal = (float)lsb/4.0; /* Turn it into fraction */

	float pressure = (float)pressure_whole + pressure_decimal;

	return(pressure);
}

float mp_drv_MPL3115A2_readAltitude(mp_drv_MPL3115A2_t *MPL3115A2) {
	unsigned char msb, csb, lsb;

	/** \todo this function must be improved */

	/* Check PDR bit, if it's not set then toggle OST */
	if(mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_STATUS) & (1<<2) == 0) mp_drv_MPL3115A2_OST(MPL3115A2);

	/* Wait for PDR bit, indicates we have new pressure data */
	int counter = 0;
	while(mp_drv_MPL3115A2_read(MPL3115A2, MPL3115A2_STATUS) & (1<<2) == 0) {
		if(++counter > 600) return(-999); //Error out after max of 512ms for a read
		mp_clock_delay(1);
	}

	mp_i2c_mode(&MPL3115A2->i2c, 1);
	mp_i2c_txStart(&MPL3115A2->i2c);

	/* write register */
	mp_i2c_tx(&MPL3115A2->i2c, MPL3115A2_OUT_P_MSB);
	mp_i2c_waitTX(&MPL3115A2->i2c);

	/* no stop let do a restart */
	mp_i2c_mode(&MPL3115A2->i2c, 0); /* receiver */
	mp_i2c_txStart(&MPL3115A2->i2c); /* start */

	mp_i2c_waitRX(&MPL3115A2->i2c);
	msb = mp_i2c_rx(&MPL3115A2->i2c);

	mp_i2c_waitRX(&MPL3115A2->i2c);
	csb = mp_i2c_rx(&MPL3115A2->i2c);

	mp_i2c_waitRX(&MPL3115A2->i2c);
	lsb = mp_i2c_rx(&MPL3115A2->i2c);

	/* prepare stop */
	mp_i2c_txStop(&MPL3115A2->i2c);

	mp_drv_MPL3115A2_OST(MPL3115A2);

	float tempcsb = (lsb>>4)/16.0;

	float altitude = (float)( (msb << 8) | csb) + tempcsb;

	return(altitude);
}

void mp_drv_MPL3115A2_OST(mp_drv_MPL3115A2_t *MPL3115A2) {
	MPL3115A2->settings &= ~(1<<1); //Clear OST bit
	mp_drv_MPL3115A2_write(MPL3115A2, MPL3115A2_CTRL_REG1, MPL3115A2->settings);

	MPL3115A2->settings |= (1<<1); //Set OST bit
	mp_drv_MPL3115A2_write(MPL3115A2, MPL3115A2_CTRL_REG1, MPL3115A2->settings);
}

static unsigned short mp_drv_MPL3115A2_read(mp_drv_MPL3115A2_t *MPL3115A2, unsigned char regAddr) {
	unsigned short val = 0;

	mp_i2c_mode(&MPL3115A2->i2c, 1);
	mp_i2c_txStart(&MPL3115A2->i2c);

	/* write register */
	mp_i2c_tx(&MPL3115A2->i2c, regAddr);
	mp_i2c_waitTX(&MPL3115A2->i2c);

	/* no stop let do a restart */
	mp_i2c_mode(&MPL3115A2->i2c, 0); /* receiver */
	mp_i2c_txStart(&MPL3115A2->i2c); /* start */

	/* get a char */
	mp_i2c_waitRX(&MPL3115A2->i2c);
	val = mp_i2c_rx(&MPL3115A2->i2c);

	/* prepare stop */
	mp_i2c_txStop(&MPL3115A2->i2c);

	/* Return val */
	return val;
}


static void mp_drv_MPL3115A2_write(mp_drv_MPL3115A2_t *MPL3115A2, unsigned char address, unsigned char writeByte) {
	mp_i2c_mode(&MPL3115A2->i2c, 1);
	mp_i2c_txStart(&MPL3115A2->i2c);

    /* Send pointer byte */
    mp_i2c_tx(&MPL3115A2->i2c, address);
    mp_i2c_waitTX(&MPL3115A2->i2c);

    /* Send byte */
    mp_i2c_tx(&MPL3115A2->i2c, writeByte);
    mp_i2c_waitTX(&MPL3115A2->i2c);

    mp_i2c_txStop(&MPL3115A2->i2c);
}

static void _mp_drv_MPL3115A2_onDRDY(void *user) {
	mp_drv_MPL3115A2_t *MPL3115A2 = user;

	/* reroute asr */
	MPL3115A2->task->signal = MP_TASK_SIG_PENDING;

}

MP_TASK(_mp_drv_MPL3115A2_ASR) {
	mp_drv_MPL3115A2_t *MPL3115A2 = task->user;

	/* acknowledge task end */
	if(task->signal == MP_TASK_SIG_STOP) {
		task->signal = MP_TASK_SIG_DEAD;
		return;
	}

	MPL3115A2->task->signal = MP_TASK_SIG_SLEEP;
}

#endif

