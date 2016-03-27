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

#define SUPPORT_DRV_ADS124X

#ifdef SUPPORT_DRV_ADS124X

#ifndef _HAVE_MP_DRV_ADS124X_H
	#define _HAVE_MP_DRV_ADS124X_H

	#define _ADS124X_REGCOUNT 0x0f

	/**
	 * @defgroup mpDriverTiADS124X
	 * @{
	 */

	typedef struct mp_drv_ADS124X_s mp_drv_ADS124X_t;

	struct mp_drv_ADS124X_s {
		/** kernel handler */
		mp_kernel_t *kernel;

		mp_spi_t spi;

		/* reg master handker */
		mp_regMaster_t regMaster;

		mp_gpio_port_t *drdy;

		mp_regMaster_cb_t readerControl;

		unsigned char registerMap[_ADS124X_REGCOUNT];
	};

	/**@}*/

	mp_ret_t mp_drv_ADS124X_init(mp_kernel_t *kernel, mp_drv_ADS124X_t *ADS124X, mp_options_t *options, char *who);
	void mp_drv_ADS124X_fini(mp_drv_ADS124X_t *ADS124X);


	/*! \name ADS1246 register map
	 * @{
	 */
	#define ADS1246_REG_BCS   (0x00)
	#define ADS1246_REG_VBIAS (0x01)
	#define ADS1246_REG_MUX1  (0x02)
	#define ADS1246_REG_SYS0  (0x03)
	#define ADS1246_REG_OFC0  (0x04)
	#define ADS1246_REG_OFC1  (0x05)
	#define ADS1246_REG_OFC2  (0x06)
	#define ADS1246_REG_FSC0  (0x07)
	#define ADS1246_REG_FSC1  (0x08)
	#define ADS1246_REG_FSC2  (0x09)
	#define ADS1246_REG_ID    (0x0A)
	/*! @} */

	/*! \name ADS1246 Burnout Current Source Register
	 * @{
	 */
	#define ADS1246_REG_BCS_OFF   (0x00) // Burnout current source off (default)
	#define ADS1246_REG_BCS_ON_05 (0x40) // Burnout current source on, 0.5μA
	#define ADS1246_REG_BCS_ON_2  (0x80) // Burnout current source on, 2μA
	#define ADS1246_REG_BCS_ON_10 (0xC0) // Burnout current source on, 10μA
	/*! @} */

	/*! \name ADS1246 Multiplexer Control Register 1
	 * @{
	 */
	#define ADS1246_REG_MUX1_MUXCAL_NORMAL   (0x00) // Normal operation (default)
	#define ADS1246_REG_MUX1_MUXCAL_OFFSET   (0x01) // Offset calibration: inputs shorted to midsupply (AVDD + AVSS)/2
	#define ADS1246_REG_MUX1_MUXCAL_GAIN     (0x02) // Gain calibration: VREFP – VREFN (full-scale)
	#define ADS1246_REG_MUX1_MUXCAL_TEMP     (0x03) // Temperature measurement diode
	/*! @} */

	/*! \name ADS1246 System Control Register 0
	 * @{
	 */
	#define ADS1246_REG_SYS0_PGA_1   (0x00)
	#define ADS1246_REG_SYS0_PGA_2   (0x20)
	#define ADS1246_REG_SYS0_PGA_4   (0x40)
	#define ADS1246_REG_SYS0_PGA_8   (0x60)
	#define ADS1246_REG_SYS0_PGA_16  (0x80)
	#define ADS1246_REG_SYS0_PGA_32  (0xA0)
	#define ADS1246_REG_SYS0_PGA_64  (0xC0)
	#define ADS1246_REG_SYS0_PGA_128 (0xE0)

	#define ADS1246_REG_SYS0_DOR_5SPS    (0x00)
	#define ADS1246_REG_SYS0_DOR_10SPS   (0x01)
	#define ADS1246_REG_SYS0_DOR_20SPS   (0x02)
	#define ADS1246_REG_SYS0_DOR_40SPS   (0x03)
	#define ADS1246_REG_SYS0_DOR_80SPS   (0x04)
	#define ADS1246_REG_SYS0_DOR_160SPS  (0x05)
	#define ADS1246_REG_SYS0_DOR_320SPS  (0x06)
	#define ADS1246_REG_SYS0_DOR_640SPS  (0x07)
	#define ADS1246_REG_SYS0_DOR_1000SPS (0x08)
	#define ADS1246_REG_SYS0_DOR_2000SPS (0x09)
	/*! @} */

	/*! \name ADS1246 ID Register
	 * @{
	 */
	// DOUT/DRDY pin functions both as Data Out and Data Ready, active low
	#define ADS1246_REG_ID_DRDY   (0x08)
	/*! @} */

	/*! \name ADS1247/8 register map
	 * @{
	 */
	#define ADS12478_REG_BCS     (0x00)
	#define ADS12478_REG_VBIAS   (0x01)
	#define ADS12478_REG_MUX1    (0x02)
	#define ADS12478_REG_SYS0    (0x03)
	#define ADS12478_REG_OFC0    (0x04)
	#define ADS12478_REG_OFC1    (0x05)
	#define ADS12478_REG_OFC2    (0x06)
	#define ADS12478_REG_FSC0    (0x07)
	#define ADS12478_REG_FSC1    (0x08)
	#define ADS12478_REG_FSC2    (0x09)
	#define ADS12478_REG_IDAC0   (0x0A)
	#define ADS12478_REG_IDAC1   (0x0B)
	#define ADS12478_REG_GPIOCFG (0x0C)
	#define ADS12478_REG_GPIODIR (0x0D)
	#define ADS12478_REG_GPIODAT (0x0E)
	/*! @} */




	/*! \name ADS124X Constants
	 * @{
	 */

	/*! @} */

#endif

#endif
