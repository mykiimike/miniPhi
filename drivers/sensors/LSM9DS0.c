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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Some parts of this source has been ported from Kris Winer code's.
 * https://github.com/kriswiner/LSM9DS0
 */

#include <mp.h>

#ifdef SUPPORT_DRV_LSM9DS0

static void _mp_drv_LSM9DS0_calcgRes(mp_drv_LSM9DS0_t *LSM9DS0);
static void _mp_drv_LSM9DS0_calcaRes(mp_drv_LSM9DS0_t *LSM9DS0);
static void _mp_drv_LSM9DS0_calcmRes(mp_drv_LSM9DS0_t *LSM9DS0);

static _mp_drv_LSM9DS0_spi_writeByte(mp_drv_LSM9DS0_t *LSM9DS0, mp_gpio_port_t *cs,
		unsigned char subAddress, unsigned char data
);
static unsigned char _mp_drv_LSM9DS0_spi_readByte(mp_drv_LSM9DS0_t *LSM9DS0, mp_gpio_port_t *cs,
		unsigned char subAddress
);
static void _mp_drv_LSM9DS0_spi_readBytes(
	mp_drv_LSM9DS0_t *LSM9DS0, mp_gpio_port_t *cs,
	unsigned char subAddress, unsigned char * dest, unsigned char count
);

mp_ret_t mp_drv_LSM9DS0_init(mp_kernel_t *kernel, mp_drv_LSM9DS0_t *LSM9DS0, mp_options_t *options, char *who) {
	char *value;
	mp_ret_t ret;

	memset(LSM9DS0, 0, sizeof(*LSM9DS0));
	LSM9DS0->kernel = kernel;

	/* csG */
	value = mp_options_get(options, "csG");
	if(!value) {
		mp_printk("LSM9DS0: need csG port");
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
		return(FALSE);
	}
	LSM9DS0->csXM = mp_gpio_text_handle(value, "LSM9DS0 csXM");
    if(!LSM9DS0->csXM) {
    	mp_printk("LSM9DS0: need csXM port");
    	mp_drv_LSM9DS0_fini(LSM9DS0);
		return(FALSE);
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
	}

	/* set CS high */
	mp_gpio_direction(LSM9DS0->csG, MP_GPIO_OUTPUT);
	mp_gpio_direction(LSM9DS0->csXM, MP_GPIO_OUTPUT);
	mp_gpio_set(LSM9DS0->csG);
	mp_gpio_set(LSM9DS0->csXM);

	/* open spi */
	ret = mp_spi_open(kernel, &LSM9DS0->spi, options, "LSM9DS0");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
		{ "frequency", "10000000" },
		{ "phase", "change" },
		{ "polarity", "high" },
		{ "first", "MSB" },
		{ "role", "master" },
		{ "bit", "8" },
		{ NULL, NULL }
	};
	ret = mp_spi_setup(&LSM9DS0->spi, setup);
	if(ret == FALSE) {
		mp_spi_close(&LSM9DS0->spi);
		return(FALSE);
	}

	// To verify communication, we can read from the WHO_AM_I register of
	// each device. Store those in a variable so we can return them.
	unsigned char gTest = mp_drv_LSM9DS0_gReadByte(LSM9DS0, WHO_AM_I_G); // Read the gyro WHO_AM_I
	unsigned char xmTest = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, WHO_AM_I_XM); // Read the accel/mag WHO_AM_I

	mp_printk("Initialize LSM9DS0 driver using SPI g=%x xm=%x", gTest, xmTest);

	/*
	// Gyro initialization stuff:
	mp_drv_LSM9DS0_initGyro(LSM9DS0); // This will "turn on" the gyro. Setting up interrupts, etc.
	mp_drv_LSM9DS0_setGyroODR(LSM9DS0, gODR); // Set the gyro output data rate and bandwidth.
	mp_drv_LSM9DS0_setGyroScale(LSM9DS0, LSM9DS0->gyro_scale); // Set the gyro range

	// Accelerometer initialization stuff:
	mp_drv_LSM9DS0_initAccel(LSM9DS0); // "Turn on" all axes of the accel. Set up interrupts, etc.
	mp_drv_LSM9DS0_setAccelODR(LSM9DS0, aODR); // Set the accel data rate.
	mp_drv_LSM9DS0_setAccelScale(LSM9DS0, LSM9DS0->accel_scale); // Set the accel range.

	// Magnetometer initialization stuff:
	mp_drv_LSM9DS0_calibrate(LSM9DS0); // "Turn on" all axes of the mag. Set up interrupts, etc.
	mp_drv_LSM9DS0_setMagODR(LSM9DS0, mODR); // Set the magnetometer output data rate.
	mp_drv_LSM9DS0_setMagScale(LSM9DS0, LSM9DS0->mag_scale); // Set the magnetometer's range.

	// run calibration
	//mp_drv_LSM9DS0_calibrate(LSM9DS0);

	mp_drv_LSM9DS0_start(LSM9DS0);
	*/
	return(TRUE);
}


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

	mp_spi_close(&LSM9DS0->spi);

	mp_printk("Stopping LSM9DS0 SPI binding");
	return(TRUE);
}


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


void mp_drv_LSM9DS0_readAccel(mp_drv_LSM9DS0_t *LSM9DS0) {
	unsigned char temp[6]; // We'll read six bytes from the accelerometer into temp
	mp_drv_LSM9DS0_xmReadBytes(LSM9DS0, OUT_X_L_A, temp, 6); // Read 6 bytes, beginning at OUT_X_L_A
	LSM9DS0->ax = (temp[1] << 8) | temp[0]; // Store x-axis values into ax
	LSM9DS0->ay = (temp[3] << 8) | temp[2]; // Store y-axis values into ay
	LSM9DS0->az = (temp[5] << 8) | temp[4]; // Store z-axis values into az
}

void mp_drv_LSM9DS0_readMag(mp_drv_LSM9DS0_t *LSM9DS0) {
	unsigned char temp[6]; // We'll read six bytes from the mag into temp
	mp_drv_LSM9DS0_xmReadBytes(LSM9DS0, OUT_X_L_M, temp, 6); // Read 6 bytes, beginning at OUT_X_L_M
	LSM9DS0->mx = (temp[1] << 8) | temp[0]; // Store x-axis values into mx
	LSM9DS0->my = (temp[3] << 8) | temp[2]; // Store y-axis values into my
	LSM9DS0->mz = (temp[5] << 8) | temp[4]; // Store z-axis values into mz
}

void mp_drv_LSM9DS0_readTemp(mp_drv_LSM9DS0_t *LSM9DS0) {
	unsigned char temp[2]; // We'll read two bytes from the temperature sensor into temp
	mp_drv_LSM9DS0_xmReadBytes(LSM9DS0, OUT_TEMP_L_XM, temp, 2); // Read 2 bytes, beginning at OUT_TEMP_L_M
	LSM9DS0->temperature = (((unsigned short) temp[1] << 12) | temp[0] << 4 ) >> 4; // Temperature is a 12-bit signed integer
}

void mp_drv_LSM9DS0_readGyro(mp_drv_LSM9DS0_t *LSM9DS0) {
	unsigned char temp[6]; // We'll read six bytes from the gyro into temp
	mp_drv_LSM9DS0_gReadBytes(LSM9DS0, OUT_X_L_G, temp, 6); // Read 6 bytes, beginning at OUT_X_L_G
	LSM9DS0->gx = (temp[1] << 8) | temp[0]; // Store x-axis values into gx
	LSM9DS0->gy = (temp[3] << 8) | temp[2]; // Store y-axis values into gy
	LSM9DS0->gz = (temp[5] << 8) | temp[4]; // Store z-axis values into gz
}

void mp_drv_LSM9DS0_initGyro(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG1_G, 0x0F); // Normal mode, enable all axes
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG2_G, 0x00); // Normal mode, high cutoff frequency
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG3_G, 0x88);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG4_G, 0x00); // Set scale to 245 dps
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG5_G, 0x00);

	//mp_drv_LSM9DS0_gWriteByte(LSM9DS0, INT1_CFG_G, 0x2a);
}

void mp_drv_LSM9DS0_initAccel(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG0_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG1_XM, 0x57); // 100Hz data rate, x/y/z all enabled
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG2_XM, 0x00); // Set scale to 2g
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG3_XM, 0x04);
}

void mp_drv_LSM9DS0_initMag(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG5_XM, 0x94); // Mag data rate - 100 Hz, enable temperature sensor
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG6_XM, 0x00); // Mag scale to +/- 2GS
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG7_XM, 0x00); // Continuous conversion mode
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG4_XM, 0x04); // Magnetometer data ready on INT2_XM (0x08)
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, INT_CTRL_REG_M, 0x09); // Enable interrupts for mag, active-low, push-pull
}

void mp_drv_LSM9DS0_finiGyro(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG1_G, 0x00);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG2_G, 0x00);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG3_G, 0x00);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG4_G, 0x00);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG5_G, 0x00);
}

void mp_drv_LSM9DS0_finiAccel(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG0_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG1_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG2_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG3_XM, 0x00);
}

void mp_drv_LSM9DS0_finiMag(mp_drv_LSM9DS0_t *LSM9DS0) {
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG5_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG6_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG7_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG4_XM, 0x00);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, INT_CTRL_REG_M, 0x00);
}


float mp_drv_LSM9DS0_calcGyro(mp_drv_LSM9DS0_t *LSM9DS0, unsigned short gyro) {
	// Return the gyro raw reading times our pre-calculated DPS / (ADC tick):
	return LSM9DS0->gRes * gyro;
}

float mp_drv_LSM9DS0_calcAccel(mp_drv_LSM9DS0_t *LSM9DS0, unsigned short accel) {
	// Return the accel raw reading times our pre-calculated g's / (ADC tick):
	return LSM9DS0->aRes * accel;
}

float mp_drv_LSM9DS0_calcMag(mp_drv_LSM9DS0_t *LSM9DS0, unsigned short mag) {
	// Return the mag raw reading times our pre-calculated Gs / (ADC tick):
	return LSM9DS0->mRes * mag;
}


void mp_drv_LSM9DS0_setGyroScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_gyro_scale_t gScl) {
	// We need to preserve the other bytes in CTRL_REG4_G. So, first read it:
	unsigned char temp = mp_drv_LSM9DS0_gReadByte(LSM9DS0, CTRL_REG4_G);

	// Then mask out the gyro scale bits:
	temp &= 0xFF^(0x3 << 4);

	// Then shift in our new scale bits:
	temp |= gScl << 4;

	// And write the new register value back into CTRL_REG4_G:
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG4_G, temp);

	// We've updated the sensor, but we also need to update our class variables
	// First update gScale:
	LSM9DS0->gyro_scale = gScl;

	// Then calculate a new gRes, which relies on gScale being set correctly:
	_mp_drv_LSM9DS0_calcgRes(LSM9DS0);
}

void mp_drv_LSM9DS0_setAccelScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_scale_t aScl) {
	// We need to preserve the other bytes in CTRL_REG2_XM. So, first read it:
	unsigned char temp = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, CTRL_REG2_XM);

	// Then mask out the accel scale bits:
	temp &= 0xFF^(0x3 << 3);

	// Then shift in our new scale bits:
	temp |= aScl << 3;

	// And write the new register value back into CTRL_REG2_XM:
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG2_XM, temp);

	// We've updated the sensor, but we also need to update our class variables
	// First update aScale:
	LSM9DS0->accel_scale = aScl;

	// Then calculate a new aRes, which relies on aScale being set correctly:
	_mp_drv_LSM9DS0_calcaRes(LSM9DS0);
}

void mp_drv_LSM9DS0_setMagScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_mag_scale_t mScl) {
	// We need to preserve the other bytes in CTRL_REG6_XM. So, first read it:
	unsigned char temp = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, CTRL_REG6_XM);

	// Then mask out the mag scale bits:
	temp &= 0xFF^(0x3 << 5);

	// Then shift in our new scale bits:
	temp |= mScl << 5;

	// And write the new register value back into CTRL_REG6_XM:
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG6_XM, temp);

	// We've updated the sensor, but we also need to update our class variables
	// First update mScale:
	LSM9DS0->mag_scale = mScl;

	// Then calculate a new mRes, which relies on mScale being set correctly:
	_mp_drv_LSM9DS0_calcmRes(LSM9DS0);
}


void mp_drv_LSM9DS0_setGyroODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_gyro_odr_t gRate) {
	// We need to preserve the other bytes in CTRL_REG1_G. So, first read it:
	unsigned char temp = mp_drv_LSM9DS0_gReadByte(LSM9DS0, CTRL_REG1_G);

	// Then mask out the gyro ODR bits:
	temp &= 0xFF^(0xF << 4);

	// Then shift in our new ODR bits:
	temp |= (gRate << 4);

	// And write the new register value back into CTRL_REG1_G:
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG1_G, temp);
}

void mp_drv_LSM9DS0_setAccelODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_odr_t aRate) {
	// We need to preserve the other bytes in CTRL_REG1_XM. So, first read it:
	unsigned char temp = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, CTRL_REG1_XM);

	// Then mask out the accel ODR bits:
	temp &= 0xFF^(0xF << 4);

	// Then shift in our new ODR bits:
	temp |= (aRate << 4);

	// And write the new register value back into CTRL_REG1_XM:
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG1_XM, temp);
}

void mp_drv_LSM9DS0_setAccelABW(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_abw_t abwRate) {
	// We need to preserve the other bytes in CTRL_REG2_XM. So, first read it:
	unsigned char temp = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, CTRL_REG2_XM);

	// Then mask out the accel ABW bits:
	temp &= 0xFF^(0x3 << 7);

	// Then shift in our new ODR bits:
	temp |= (abwRate << 7);

	// And write the new register value back into CTRL_REG2_XM:
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG2_XM, temp);
}

void mp_drv_LSM9DS0_setMagODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_mag_odr_t mRate) {
	// We need to preserve the other bytes in CTRL_REG5_XM. So, first read it:
	unsigned char temp = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, CTRL_REG5_XM);

	// Then mask out the mag ODR bits:
	temp &= 0xFF^(0x7 << 2);

	// Then shift in our new ODR bits:
	temp |= (mRate << 2);

	// And write the new register value back into CTRL_REG5_XM:
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG5_XM, temp);
}



void mp_drv_LSM9DS0_calibrate(mp_drv_LSM9DS0_t *LSM9DS0) {
	unsigned char data[6] = {0, 0, 0, 0, 0, 0};
	unsigned long gyro_bias[3] = {0, 0, 0}, accel_bias[3] = {0, 0, 0};
	unsigned short samples, ii;

	unsigned char c = mp_drv_LSM9DS0_gReadByte(LSM9DS0, CTRL_REG5_G);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG5_G, c | 0x40); // Enable gyro FIFO
	mp_clock_delay(20); // Wait for change to take effect
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, FIFO_CTRL_REG_G, 0x20 | 0x1F); // Enable gyro FIFO stream mode and set watermark at 32 samples
	mp_clock_delay(1000); // delay 1000 milliseconds to collect FIFO samples

	samples = (mp_drv_LSM9DS0_gReadByte(LSM9DS0, FIFO_SRC_REG_G) & 0x1F); // Read number of stored samples
	for(ii = 0; ii < samples ; ii++) { // Read the gyro data stored in the FIFO
		unsigned short gyro_temp[3] = {0, 0, 0};
		mp_drv_LSM9DS0_gReadBytes(LSM9DS0, OUT_X_L_G, &data[0], 6);
		gyro_temp[0] = (unsigned short) (((unsigned short)data[1] << 8) | data[0]); // Form signed 16-bit integer for each sample in FIFO
		gyro_temp[1] = (unsigned short) (((unsigned short)data[3] << 8) | data[2]);
		gyro_temp[2] = (unsigned short) (((unsigned short)data[5] << 8) | data[4]);
		gyro_bias[0] += (unsigned long) gyro_temp[0]; // Sum individual signed 16-bit biases to get accumulated signed 32-bit biases
		gyro_bias[1] += (unsigned long) gyro_temp[1];
		gyro_bias[2] += (unsigned long) gyro_temp[2];
	}

	gyro_bias[0] /= samples; // average the data
	gyro_bias[1] /= samples;
	gyro_bias[2] /= samples;

	// Properly scale the data to get deg/s
	LSM9DS0->gbias[0] = (float)gyro_bias[0]*LSM9DS0->gRes;
	LSM9DS0->gbias[1] = (float)gyro_bias[1]*LSM9DS0->gRes;
	LSM9DS0->gbias[2] = (float)gyro_bias[2]*LSM9DS0->gRes;

	mp_printk("LSM9DS0 Gyro calibration bias are x=%f y=%f z=%f using %d samples",
			LSM9DS0->gbias[0], LSM9DS0->gbias[1], LSM9DS0->gbias[2], samples);

	c = mp_drv_LSM9DS0_gReadByte(LSM9DS0, CTRL_REG5_G);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, CTRL_REG5_G, c & ~0x40); // Disable gyro FIFO
	mp_clock_delay(20);
	mp_drv_LSM9DS0_gWriteByte(LSM9DS0, FIFO_CTRL_REG_G, 0x00); // Enable gyro bypass mode
	mp_clock_delay(20);

	// Now get the accelerometer biases
	c = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, CTRL_REG0_XM);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG0_XM, c | 0x40); // Enable accelerometer FIFO
	mp_clock_delay(20); // Wait for change to take effect
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, FIFO_CTRL_REG, 0x20 | 0x1F); // Enable accelerometer FIFO stream mode and set watermark at 32 samples
	mp_clock_delay(1000);


	samples = (mp_drv_LSM9DS0_xmReadByte(LSM9DS0, FIFO_SRC_REG) & 0x1F); // Read number of stored accelerometer samples
	for(ii = 0; ii < samples ; ii++) { // Read the accelerometer data stored in the FIFO
		unsigned short accel_temp[3] = {0, 0, 0};

		mp_drv_LSM9DS0_xmReadBytes(LSM9DS0, OUT_X_L_A, &data[0], 6);
		accel_temp[0] = (unsigned short) (((unsigned short)data[1] << 8) | data[0]);// Form signed 16-bit integer for each sample in FIFO
		accel_temp[1] = (unsigned short) (((unsigned short)data[3] << 8) | data[2]);
		accel_temp[2] = (unsigned short) (((unsigned short)data[5] << 8) | data[4]);
		accel_bias[0] += (unsigned long) accel_temp[0]; // Sum individual signed 16-bit biases to get accumulated signed 32-bit biases
		accel_bias[1] += (unsigned long) accel_temp[1];
		accel_bias[2] += (unsigned long) accel_temp[2];
	}

	accel_bias[0] /= samples; // average the data
	accel_bias[1] /= samples;
	accel_bias[2] /= samples;

	// Remove gravity from the z-axis accelerometer bias calculation
	if(accel_bias[2] > 0L)
		accel_bias[2] -= (unsigned long) (1.0/LSM9DS0->aRes);
	else
		accel_bias[2] += (unsigned long) (1.0/LSM9DS0->aRes);

	LSM9DS0->abias[0] = (float)accel_bias[0]*LSM9DS0->aRes; // Properly scale data to get gs
	LSM9DS0->abias[1] = (float)accel_bias[1]*LSM9DS0->aRes;
	LSM9DS0->abias[2] = (float)accel_bias[2]*LSM9DS0->aRes;

	mp_printk("LSM9DS0 accelerometer calibration bias are x=%f y=%f z=%f using %d samples",
			LSM9DS0->abias[0], LSM9DS0->abias[1], LSM9DS0->abias[2], samples);

	c = mp_drv_LSM9DS0_xmReadByte(LSM9DS0, CTRL_REG0_XM);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, CTRL_REG0_XM, c & ~0x40); // Disable accelerometer FIFO
	mp_clock_delay(20);
	mp_drv_LSM9DS0_xmWriteByte(LSM9DS0, FIFO_CTRL_REG, 0x00); // Enable accelerometer bypass mode

}


static _mp_drv_LSM9DS0_spi_writeByte(mp_drv_LSM9DS0_t *LSM9DS0, mp_gpio_port_t *cs, unsigned char subAddress, unsigned char data) {
	/* Initiate communication */
	if(cs)
		mp_gpio_unset(cs);

	/* send address */
	mp_spi_tx(&LSM9DS0->spi, subAddress & 0x3F);

	/* send data */
	mp_spi_tx(&LSM9DS0->spi, data);

	/* close communication */
	if(cs)
		mp_gpio_set(cs);
}


static unsigned char _mp_drv_LSM9DS0_spi_readByte(mp_drv_LSM9DS0_t *LSM9DS0, mp_gpio_port_t *cs, unsigned char subAddress) {
	unsigned char temp;
	// Use the multiple read function to read 1 byte.
	// Value is returned to `temp`.
	_mp_drv_LSM9DS0_spi_readBytes(LSM9DS0, cs, subAddress, &temp, 1);
	return temp;
}


static void _mp_drv_LSM9DS0_spi_readBytes(
		mp_drv_LSM9DS0_t *LSM9DS0, mp_gpio_port_t *cs,
		unsigned char subAddress, unsigned char * dest, unsigned char count
	) {
	int i;

	/* Initiate communication */
	if(cs)
		mp_gpio_unset(cs);

	// To indicate a read, set bit 0 (msb) to 1
	// If we're reading multiple bytes, set bit 1 to 1
	// The remaining six bytes are the address to be read
	if (count > 1)
		mp_spi_tx(&LSM9DS0->spi, 0xC0 | (subAddress & 0x3F));
	else
		mp_spi_tx(&LSM9DS0->spi, 0x80 | (subAddress & 0x3F));

	for(i=0; i<count; i++)
		dest[i] = mp_spi_rx(&LSM9DS0->spi);

	/* close communication */
	if(cs)
		mp_gpio_set(cs);

}


void mp_drv_LSM9DS0_gWriteByte(mp_drv_LSM9DS0_t *LSM9DS0, unsigned char subAddress, unsigned char data) {
	_mp_drv_LSM9DS0_spi_writeByte(LSM9DS0, LSM9DS0->csG, subAddress, data);
}

void mp_drv_LSM9DS0_xmWriteByte(mp_drv_LSM9DS0_t *LSM9DS0, unsigned char subAddress, unsigned char data) {
	_mp_drv_LSM9DS0_spi_writeByte(LSM9DS0, LSM9DS0->csXM, subAddress, data);
}

unsigned char mp_drv_LSM9DS0_gReadByte(mp_drv_LSM9DS0_t *LSM9DS0, unsigned char subAddress) {
	return _mp_drv_LSM9DS0_spi_readByte(LSM9DS0, LSM9DS0->csG, subAddress);
}

void mp_drv_LSM9DS0_gReadBytes(mp_drv_LSM9DS0_t *LSM9DS0, unsigned char subAddress, unsigned char * dest, unsigned char count) {
	_mp_drv_LSM9DS0_spi_readBytes(LSM9DS0, LSM9DS0->csG, subAddress, dest, count);
}

unsigned char mp_drv_LSM9DS0_xmReadByte(mp_drv_LSM9DS0_t *LSM9DS0, unsigned char subAddress) {
	return _mp_drv_LSM9DS0_spi_readByte(LSM9DS0, LSM9DS0->csXM, subAddress);
}

void mp_drv_LSM9DS0_xmReadBytes(mp_drv_LSM9DS0_t *LSM9DS0, unsigned char subAddress, unsigned char * dest, unsigned char count) {
	_mp_drv_LSM9DS0_spi_readBytes(LSM9DS0, LSM9DS0->csXM, subAddress, dest, count);
}


#endif

