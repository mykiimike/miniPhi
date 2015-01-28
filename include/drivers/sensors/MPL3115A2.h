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


#ifdef SUPPORT_DRV_MPL3115A2

#ifndef _HAVE_MP_DRV_MPL3115A2_H
	#define _HAVE_MP_DRV_MPL3115A2_H

	typedef struct mp_drv_MPL3115A2_s mp_drv_MPL3115A2_t;

	struct mp_drv_MPL3115A2_s {
		mp_kernel_t *kernel;

		mp_i2c_t i2c;
		mp_regMaster_t regMaster;

		mp_gpio_port_t *drdy;

		mp_task_t *task;

		mp_sensor_t *sensor;

		unsigned char settings;

		char whoIam;
	};

	mp_sensor_t *mp_drv_MPL3115A2_init(mp_kernel_t *kernel, mp_drv_MPL3115A2_t *MPL3115A2, mp_options_t *options, char *who);
	void mp_drv_MPL3115A2_fini(mp_drv_MPL3115A2_t *MPL3115A2);
	void mp_drv_MPL3115A2_sleep(mp_drv_MPL3115A2_t *MPL3115A2);
	void mp_drv_MPL3115A2_wakeUp(mp_drv_MPL3115A2_t *MPL3115A2);
	void mp_drv_MPL3115A2_reset(mp_drv_MPL3115A2_t *MPL3115A2);
	void mp_drv_MPL3115A2_setModeAltimeter(mp_drv_MPL3115A2_t *MPL3115A2);
	void mp_drv_MPL3115A2_setModeBarometer(mp_drv_MPL3115A2_t *MPL3115A2);

	#define MPL3115A2_ADDRESS 0x60

	/*! \name MPL3115A2 Configuration Register Bits
	 * @{
	 */
	#define MPL3115A2_STATUS           0x00
	#define MPL3115A2_OUT_P_MSB        0x01
	#define MPL3115A2_OUT_P_CSB        0x02
	#define MPL3115A2_OUT_P_LSB        0x03
	#define MPL3115A2_OUT_T_MSB        0x04
	#define MPL3115A2_OUT_T_LSB        0x05
	#define MPL3115A2_DR_STATUS        0x06
	#define MPL3115A2_OUT_P_DELTA_MSB  0x07
	#define MPL3115A2_OUT_P_DELTA_CSB  0x08
	#define MPL3115A2_OUT_P_DELTA_LSB  0x09
	#define MPL3115A2_OUT_T_DELTA_MSB  0x0A
	#define MPL3115A2_OUT_T_DELTA_LSB  0x0B
	#define MPL3115A2_WHO_AM_I         0x0C
	#define MPL3115A2_F_STATUS         0x0D
	#define MPL3115A2_F_DATA           0x0E
	#define MPL3115A2_F_SETUP          0x0F
	#define MPL3115A2_TIME_DLY         0x10
	#define MPL3115A2_SYSMOD           0x11
	#define MPL3115A2_INT_SOURCE       0x12
	#define MPL3115A2_PT_DATA_CFG      0x13
	#define MPL3115A2_BAR_IN_MSB       0x14
	#define MPL3115A2_BAR_IN_LSB       0x15
	#define MPL3115A2_P_TGT_MSB        0x16
	#define MPL3115A2_P_TGT_LSB        0x17
	#define MPL3115A2_T_TGT            0x18
	#define MPL3115A2_P_WND_MSB        0x19
	#define MPL3115A2_P_WND_LSB        0x1A
	#define MPL3115A2_T_WND            0x1B
	#define MPL3115A2_P_MIN_MSB        0x1C
	#define MPL3115A2_P_MIN_CSB        0x1D
	#define MPL3115A2_P_MIN_LSB        0x1E
	#define MPL3115A2_T_MIN_MSB        0x1F
	#define MPL3115A2_T_MIN_LSB        0x20
	#define MPL3115A2_P_MAX_MSB        0x21
	#define MPL3115A2_P_MAX_CSB        0x22
	#define MPL3115A2_P_MAX_LSB        0x23
	#define MPL3115A2_T_MAX_MSB        0x24
	#define MPL3115A2_T_MAX_LSB        0x25
	#define MPL3115A2_CTRL_REG1        0x26
	#define MPL3115A2_CTRL_REG2        0x27
	#define MPL3115A2_CTRL_REG3        0x28
	#define MPL3115A2_CTRL_REG4        0x29
	#define MPL3115A2_CTRL_REG5        0x2A
	#define MPL3115A2_OFF_P            0x2B
	#define MPL3115A2_OFF_T            0x2C
	#define MPL3115A2_OFF_H            0x2D

	/*! @} */

	/*! \name MPL3115A2 Constants
	 * @{
	 */

	/*! @} */

#endif

#endif
