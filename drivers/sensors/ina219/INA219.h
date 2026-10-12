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


#ifdef SUPPORT_DRV_INA219

#ifndef _HAVE_MP_DRV_INA219_H
	#define _HAVE_MP_DRV_INA219_H

	/**
	 * @defgroup mpDriverTiINA219
	 * @{
	 */

	typedef struct mp_drv_INA219_s mp_drv_INA219_t;

	struct mp_drv_INA219_s {
		mp_kernel_t *kernel;
		mp_i2c_t i2c;

		mp_regMaster_t regMaster;

		unsigned short configuration;
		unsigned short calibrationVal;
		unsigned char currentDivider;
		unsigned char powerDivider;

		unsigned short rawBusVoltage;
		unsigned short rawShuntVoltage;
		unsigned short rawCurrent;

		mp_sensor_t *busVoltage;
		mp_sensor_t *shuntVoltage;
		mp_sensor_t *current;


		mp_task_t *task;
	};

	/*! \name INA219 basic registers
	 * @{
	 */

	/** Default address */
	#define INA219_ADDRESS                         (0x40)    // 1000000 (A0+A1=GND)
	#define INA219_READ                            (0x01)

	/*! @} */

	/*! \name INA219 configuration registers
	 * @{
	 */

	/** Configuration register */
	#define INA219_REG_CONFIG                      (0x00)

	/** Reset Bit */
	#define INA219_CONFIG_RESET                    (0x8000)
	/** Bus Voltage Range Mask */
	#define INA219_CONFIG_BVOLTAGERANGE_MASK       (0x2000)
	/** 0-16V Range */
	#define INA219_CONFIG_BVOLTAGERANGE_16V        (0x0000)
	/** 0-32V Range */
	#define INA219_CONFIG_BVOLTAGERANGE_32V        (0x2000)

	/** Gain Mask */
	#define INA219_CONFIG_GAIN_MASK                (0x1800)
	/** Gain 1, 40mV Range */
	#define INA219_CONFIG_GAIN_1_40MV              (0x0000)
	/** Gain 2, 80mV Range */
	#define INA219_CONFIG_GAIN_2_80MV              (0x0800)
	/** Gain 4, 160mV Range */
	#define INA219_CONFIG_GAIN_4_160MV             (0x1000)
	/** Gain 8, 320mV Range */
	#define INA219_CONFIG_GAIN_8_320MV             (0x1800)

	/** Bus ADC Resolution Mask */
	#define INA219_CONFIG_BADCRES_MASK             (0x0780)
	/** 9-bit bus res = 0..511 */
	#define INA219_CONFIG_BADCRES_9BIT             (0x0080)
	/** 10-bit bus res = 0..1023 */
	#define INA219_CONFIG_BADCRES_10BIT            (0x0100)
	/** 11-bit bus res = 0..2047 */
	#define INA219_CONFIG_BADCRES_11BIT            (0x0200)
	/** 12-bit bus res = 0..4097 */
	#define INA219_CONFIG_BADCRES_12BIT            (0x0400)

	/** Shunt ADC Resolution and Averaging Mask */
	#define INA219_CONFIG_SADCRES_MASK             (0x0078)
	/**  1 x 9-bit shunt sample */
	#define INA219_CONFIG_SADCRES_9BIT_1S_84US     (0x0000)
	/** 1 x 10-bit shunt sample */
	#define INA219_CONFIG_SADCRES_10BIT_1S_148US   (0x0008)
	/** 1 x 11-bit shunt sample */
	#define INA219_CONFIG_SADCRES_11BIT_1S_276US   (0x0010)
	/** 1 x 12-bit shunt sample */
	#define INA219_CONFIG_SADCRES_12BIT_1S_532US   (0x0018)
	/** 2 x 12-bit shunt samples averaged together */
	#define INA219_CONFIG_SADCRES_12BIT_2S_1060US  (0x0048)
	/** 4 x 12-bit shunt samples averaged together */
	#define INA219_CONFIG_SADCRES_12BIT_4S_2130US  (0x0050)
	/** 8 x 12-bit shunt samples averaged together */
	#define INA219_CONFIG_SADCRES_12BIT_8S_4260US  (0x0058)
	/** 16 x 12-bit shunt samples averaged together */
	#define INA219_CONFIG_SADCRES_12BIT_16S_8510US (0x0060)
	/** 32 x 12-bit shunt samples averaged together */
	#define INA219_CONFIG_SADCRES_12BIT_32S_17MS   (0x0068)
	/** 64 x 12-bit shunt samples averaged together */
	#define INA219_CONFIG_SADCRES_12BIT_64S_34MS   (0x0070)
	/** 128 x 12-bit shunt samples averaged together */
	#define INA219_CONFIG_SADCRES_12BIT_128S_69MS  (0x0078)

	/** Operating Mode Mask */
	#define INA219_CONFIG_MODE_MASK                (0x0007)
	#define INA219_CONFIG_MODE_POWERDOWN           (0x0000)
	#define INA219_CONFIG_MODE_SVOLT_TRIGGERED     (0x0001)
	#define INA219_CONFIG_MODE_BVOLT_TRIGGERED     (0x0002)
	#define INA219_CONFIG_MODE_SANDBVOLT_TRIGGERED (0x0003)
	#define INA219_CONFIG_MODE_ADCOFF              (0x0004)
	#define INA219_CONFIG_MODE_SVOLT_CONTINUOUS    (0x0005)
	#define INA219_CONFIG_MODE_BVOLT_CONTINUOUS    (0x0006)
	#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS (0x0007)

	/*! @} */

	/*! \name INA219 other registers
	 * @{
	 */

	/** Shunt voltage register */
	#define INA219_REG_SHUNTVOLTAGE                (0x01)
	/** Bus voltage register */
	#define INA219_REG_BUSVOLTAGE                  (0x02)
	/** Power register */
	#define INA219_REG_POWER                       (0x03)
	/** Current register */
	#define INA219_REG_CURRENT                     (0x04)
	/** Calibration register */
	#define INA219_REG_CALIBRATION                 (0x05)

	/*! @} */


	/** @} */

	mp_ret_t mp_drv_INA219_init(mp_kernel_t *kernel, mp_drv_INA219_t *INA219, mp_options_t *options, char *who);
	void mp_drv_INA219_fini(mp_drv_INA219_t *INA219);

	void mp_drv_INA219_setCalibration_32V_2A(mp_drv_INA219_t *INA219);
	void mp_drv_INA219_setCalibration_32V_1A(mp_drv_INA219_t *INA219);
	void mp_drv_INA219_setCalibration_16V_400mA(mp_drv_INA219_t *INA219);

	void mp_drv_INA219_update_busVoltage(mp_drv_INA219_t *INA219);
	void mp_drv_INA219_update_shuntVoltage(mp_drv_INA219_t *INA219);
	void mp_drv_INA219_update_current(mp_drv_INA219_t *INA219);
#endif

#endif
