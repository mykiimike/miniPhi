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

#ifdef SUPPORT_DRV_LSM9DS0

#ifndef _HAVE_MP_DRV_LSM9DS0_H
	#define _HAVE_MP_DRV_LSM9DS0_H

	#define MP_DRV_LSM9DS0_MODE_I2C 1
	#define MP_DRV_LSM9DS0_MODE_SPI 2

	#define LSM9DS0_ADDRESS_ACCELMAG           (0x1D)         // 3B >> 1 = 7bit default
	#define LSM9DS0_ADDRESS_GYRO               (0x6B)         // D6 >> 1 = 7bit default

	/**
	 * @defgroup mpDriverSTLSM9DS0
	 * @{
	 */

	#define LSM9DS0_GYRO_CALIBRATION_COUNT 10
	#define LSM9DS0_GYRO_CALIBRATION_DROP 5

	#define LSM9DS0_ACCELERO_CALIBRATION_COUNT 10
	#define LSM9DS0_ACCELERO_CALIBRATION_DROP 5

	typedef struct mp_drv_LSM9DS0_s mp_drv_LSM9DS0_t;

	typedef void (*mp_drv_LSM9DS0_onData_t)(mp_drv_LSM9DS0_t *LSM9DS0);

	/* gyro_scale defines the possible full-scale ranges of the gyroscope */
	typedef enum mp_drv_LSM9DS0_gyro_scale_s {
		G_SCALE_245DPS, // 00: 245 degrees per second
		G_SCALE_500DPS, // 01: 500 dps
		G_SCALE_2000DPS, // 10: 2000 dps
	} mp_drv_LSM9DS0_gyro_scale_t;

	/* accel_scale defines all possible FSR's of the accelerometer: */
	typedef enum mp_drv_LSM9DS0_accel_scale_s {
		A_SCALE_2G, // 000: 2g
		A_SCALE_4G, // 001: 4g
		A_SCALE_6G, // 010: 6g
		A_SCALE_8G, // 011: 8g
		A_SCALE_16G // 100: 16g
	} mp_drv_LSM9DS0_accel_scale_t;

	/* mag_scale defines all possible FSR's of the magnetometer */
	typedef enum mp_drv_LSM9DS0_mag_scale_s {
		M_SCALE_2GS, // 00: 2Gs
		M_SCALE_4GS, // 01: 4Gs
		M_SCALE_8GS, // 10: 8Gs
		M_SCALE_12GS, // 11: 12Gs
	} mp_drv_LSM9DS0_mag_scale_t;

	/* gyro_odr defines all possible data rate/bandwidth combos of the gyro */
	typedef enum mp_drv_LSM9DS0_gyro_odr_s {
		G_ODR_95_BW_125 = 0x0, // 95 12.5
		G_ODR_95_BW_25 = 0x1, // 95 25
		// 0x2 and 0x3 define the same data rate and bandwidth
		G_ODR_190_BW_125 = 0x4, // 190 12.5
		G_ODR_190_BW_25 = 0x5, // 190 25
		G_ODR_190_BW_50 = 0x6, // 190 50
		G_ODR_190_BW_70 = 0x7, // 190 70
		G_ODR_380_BW_20 = 0x8, // 380 20
		G_ODR_380_BW_25 = 0x9, // 380 25
		G_ODR_380_BW_50 = 0xA, // 380 50
		G_ODR_380_BW_100 = 0xB, // 380 100
		G_ODR_760_BW_30 = 0xC, // 760 30
		G_ODR_760_BW_35 = 0xD, // 760 35
		G_ODR_760_BW_50 = 0xE, // 760 50
		G_ODR_760_BW_100 = 0xF, // 760 100
	} mp_drv_LSM9DS0_gyro_odr_t;

	/* accel_oder defines all possible output data rates of the accelerometer */
	typedef enum mp_drv_LSM9DS0_accel_odr_s {
		A_POWER_DOWN, // Power-down mode (0x0)
		A_ODR_3125, // 3.125 Hz (0x1)
		A_ODR_625, // 6.25 Hz (0x2)
		A_ODR_125, // 12.5 Hz (0x3)
		A_ODR_25, // 25 Hz (0x4)
		A_ODR_50, // 50 Hz (0x5)
		A_ODR_100, // 100 Hz (0x6)
		A_ODR_200, // 200 Hz (0x7)
		A_ODR_400, // 400 Hz (0x8)
		A_ODR_800, // 800 Hz (9)
		A_ODR_1600 // 1600 Hz (0xA)
	} mp_drv_LSM9DS0_accel_odr_t;

	/* accel_abw defines all possible anti-aliasing filter rates of the accelerometer */
	typedef enum accel_abw_s {
		A_ABW_773, // 773 Hz (0x0)
		A_ABW_194, // 194 Hz (0x1)
		A_ABW_362, // 362 Hz (0x2)
		A_ABW_50, // 50 Hz (0x3)
	} mp_drv_LSM9DS0_accel_abw_t;

	/* mag_oder defines all possible output data rates of the magnetometer */
	typedef enum mp_drv_LSM9DS0_mag_odr_s {
		M_ODR_3125, // 3.125 Hz (0x00)
		M_ODR_625, // 6.25 Hz (0x01)
		M_ODR_125, // 12.5 Hz (0x02)
		M_ODR_25, // 25 Hz (0x03)
		M_ODR_50, // 50 (0x04)
		M_ODR_100, // 100 Hz (0x05)
	} mp_drv_LSM9DS0_mag_odr_t;


	struct mp_drv_LSM9DS0_s {
		unsigned char init;

		mp_kernel_t *kernel;

		/**
		 * Internal interrupt source
		 * - 0x1 : Gyro pending
		 * - 0x2 : Magneto pending
		 * - 0x4 : Accelero pending
		 * */
		unsigned char intSrc:4;

		char protocol:2;
		char drdyCount:2;
		char gyroCal:2;
		char accelCal:2;

		mp_drv_LSM9DS0_gyro_scale_t gyro_scale;
		mp_drv_LSM9DS0_accel_scale_t accel_scale;
		mp_drv_LSM9DS0_mag_scale_t mag_scale;

		mp_gpio_port_t *csG;
		mp_gpio_port_t *csXM;
		mp_gpio_port_t *int1;
		mp_gpio_port_t *int2;
		mp_gpio_port_t *intG;
		mp_gpio_port_t *drdy;

		mp_task_t *task;

		union {
			mp_i2c_t i2c;
			mp_spi_t spi;
		};

		mp_regMaster_t regMaster;

		unsigned char buffer[6];

		// gRes, aRes, and mRes store the current resolution for each sensor.
		// Units of these values would be DPS (or g's or Gs's) per ADC tick.
		// This value is calculated as (sensor scale) / (2^15).
		float gRes, aRes, mRes;

		union {
			/** use to store tmp data. the last int is use to control calibration counter */
			signed long abiasCal[4];

			/** final result of the bias */
			float abias[3];
		};

		union {
			/** use to store tmp data. the last int is use to control calibration counter */
			signed long gbiasCal[4];

			/** final result of the bias */
			float gbias[3];
		};

		mp_sensor_t *gyro;
		mp_sensor_t *magneto;
		mp_sensor_t *temperature;
		mp_sensor_t *accelero;

		/** User callback: Gyro on data */
		mp_drv_LSM9DS0_onData_t onGyroData;

		/** User callback: Accelerometer on data */
		mp_drv_LSM9DS0_onData_t onAccelData;

		/** User callback: Magneto on data */
		mp_drv_LSM9DS0_onData_t onMagData;

		/** User callback: Temperature on data */
		mp_drv_LSM9DS0_onData_t onTempData;

		/** User pointer */
		void *onUser;

		/** used to configure gyro register w/o reads */
		unsigned char gReg1, gReg4;

		/** used to configure accelero/magneto register w/o reads */
		unsigned char xmReg1, xmReg2, xmReg5, xmReg6;

	};

	/** @} */

	mp_ret_t mp_drv_LSM9DS0_init(mp_kernel_t *kernel, mp_drv_LSM9DS0_t *LSM9DS0, mp_options_t *options, char *who);
	mp_ret_t mp_drv_LSM9DS0_fini(mp_drv_LSM9DS0_t *LSM9DS0);

	void mp_drv_LSM9DS0_xmRead(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char *wait, int waitSize,
		mp_regMaster_cb_t callback
	);
	void mp_drv_LSM9DS0_xmWrite(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char value
	);

	void mp_drv_LSM9DS0_gRead(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char *wait, int waitSize,
		mp_regMaster_cb_t callback
	);
	void mp_drv_LSM9DS0_gWrite(
		mp_drv_LSM9DS0_t *LSM9DS0,
		unsigned char reg,
		unsigned char value
	);

	void mp_drv_LSM9DS0_initGyro(mp_drv_LSM9DS0_t *LSM9DS0);
	void mp_drv_LSM9DS0_initAccel(mp_drv_LSM9DS0_t *LSM9DS0);
	void mp_drv_LSM9DS0_initMag(mp_drv_LSM9DS0_t *LSM9DS0);

	void mp_drv_LSM9DS0_finiGyro(mp_drv_LSM9DS0_t *LSM9DS0);
	void mp_drv_LSM9DS0_finiAccel(mp_drv_LSM9DS0_t *LSM9DS0);
	void mp_drv_LSM9DS0_finiMag(mp_drv_LSM9DS0_t *LSM9DS0);

	void mp_drv_LSM9DS0_setGyroScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_gyro_scale_t gScl);
	void mp_drv_LSM9DS0_setAccelScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_scale_t aScl, mp_bool_t calibrate);
	void mp_drv_LSM9DS0_setMagScale(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_mag_scale_t mScl);
	void mp_drv_LSM9DS0_setGyroODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_gyro_odr_t gRate);
	void mp_drv_LSM9DS0_setAccelODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_odr_t aRate);
	void mp_drv_LSM9DS0_setAccelABW(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_accel_abw_t abwRate);
	void mp_drv_LSM9DS0_setMagODR(mp_drv_LSM9DS0_t *LSM9DS0, mp_drv_LSM9DS0_mag_odr_t mRate);

	float mp_drv_LSM9DS0_calcGyro(mp_drv_LSM9DS0_t *LSM9DS0, unsigned short gyro);
	float mp_drv_LSM9DS0_calcAccel(mp_drv_LSM9DS0_t *LSM9DS0, unsigned short accel);
	float mp_drv_LSM9DS0_calcMag(mp_drv_LSM9DS0_t *LSM9DS0, unsigned short mag);

	/* LSM9DS0 Gyro Registers */
	#define WHO_AM_I_G		0x0F
	#define CTRL_REG1_G		0x20
	#define CTRL_REG2_G		0x21
	#define CTRL_REG3_G		0x22
	#define CTRL_REG4_G		0x23
	#define CTRL_REG5_G		0x24
	#define REFERENCE_G		0x25
	#define STATUS_REG_G	0x27
	#define OUT_X_L_G		0x28
	#define OUT_X_H_G		0x29
	#define OUT_Y_L_G		0x2A
	#define OUT_Y_H_G		0x2B
	#define OUT_Z_L_G		0x2C
	#define OUT_Z_H_G		0x2D
	#define FIFO_CTRL_REG_G	0x2E
	#define FIFO_SRC_REG_G	0x2F
	#define INT1_CFG_G		0x30
	#define INT1_SRC_G		0x31
	#define INT1_THS_XH_G	0x32
	#define INT1_THS_XL_G	0x33
	#define INT1_THS_YH_G	0x34
	#define INT1_THS_YL_G	0x35
	#define INT1_THS_ZH_G	0x36
	#define INT1_THS_ZL_G	0x37
	#define INT1_DURATION_G	0x38

	/* LSM9DS0 Accel/Magneto (XM) Registers */
	#define OUT_TEMP_L_XM	0x05
	#define OUT_TEMP_H_XM	0x06
	#define STATUS_REG_M	0x07
	#define OUT_X_L_M		0x08
	#define OUT_X_H_M		0x09
	#define OUT_Y_L_M		0x0A
	#define OUT_Y_H_M		0x0B
	#define OUT_Z_L_M		0x0C
	#define OUT_Z_H_M		0x0D
	#define WHO_AM_I_XM		0x0F
	#define INT_CTRL_REG_M	0x12
	#define INT_SRC_REG_M	0x13
	#define INT_THS_L_M		0x14
	#define INT_THS_H_M		0x15
	#define OFFSET_X_L_M	0x16
	#define OFFSET_X_H_M	0x17
	#define OFFSET_Y_L_M	0x18
	#define OFFSET_Y_H_M	0x19
	#define OFFSET_Z_L_M	0x1A
	#define OFFSET_Z_H_M	0x1B
	#define REFERENCE_X		0x1C
	#define REFERENCE_Y		0x1D
	#define REFERENCE_Z		0x1E
	#define CTRL_REG0_XM	0x1F
	#define CTRL_REG1_XM	0x20
	#define CTRL_REG2_XM	0x21
	#define CTRL_REG3_XM	0x22
	#define CTRL_REG4_XM	0x23
	#define CTRL_REG5_XM	0x24
	#define CTRL_REG6_XM	0x25
	#define CTRL_REG7_XM	0x26
	#define STATUS_REG_A	0x27
	#define OUT_X_L_A		0x28
	#define OUT_X_H_A		0x29
	#define OUT_Y_L_A		0x2A
	#define OUT_Y_H_A		0x2B
	#define OUT_Z_L_A		0x2C
	#define OUT_Z_H_A		0x2D
	#define FIFO_CTRL_REG	0x2E
	#define FIFO_SRC_REG	0x2F
	#define INT_GEN_1_REG	0x30
	#define INT_GEN_1_SRC	0x31
	#define INT_GEN_1_THS	0x32
	#define INT_GEN_1_DURATION	0x33
	#define INT_GEN_2_REG	0x34
	#define INT_GEN_2_SRC	0x35
	#define INT_GEN_2_THS	0x36
	#define INT_GEN_2_DURATION	0x37
	#define CLICK_CFG		0x38
	#define CLICK_SRC		0x39
	#define CLICK_THS		0x3A
	#define TIME_LIMIT		0x3B
	#define TIME_LATENCY	0x3C
	#define TIME_WINDOW		0x3D
	#define ACT_THS			0x3E
	#define ACT_DUR			0x3F

#endif

#endif
