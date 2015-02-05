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


#ifdef SUPPORT_DRV_TMP006

#ifndef _HAVE_MP_DRV_TMP006_H
	#define _HAVE_MP_DRV_TMP006_H

	typedef struct mp_drv_TMP006_s mp_drv_TMP006_t;

	struct mp_drv_TMP006_s {
		mp_kernel_t *kernel;
		mp_i2c_t i2c;
		mp_gpio_port_t *drdy;

		mp_regMaster_t regMaster;

		mp_task_t *task;

		mp_sensor_t *sensor;

		unsigned short deviceId;
		unsigned short manufacturerId;

		unsigned short settings;

	};

	mp_sensor_t *mp_drv_TMP006_init(mp_kernel_t *kernel, mp_drv_TMP006_t *TMP006, mp_options_t *options, char *who);
	void mp_drv_TMP006_fini(mp_drv_TMP006_t *TMP006);
	void mp_drv_TMP006_sleep(mp_drv_TMP006_t *TMP006);
	void mp_drv_TMP006_wakeUp(mp_drv_TMP006_t *TMP006);

	/*! \name TMP006 Internal Pointer Register Address
	 * @{
	 */
	/*! TMP006 object voltage register pointer */
	#define TMP006_REG_VOBJ       0x00

	/*! TMP006 ambient temperature register pointer */
	#define TMP006_REG_TABT       0x01

	/*! TMP006 configuration register pointer */
	#define TMP006_REG_WRITE_REG  0x02

	/*! TMP006 manufacturer ID register pointer */
	#define TMP006_REG_MAN_ID     0xFE

	/*! TMP006 device ID register pointer */
	#define TMP006_REG_DEVICE_ID  0xFF

	/*! @} */

	/*! \name TMP006 Configuration Register Bits
	 * @{
	 */
	#define TMP006_CFG_RESET    0x8000
	#define TMP006_CFG_MODEON   0x7000
	#define TMP006_CFG_1SAMPLE  0x0000
	#define TMP006_CFG_2SAMPLE  0x0200
	#define TMP006_CFG_4SAMPLE  0x0400
	#define TMP006_CFG_8SAMPLE  0x0600
	#define TMP006_CFG_16SAMPLE 0x0800
	#define TMP006_CFG_DRDYEN   0x0100
	#define TMP006_CFG_DRDY     0x0080

	/*! @} */

	/*! \name TMP006 Constants
	 * @{
	 */
	#define TMP006_B0 -0.0000294
	#define TMP006_B1 -0.00000057
	#define TMP006_B2 0.00000000463
	#define TMP006_C2 13.4
	#define TMP006_TREF 298.15
	#define TMP006_A2 -0.00001678
	#define TMP006_A1 0.00175
	#define TMP006_S0 6.4  // * 10^-14
	/*! @} */

#endif

#endif
