/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
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

#ifndef _HAVE_MP_DRV_LEPTON_H
	#define _HAVE_MP_DRV_LEPTON_H

	/**
	 * FLIR Lepton 2 on the v1.3 breakout.
	 * VoSPI on SPI mode 3 fills one 80x60 frame of bytes; the face is that frame.
	 * On the STM32F1 each packet is moved by DMA1 (SPI2 RX channel 4, TX channel 5)
	 * and chip select is raised in that interrupt, so the CPU can draw meanwhile.
	 * CCI on I2C reads the FLIR serial, the part number and the software version
	 * once the module has booted, then publishes the FPA temperature in °C.
	 * The frame lives in the device, not on the heap.
	 *
	 * options: spiGate, simo, somi, sck, cs, i2cGate, sda, scl, address (0x2A).
	 */

	#define MP_DRV_LEPTON_WIDTH 80
	#define MP_DRV_LEPTON_HEIGHT 60

	typedef struct mp_drv_lepton_s mp_drv_lepton_t;

	struct mp_drv_lepton_s {
		mp_kernel_t *kernel;
		mp_spi_t spi;
		mp_i2c_t i2c;
		mp_regMaster_t regMaster;
		mp_gpio_port_t *cs;
		mp_task_t *task;
		mp_sensor_t *temperature;

		mp_camera_frame_t frame;
		unsigned char pixels[MP_DRV_LEPTON_WIDTH * MP_DRV_LEPTON_HEIGHT];
		unsigned char packet[164];

		unsigned char address;
		/** boot poll, then video */
		unsigned char phase;
		unsigned char tries;
		mp_bool_t booted;
		mp_bool_t gaveUp;

		/** one CCI transaction at a time */
		unsigned char step;
		unsigned char op;
		unsigned char poll;
		mp_bool_t busy;
		unsigned char cciReg[2];
		unsigned char cciTx[4];
		unsigned char cciRx[32];
		/** command in flight, and how many 16-bit words it returns */
		unsigned short cciCmd;
		unsigned char cciWords;
		/** 0 serial, 1 part number, 2 software version, then temperature */
		unsigned char ident;
		char model[33];
		char serial[17];
		char version[40];
		unsigned long tempAt;
		/** raw ends of the frame being read, and the stretch used to store it */
		unsigned short seenMin;
		unsigned short seenMax;
		unsigned short bias;
		unsigned int scale;

		/** VoSPI DMA: -1 seeks line 0, otherwise the next row. 0 idle, 1 frame, 2 lost */
		short dmaRow;
		unsigned short dmaSkips;
		mp_bool_t dmaOn;
		signed char dmaResult;
	};

	mp_ret_t mp_drv_lepton_init(mp_kernel_t *kernel, mp_drv_lepton_t *lep, mp_options_t *options, char *who);
	void mp_drv_lepton_fini(mp_drv_lepton_t *lep);

#ifdef SUPPORT_COMMON_DEVICE
	/** an MP_DEVICE_CAMERA */
	extern const mp_device_driver_t mp_drv_lepton_device;
#endif

#endif
