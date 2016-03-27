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

		/* SPI context */
		mp_spi_t spi;

		/* reg master handker */
		mp_regMaster_t regMaster;

		/* DRDY GPIO */
		mp_gpio_port_t *drdy;

		/* reg master read controler */
		mp_regMaster_cb_t readerControl;

		/* internal register map */
		unsigned char registerMap[_ADS124X_REGCOUNT];
	};

	/**@}*/

	mp_ret_t mp_drv_ADS124X_init(mp_kernel_t *kernel, mp_drv_ADS124X_t *ADS124X, mp_options_t *options, char *who);
	void mp_drv_ADS124X_fini(mp_drv_ADS124X_t *ADS124X);

	/*! \name ADS1246/7/8 SPI command definitions
	 * @{
	 */
	#define ADS124X_SPI_WAKEUP   (0x00)
	#define ADS124X_SPI_SLEEP    (0x02)
	#define ADS124X_SPI_SYNC     (0x04)
	#define ADS124X_SPI_RESET    (0x06)
	#define ADS124X_SPI_NOP      (0xFF)
	#define ADS124X_SPI_RDATA    (0x12)
	#define ADS124X_SPI_RDATAC   (0x14)
	#define ADS124X_SPI_SDATAC   (0x16)
	#define ADS124X_SPI_RREG     (0x20)
	#define ADS124X_SPI_WREG     (0x40)
	#define ADS124X_SPI_SYSOCAL  (0x60)
	#define ADS124X_SPI_SYSGCAL  (0x61)
	#define ADS124X_SPI_SELFOCAL (0x62)
	/**@}*/


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
	#define ADS12478_REG_MUX0    (0x00)
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

	/*! \name ADS1247/8 Multiplexer Control Register 0
	 * @{
	 */
	#define ADS12478_REG_MUX0_BCS_OFF   (0x00) // Burnout current source off (default)
	#define ADS12478_REG_MUX0_BCS_ON_05 (0x40) // Burnout current source on, 0.5μA
	#define ADS12478_REG_MUX0_BCS_ON_2  (0x80) // Burnout current source on, 2μA
	#define ADS12478_REG_MUX0_BCS_ON_10 (0xC0) // Burnout current source on, 10μA

	/* positive input channel */
	#define ADS12478_REG_MUX0_POS_AIN0 (0x00)
	#define ADS12478_REG_MUX0_POS_AIN1 (0x08)
	#define ADS12478_REG_MUX0_POS_AIN2 (0x10)
	#define ADS12478_REG_MUX0_POS_AIN3 (0x18)
	#define ADS12478_REG_MUX0_POS_AIN4 (0x20)
	#define ADS12478_REG_MUX0_POS_AIN5 (0x28)
	#define ADS12478_REG_MUX0_POS_AIN6 (0x30)
	#define ADS12478_REG_MUX0_POS_AIN7 (0x38)

	/* negative input channel */
	#define ADS12478_REG_MUX0_NEG_AIN0 (0x00)
	#define ADS12478_REG_MUX0_NEG_AIN1 (0x01)
	#define ADS12478_REG_MUX0_NEG_AIN2 (0x02)
	#define ADS12478_REG_MUX0_NEG_AIN3 (0x03)
	#define ADS12478_REG_MUX0_NEG_AIN4 (0x04)
	#define ADS12478_REG_MUX0_NEG_AIN5 (0x05)
	#define ADS12478_REG_MUX0_NEG_AIN6 (0x06)
	#define ADS12478_REG_MUX0_NEG_AIN7 (0x07)
	/*! @} */


	/*! \name ADS1247/8 Multiplexer Control Register 1
	 * @{
	 */
	//  Internal reference is always off (default)
	#define ADS12478_REG_MUX1_VREFCON_OFF     (0x00)

	// Internal reference is always on
	#define ADS12478_REG_MUX1_VREFCON_ON      (0x20)

	// Internal reference is on when a conversion is in progress
	#define ADS12478_REG_MUX1_VREFCON_CONVON  (0x40)


	// REF0 input pair selected (default)
	#define ADS12478_REG_MUX1_REFSELT_REF0    (0x00)

	// REF1 input pair selected (ADS1248 only)
	#define ADS12478_REG_MUX1_REFSELT_REF1    (0x08)

	// Onboard reference selected
	#define ADS12478_REG_MUX1_REFSELT_ONB     (0x10)

	// Onboard reference selected and internally connected to REF0 input pair
	#define ADS12478_REG_MUX1_REFSELT_ONBREF0 (0x18)


	// Normal operation (default)
	#define ADS12478_REG_MUX1_MUXCAL_NORMAL   (0x00)

	// Offset measurement
	#define ADS12478_REG_MUX1_MUXCAL_OFFSET   (0x01)

	// Gain measurement
	#define ADS12478_REG_MUX1_MUXCAL_GAIN     (0x02)

	// Temperature diode
	#define ADS12478_REG_MUX1_MUXCAL_TEMP     (0x03)

	// External REF1 measurement (ADS1248 only)
	#define ADS12478_REG_MUX1_MUXCAL_REF1     (0x04)

	// External REF0 measurement
	#define ADS12478_REG_MUX1_MUXCAL_REF0     (0x05)

	// AVDD measurement
	#define ADS12478_REG_MUX1_MUXCAL_AVDD     (0x06)

	// DVDD measurement
	#define ADS12478_REG_MUX1_MUXCAL_DVDD     (0x07)

	/*! @} */

	/*! \name ADS1247/8 System Control Register 0
	 * @{
	 */
	#define ADS12478_REG_SYS0_PGA_1       (0x00)
	#define ADS12478_REG_SYS0_PGA_2       (0x20)
	#define ADS12478_REG_SYS0_PGA_4       (0x40)
	#define ADS12478_REG_SYS0_PGA_8       (0x60)
	#define ADS12478_REG_SYS0_PGA_16      (0x80)
	#define ADS12478_REG_SYS0_PGA_32      (0xA0)
	#define ADS12478_REG_SYS0_PGA_64      (0xC0)
	#define ADS12478_REG_SYS0_PGA_128     (0xE0)

	#define ADS12478_REG_SYS0_DOR_5SPS    (0x00)
	#define ADS12478_REG_SYS0_DOR_10SPS   (0x01)
	#define ADS12478_REG_SYS0_DOR_20SPS   (0x02)
	#define ADS12478_REG_SYS0_DOR_40SPS   (0x03)
	#define ADS12478_REG_SYS0_DOR_80SPS   (0x04)
	#define ADS12478_REG_SYS0_DOR_160SPS  (0x05)
	#define ADS12478_REG_SYS0_DOR_320SPS  (0x06)
	#define ADS12478_REG_SYS0_DOR_640SPS  (0x07)
	#define ADS12478_REG_SYS0_DOR_1000SPS (0x08)
	#define ADS12478_REG_SYS0_DOR_2000SPS (0x09)
	/*! @} */

	/*! \name ADS1247/8 IDAC Control Register 0
	 * @{
	 */
	// DOUT/DRDY pin functions both as Data Out and Data Ready, active low
	#define ADS12478_REG_IDAC0_DRDY       (0x08)

	// IMAG Off (default)
	#define ADS12478_REG_IDAC0_IMAG_OFF   (0x00)

	// IMAG 50μA
	#define ADS12478_REG_IDAC0_IMAG_50    (0x01)

	// IMAG 100μA
	#define ADS12478_REG_IDAC0_IMAG_100   (0x02)

	// IMAG 250μA
	#define ADS12478_REG_IDAC0_IMAG_250   (0x03)

	// IMAG 500μA
	#define ADS12478_REG_IDAC0_IMAG_500   (0x04)

	// IMAG 750μA
	#define ADS12478_REG_IDAC0_IMAG_750   (0x05)

	// IMAG 1000μA
	#define ADS12478_REG_IDAC0_IMAG_1000  (0x06)

	// IMAG 1500μA
	#define ADS12478_REG_IDAC0_IMAG_1500  (0x07)

	/*! @} */

	/*! \name ADS1247/8 IDAC Control Register 1
	 * @{
	 */
	#define ADS12478_REG_IDAC1_L1DIR_AIN0       (0x00)
	#define ADS12478_REG_IDAC1_L1DIR_AIN1       (0x10)
	#define ADS12478_REG_IDAC1_L1DIR_AIN2       (0x20)
	#define ADS12478_REG_IDAC1_L1DIR_AIN3       (0x30)
	#define ADS12478_REG_IDAC1_L1DIR_AIN4       (0x40)
	#define ADS12478_REG_IDAC1_L1DIR_AIN5       (0x50)
	#define ADS12478_REG_IDAC1_L1DIR_AIN6       (0x60)
	#define ADS12478_REG_IDAC1_L1DIR_AIN7       (0x70)
	#define ADS12478_REG_IDAC1_L1DIR_IEXT1      (0x80)
	#define ADS12478_REG_IDAC1_L1DIR_IEXT2      (0x90)
	#define ADS12478_REG_IDAC1_L1DIR_DISCO      (0xC0)

	#define ADS12478_REG_IDAC1_L2DIR_AIN0       (0x00)
	#define ADS12478_REG_IDAC1_L2DIR_AIN1       (0x01)
	#define ADS12478_REG_IDAC1_L2DIR_AIN2       (0x02)
	#define ADS12478_REG_IDAC1_L2DIR_AIN3       (0x03)
	#define ADS12478_REG_IDAC1_L2DIR_AIN4       (0x04)
	#define ADS12478_REG_IDAC1_L2DIR_AIN5       (0x05)
	#define ADS12478_REG_IDAC1_L2DIR_AIN6       (0x06)
	#define ADS12478_REG_IDAC1_L2DIR_AIN7       (0x07)
	#define ADS12478_REG_IDAC1_L2DIR_IEXT1      (0x08)
	#define ADS12478_REG_IDAC1_L2DIR_IEXT2      (0x09)
	#define ADS12478_REG_IDAC1_L2DIR_DISCO      (0x0C)
	/*! @} */

	/*! \name ADS124X Constants
	 * @{
	 */

	/*! @} */

#endif

#endif

